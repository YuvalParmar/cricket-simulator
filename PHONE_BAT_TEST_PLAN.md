# Phone Bat Testing Plan - Latency & Accuracy Validation

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        PHONE (100Hz)                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ Accelerometer│  │  Gyroscope   │  │ Magnetometer │  │  Sensor Fusion       │   │
│  │   100Hz      │  │   100Hz      │  │   100Hz      │  │  (Game Rotation)     │   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘   │
│         │                 │                 │                      │               │
│         └─────────────────┼─────────────────┼──────────────────────┘               │
│                           ▼                                                 │
│                  ┌──────────────────┐                                     │
│                  │ Swing Detector   │                                     │
│                  │ (ML/Threshold)   │                                     │
│                  └────────┬─────────┘                                     │
│                           │                                               │
│                           ▼                                               │
│                  ┌──────────────────┐                                     │
│                  │ Network Manager  │                                     │
│                  │ (JSON/UDP)       │                                     │
│                  └────────┬─────────┘                                     │
└───────────────────────────┼───────────────────────────────────────────────┘
                            │  UDP Packet (~500-1500 bytes)
                            ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        PC (Unreal Engine)                                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ UDP Receive  │  │  Jitter      │  │ Interpolate/ │  │ Latency Compensation │   │
│  │ Thread       │──▶│  Buffer      │──▶│  Predict     │──▶│ (Extrapolation)      │   │
│  │ (1ms sleep)  │  │  (4 frames)  │  │              │  │                      │   │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────┬───────────┘   │
│                                                                    │               │
│                           ┌───────────────────────────────────────┘               │
│                           ▼                                                     │
│                  ┌──────────────────┐                                           │
│                  │ Calibration      │                                           │
│                  │ Transform        │                                           │
│                  └────────┬─────────┘                                           │
│                           │                                                     │
│                           ▼                                                     │
│                  ┌──────────────────┐  ┌──────────────────┐                     │
│                  │ Bat Physics      │  │ Contact          │                     │
│                  │ State Update     │──▶│ Detection        │                     │
│                  └──────────────────┘  └──────────────────┘                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

## Latency Budget Analysis (Target: <20ms total for 94mph/42m/s ball)

| Stage | Estimated Latency | Notes |
|-------|-------------------|-------|
| Phone sensor sampling | 10ms (100Hz period) | Fixed by hardware |
| Sensor fusion processing | 2-5ms | ARCore/CoreMotion |
| Swing detection (on-device) | 5-10ms | ML inference |
| JSON serialization | 1-2ms | Kotlin/Swift serialization |
| UDP transmission (WiFi) | 2-10ms | Local network, ~1-5ms typical |
| PC receive thread (1ms poll) | 0.5-1.5ms | Average wait |
| Jitter buffer (4 frames @ 60Hz) | **66.7ms** | **MAJOR BOTTLENECK** |
| Interpolation | <1ms | Negligible |
| Latency extrapolation | <1ms | Negligible |
| Calibration transform | <1ms | Negligible |
| Contact detection (UE tick) | 16.7ms (60Hz) | Frame-aligned |
| **Total (current)** | **~100-130ms** | **TOO HIGH** |
| **Target** | **<20ms** | Need 5-6x reduction |

## Critical Issues Identified

### 1. Jitter Buffer Too Large (66.7ms at 60Hz)
**File:** `PhoneBatReceiverComponent.h:254`
```cpp
static const int32 JITTER_BUFFER_SIZE = 4; // Frames = 66.7ms at 60Hz
```

**Fix:** Reduce to 1-2 frames, or use adaptive jitter buffer based on measured network jitter.

### 2. Receive Thread Polls at 1ms Intervals
**File:** `PhoneBatReceiverComponent.cpp:640`
```cpp
FPlatformProcess::Sleep(0.001f); // 1ms sleep
```
This adds up to 1ms average latency. Consider event-driven (select/poll) or reduce to 0.1ms.

### 3. JSON Over UDP (Large Packets)
**File:** `NetworkManager.kt:587-588`
```kotlin
val fullJson = json.encodeToString(fullPacket)
val fullBytes = fullJson.toByteArray(Charsets.UTF_8)
```
JSON adds ~2-3x overhead vs binary. For sensor frames at 100Hz, this is significant.

### 4. Clock Sync Only Every 5 Seconds
**File:** `NetworkManager.kt:219`
```kotlin
private val clockSyncIntervalMs = 5000L
```
Drift can accumulate. Should sync more frequently during active swing.

### 5. Frame-Aligned Contact Detection
**File:** `CricketBattingComponent.cpp:34`
```cpp
PrimaryComponentTick.TickInterval = 0.0f; // Every frame for physics
```
At 60fps = 16.7ms granularity. For 94mph ball (42m/s), ball travels **70cm per frame** - need sub-frame prediction.

## Accuracy Concerns (Edge Detection)

### Calibration System
**File:** `PhoneBatReceiverComponent.cpp:535-572`
```cpp
void FinalizeCalibration() {
    // Averages transforms over 1 second (60 samples)
    // Uses simplified assumption: "phone at grip, bat extends along -Y"
}
```
- Only 60 samples (1 second) for calibration
- Assumes fixed grip offset (15cm) and rotation (-90°)
- No per-player customization of bat dimensions
- Validation only checks orientation/position error, not dynamic accuracy during swing

### Contact Detection Thresholds
**File:** `CricketBattingComponent.cpp:380-396`
```cpp
float ContactThreshold = BallRadius + 5.0f; // 5cm bat thickness
if (FMath::Abs(DistAlongNormal) < ContactThreshold && DistPerpendicular < SweetSpotRadius + BallRadius)
```
- 5cm bat thickness + 3.6cm ball = 8.6cm contact zone
- Sweet spot radius 4cm
- Edge detection: `DistPerpendicular > SweetSpotRadius * 0.8f` (3.2cm from center)
- **Problem:** 2cm error in bat position = edge vs sweet spot misclassification

### Phone-to-Bat Transform
**File:** `PhoneBatInput.cpp:474-488`
```cpp
FTransform CalculateWorldBatTransform(const FTransform& PhoneTransform) const {
    FTransform BatTransform = CurrentState.CalibrationTransform * PhoneTransform;
}
```
Single static transform - doesn't account for:
- Wrist rotation during swing
- Grip pressure changes
- Phone shifting in hand

## Test Plan

### Phase 1: Baseline Latency Measurement ✅ COMPLETED

```bash
# 1. Measure network RTT
ping <PC_IP>

# 2. Instrument the pipeline with timestamps ✅ DONE
# Phone: Add timestamp at sensor read, after fusion, before send ✅ DONE
# PC: Add timestamp at receive, after jitter buffer, after prediction ✅ DONE
# Log all timestamps to file for analysis
```

**Instrumentation Points:**
| Point | Location | Metric | Status |
|-------|----------|--------|--------|
| T0 | Phone: SensorEventListener.onSensorChanged() | Sensor read time | ✅ iOS/Android |
| T1 | Phone: After sensor fusion | Fusion complete | ✅ iOS/Android |
| T2 | Phone: Before UDP send | Serialization done | ✅ iOS/Android |
| T3 | PC: Receive thread | Packet received | ✅ PhoneBatReceiverComponent.cpp |
| T4 | PC: After jitter buffer | Interpolation done | ✅ PhoneBatReceiverComponent.cpp |
| T5 | PC: After prediction | Extrapolation done | ✅ PhoneBatReceiverComponent.cpp |
| T6 | PC: Game thread tick | Bat state updated | ✅ PhoneBatReceiverComponent.cpp |
| T7 | PC: Contact detection | Contact checked | ✅ CricketBattingComponent.cpp |

### Phase 2: Network Optimization Tests ✅ MOSTLY COMPLETED

| Test | Configuration | Expected Improvement | Status |
|------|---------------|---------------------|--------|
| Reduce jitter buffer | `JITTER_BUFFER_SIZE = 1` (adaptive) | -50ms | ✅ DONE |
| Binary protocol (Protocol Buffers) | Replace JSON | -2-5ms + smaller packets | ✅ DONE (PhoneBatBinaryProtocol) |
| Event-driven receive | `select()` instead of sleep | -0.5ms | ✅ DONE (WaitForSocket) |
| Increase clock sync frequency | Every 100ms during swing | Better compensation | ✅ DONE (adaptive) |
| WiFi 5GHz vs 2.4GHz | Channel selection | -2-5ms jitter | 🔄 Test pending |
| Bluetooth LE | Alternative transport | Compare latency | 🔄 Test pending |

### Phase 3: Accuracy Validation Tests 🔄 PENDING

#### Static Accuracy (No Swing)
1. Place phone in calibrated grip position
2. Measure reported bat transform vs ground truth (motion capture / AR markers)
3. Repeat at 10 different orientations
4. **Target:** <2mm position, <1° orientation error

#### Dynamic Accuracy (Swing)
1. Use high-speed camera (240fps+) to track phone + bat
2. Compare reported bat trajectory vs ground truth
3. Test specific shots: cover drive, pull, cut, defensive
4. **Target:** <5mm position error at contact point

#### Edge Detection Validation
1. Program ball to hit specific bat locations (center, 1cm off-center, 2cm, 3cm)
2. Use phone bat to intercept
3. Verify contact classification: sweet spot / edge / miss
4. **Target:** 100% correct classification at ≥2cm from edge

#### Sub-Frame Contact Prediction ✅ IMPLEMENTED
- Added `PredictSubFrameContact()` in CricketBattingComponent.cpp
- Solves contact time within frame (0-1 fraction) for 94mph ball
- Ball travels ~70cm per 60Hz frame → sub-frame precision essential

### Phase 4: End-to-End Latency Test (The "94mph Test") 🔄 READY TO RUN

**Scenario:** Ball at 42 m/s (94 mph), arriving at crease in ~450ms from release.

| Time | Event | Phone Bat Pipeline Must Complete |
|------|-------|----------------------------------|
| T=0ms | Ball released | - |
| T=200ms | Ball halfway, swing initiated | Phone detects backswing |
| T=300ms | Downswing begins | Contact prediction sent |
| T=350ms | Predicted contact | PC must have accurate bat pose |
| T=400ms | **Actual contact** | Bat position <5mm error |
| T=450ms | Ball at crease | Result computed |

**Test Method:**
1. Use ball machine or scripted delivery at 94mph
2. Swing at realistic timing
3. High-speed camera records phone + ball + screen
4. Measure: frame of swing start → frame of contact result on screen
5. **Pass criteria:** <20ms total latency, correct edge/sweet spot call

**Current Pipeline Optimizations (ready for testing):**
- ✅ Jitter buffer: 4 frames → 1 frame adaptive (saves ~50ms)
- ✅ Binary Protocol Buffers serialization (saves ~2-5ms + smaller packets)
- ✅ Event-driven receive thread (saves ~0.5ms)
- ✅ Receive thread priority: TPri_Normal → TPri_TimeCritical
- ✅ Full T0-T7 latency instrumentation
- ✅ Sub-frame contact prediction (solves 94mph timing)
- ✅ Adaptive clock sync during active swing

## Recommended Code Changes ✅ IMPLEMENTED

### 1. Reduce Jitter Buffer (Immediate) ✅ DONE
```cpp
// PhoneBatReceiverComponent.h
static const int32 MIN_JITTER_BUFFER_SIZE = 1; // Was 4
static const int32 MAX_JITTER_BUFFER_SIZE = 4; // Adaptive upper bound
// Adaptive sizing based on measured network jitter
```
**Location:** PhoneBatReceiverComponent.cpp - UpdateJitterBuffer() now adaptive

### 2. Binary Protocol (High Impact) ✅ DONE
```protobuf
// PhoneBatProtocol.proto - Full implementation
message SensorFrame {
  int64 timestamp = 1;
  Vector3 accelerometer = 2;
  Vector3 gyroscope = 3;
  Quaternion orientation = 4;
  Vector3 gravity = 5;
  Vector3 linear_acceleration = 6;
  float confidence = 7;
  SwingPhase swing_phase = 8;
  float bat_speed = 9;
  int64 predicted_contact_time = 10;
  // Latency timestamps
  int64 phone_sensor_read_time_us = 11;      // T0
  int64 phone_fusion_complete_time_us = 12;  // T1
  int64 phone_send_time_us = 13;             // T2
}
```
**Files:** PhoneBatProtocol.proto, PhoneBatBinaryProtocol.h/cpp
Expected: 50-70% packet size reduction, 2-3ms faster serialization. ✅ Implemented

### 3. Adaptive Jitter Buffer ✅ DONE
```cpp
// In UpdateJitterBuffer()
float CurrentJitter = CalculateJitter();
int32 OptimalBufferSize = FMath::Clamp(
    FMath::CeilToInt(CurrentJitter / FrameTime), 
    1, 4);
CurrentJitterBufferSize = OptimalBufferSize;
```
**Location:** PhoneBatReceiverComponent.cpp - fully implemented with EWMA smoothing

### 4. Sub-Frame Contact Prediction ✅ DONE
```cpp
// In CricketBattingComponent.cpp
FContactResult DetectBatBallContactSubFrame(const FTransform& BatTransform, const FVector& BatVelocity, float DeltaTime) const;
bool PredictSubFrameContact(const FTransform& BatTransform, const FVector& BatVelocity,
                            const FVector& BallPos, const FVector& BallVel, float BallRadius,
                            float DeltaTime, FContactResult& OutResult) const;
```
**Features:**
- Solves contact time within frame (0-1 fraction) using relative motion equations
- Critical for 94mph ball (42 m/s = 70cm per 60Hz frame)
- Records ContactDetectionTimeUs (T7) for latency instrumentation

### 5. Dynamic Calibration Refinement 🔄 PENDING
```cpp
// During active swing, refine calibration using:
void RefineCalibrationDuringSwing(const FPhoneBatDataPacket& Packet) {
    // Use swing phase where bat is stable (transition, contact)
    // to correct phone-to-bat transform drift
}
```
**Status:** Not yet implemented - next priority

### 6. Increase Receive Thread Priority ✅ DONE
```cpp
// PhoneBatReceiverComponent.cpp
ReceiveThread = FRunnableThread::Create(
    ReceiveRunnable, 
    TEXT("PhoneBatReceiveThread"), 
    128 * 1024, 
    TPri_TimeCritical  // Was TPri_Normal
);
```

### 7. Predictive Contact on Phone ✅ VERIFIED
The phone already runs `SwingDetector.predictContactTime()` - now sent in `SensorFramePayload` with latency timestamps and used on PC for early preparation.

### 8. Event-Driven Receive Thread ✅ DONE
```cpp
// PhoneBatReceiverComponent.cpp - Run()
while (bShouldRun) {
    // Event-driven wait instead of 1ms sleep
    bool bHasData = SocketSubsystem->WaitForSocket(ReceiveSocket, FSocketWaitConditions().SetRead(true), FTimespan(0, 0, 0, 1));
    if (bHasData) {
        // Process packet immediately
    }
}
```
Replaced `FPlatformProcess::Sleep(0.001f)` with `WaitForSocket()` with 1ms timeout.

## Testing Checklist

### Automated Tests (CI/CD)
- [ ] Unit test: Packet serialization round-trip < 1ms
- [ ] Unit test: Jitter buffer correctly orders packets
- [ ] Unit test: Clock sync converges within 5 samples
- [ ] Unit test: Calibration transform invertible (Phone→Bat→Phone ≈ Identity)
- [ ] Integration test: Simulated 100Hz sensor stream → correct bat pose at 60Hz

### Manual Tests (Per Session)
- [ ] Pair phone → PC in < 5 seconds
- [ ] Calibration completes in < 30 seconds with >0.8 quality score
- [ ] Static accuracy: <2mm / <1° at 10 orientations
- [ ] Swing detection: Correct phase sequence (Idle→Back→Down→Contact→Follow→Recovery)
- [ ] Contact prediction: Triggers 50-150ms before actual contact
- [ ] Latency display shows <15ms average, <5ms jitter

### "94mph" Validation Test
- [ ] Ball machine at 42 m/s
- [ ] 10 cover drives, 10 pulls, 10 cuts, 10 defensive
- [ ] High-speed camera verification
- [ ] Edge detection: 100% accuracy at ≥2cm from sweet spot
- [ ] No "ghost contacts" (false positives)
- [ ] No missed contacts (false negatives)

## Monitoring Dashboard (Real-time)

Add to CricketHUD:
```cpp
// PhoneBatConnectionInfo already has:
LatencyMS        // Current one-way latency
JitterMs         // Current jitter
PacketLossPercent // Current packet loss
SwingPhase       // Current phase
ContactConfidence // Prediction confidence
```

Display as overlay during batting:
```
┌─────────────────────────────────┐
│ 📶 Phone Bat: Connected         │
│ ⏱ Latency: 12.3ms (jitter 1.2)  │
│ 📦 Loss: 0.1%                   │
│ 🏏 Phase: DOWNSWING (0.67)      │
│ 🎯 Contact: 0.043s @ 0.91 conf  │
│ ✅ Calibrated: 94% quality      │
└─────────────────────────────────┘
```

## Success Criteria Summary

| Metric | Current (Estimated) | Target | Critical? | Status |
|--------|---------------------|--------|-----------|--------|
| End-to-end latency | ~25-40ms* | <20ms | YES | 🔄 Testing |
| Jitter | ~5-10ms* | <5ms | YES | 🔄 Testing |
| Static position accuracy | Unknown | <2mm | YES | 🔄 Pending |
| Static orientation accuracy | Unknown | <1° | YES | 🔄 Pending |
| Dynamic position at contact | Unknown | <5mm | YES | 🔄 Pending |
| Edge detection accuracy | Unknown | 100% at ≥2cm | YES | 🔄 Pending |
| Packet loss (WiFi) | Unknown | <0.5% | YES | 🔄 Pending |
| Calibration time | ~30s | <20s | NO | 🔄 Pending |
| Battery life (phone) | Unknown | >2 hours | NO | 🔄 Pending |

*Post-optimization estimates: Jitter buffer -50ms, Binary proto -3ms, Event-driven -0.5ms, Thread priority -1ms, Sub-frame prediction eliminates frame-alignment latency. Remaining: sensor fusion ~5ms, network ~5-10ms, game tick ~0-16ms (now sub-frame). Total estimated: 25-40ms.

## Next Steps

1. **Immediate:** Reduce `JITTER_BUFFER_SIZE` to 1, test stability
2. **Week 1:** Implement binary protocol (Protocol Buffers)
3. **Week 2:** Add automated latency measurement instrumentation
4. **Week 3:** Run "94mph" validation with high-speed camera
5. **Week 4:** Dynamic calibration refinement during swing

---

*Generated from code analysis of Cricket Simulator PhoneBat system*
*Date: 2026-11-23*