//
//  SensorManager.swift
//  CricketBatCompanion
//
//  CoreMotion sensor management for swing detection
//

import Foundation
import CoreMotion
import Combine
import simd

/// Manages all sensor data collection and processing for the cricket bat companion
@MainActor
class SensorManager: ObservableObject {
    // MARK: - Published Properties

    @Published var isRecording = false
    @Published var currentOrientation: simd_quatf = simd_quatf(ix: 0, iy: 0, iz: 0, r: 1)
    @Published var currentAcceleration: SIMD3<Float> = .zero
    @Published var currentRotationRate: SIMD3<Float> = .zero
    @Published var currentMagneticField: SIMD3<Float> = .zero
    @Published var swingPhase: SwingPhase = .idle
    @Published var batSpeed: Float = 0.0
    @Published var swingMetrics: SwingMetrics = SwingMetrics()
    @Published var calibrationState: CalibrationState = .notCalibrated
    @Published var connectionQuality: ConnectionQuality = .unknown

    // MARK: - Private Properties

    private let motionManager = CMMotionManager()
    private let altimeter = CMAltimeter()
    private var sensorQueue = OperationQueue()
    private var cancellables = Set<AnyCancellable>()

    // Ring buffers for sensor data (2 seconds at 100Hz = 200 samples)
    private let bufferSize = 200
    private var accelerationBuffer: RingBuffer<SIMD3<Float>>
    private var rotationRateBuffer: RingBuffer<SIMD3<Float>>
    private var magneticFieldBuffer: RingBuffer<SIMD3<Float>>
    private var orientationBuffer: RingBuffer<simd_quatf>
    private var timestampBuffer: RingBuffer<TimeInterval>

    // Swing detection
    private var swingDetector = SwingDetector()
    private var lastSwingPhase: SwingPhase = .idle
    private var swingStartTime: TimeInterval = 0
    private var peakBatSpeed: Float = 0.0

    // Calibration
    private var calibrationSamples: [CalibrationSample] = []
    private let requiredCalibrationSamples = 100
    private var phoneToBatTransform: simd_float4x4 = matrix_identity_float4x4

    // Latency compensation
    private var latencyEstimator = LatencyEstimator()
    private var lastSentTimestamp: TimeInterval = 0

    // Configuration
    private let updateInterval: TimeInterval = 1.0 / 100.0 // 100Hz
    private let swingDetectionThreshold: Float = 15.0 // m/s²
    private let contactDetectionThreshold: Float = 50.0 // m/s² deceleration

    // MARK: - Types

    enum SwingPhase: String, CaseIterable {
        case idle = "Idle"
        case backswing = "Backswing"
        case downswing = "Downswing"
        case contact = "Contact"
        case followThrough = "FollowThrough"
        case recovery = "Recovery"
    }

    struct SwingMetrics {
        var maxBatSpeed: Float = 0.0
        var avgBatSpeed: Float = 0.0
        var backswingDuration: TimeInterval = 0.0
        var downswingDuration: TimeInterval = 0.0
        var contactTime: TimeInterval = 0.0
        var batAngleAtContact: Float = 0.0
        var wristRotationAtContact: Float = 0.0
        var swingPlaneAngle: Float = 0.0
        var tempo: Float = 0.0 // backswing/downswing ratio
    }

    enum CalibrationState {
        case notCalibrated
        case calibratingFlat
        case calibratingGrip
        case calibratingSwing
        case calibrated
        case failed(String)
    }

    struct CalibrationSample {
        let timestamp: TimeInterval
        let orientation: simd_quatf
        let acceleration: SIMD3<Float>
        let rotationRate: SIMD3<Float>
        let phase: SwingPhase
    }

    enum ConnectionQuality {
        case unknown
        case excellent
        case good
        case fair
        case poor
        case disconnected
    }

    struct SensorFrame {
        let timestamp: TimeInterval
        let orientation: simd_quatf
        let acceleration: SIMD3<Float>
        let rotationRate: SIMD3<Float>
        let magneticField: SIMD3<Float>
        let swingPhase: SwingPhase
        let batSpeed: Float
        let predictedContactTime: TimeInterval?
        let confidence: Float
    }

    // MARK: - Initialization

    override init() {
        self.accelerationBuffer = RingBuffer(capacity: bufferSize)
        self.rotationRateBuffer = RingBuffer(capacity: bufferSize)
        self.magneticFieldBuffer = RingBuffer(capacity: bufferSize)
        self.orientationBuffer = RingBuffer(capacity: bufferSize)
        self.timestampBuffer = RingBuffer(capacity: bufferSize)

        super.init()

        setupMotionManager()
        setupSwingDetector()
    }

    // MARK: - Public Methods

    /// Start sensor updates
    func startUpdates() {
        guard motionManager.isDeviceMotionAvailable else {
            print("Device motion not available")
            return
        }

        motionManager.deviceMotionUpdateInterval = updateInterval
        motionManager.startDeviceMotionUpdates(using: .xMagneticNorthZVertical, to: sensorQueue) { [weak self] motion, error in
            guard let self = self, let motion = motion else { return }
            self.processDeviceMotion(motion)
        }

        // Start altimeter for additional context
        if CMAltimeter.isRelativeAltitudeAvailable() {
            altimeter.startRelativeAltitudeUpdates(to: sensorQueue) { [weak self] data, error in
                // Could use for height detection
            }
        }

        isRecording = true
        swingDetector.reset()
    }

    /// Stop sensor updates
    func stopUpdates() {
        motionManager.stopDeviceMotionUpdates()
        altimeter.stopRelativeAltitudeUpdates()
        isRecording = false
    }

    /// Get current sensor frame for network transmission
    func getCurrentFrame() -> SensorFrame {
        let predictedContact = swingDetector.predictContactTime()
        let confidence = swingDetector.getConfidence()

        return SensorFrame(
            timestamp: CACurrentMediaTime(),
            orientation: currentOrientation,
            acceleration: currentAcceleration,
            rotationRate: currentRotationRate,
            magneticField: currentMagneticField,
            swingPhase: swingPhase,
            batSpeed: batSpeed,
            predictedContactTime: predictedContact,
            confidence: confidence
        )
    }

    /// Get buffered sensor history for ML inference
    func getSensorHistory() -> [SensorFrame] {
        var frames: [SensorFrame] = []
        let count = min(timestampBuffer.count, bufferSize)

        for i in 0..<count {
            let idx = timestampBuffer.count - 1 - i
            guard idx >= 0 && idx < timestampBuffer.count else { continue }

            let frame = SensorFrame(
                timestamp: timestampBuffer[idx] ?? 0,
                orientation: orientationBuffer[idx] ?? simd_quatf(ix: 0, iy: 0, iz: 0, r: 1),
                acceleration: accelerationBuffer[idx] ?? .zero,
                rotationRate: rotationRateBuffer[idx] ?? .zero,
                magneticField: magneticFieldBuffer[idx] ?? .zero,
                swingPhase: swingPhase, // Simplified - would need history
                batSpeed: batSpeed,
                predictedContactTime: nil,
                confidence: 1.0
            )
            frames.append(frame)
        }

        return frames.reversed()
    }

    /// Start calibration process
    func startCalibration() {
        calibrationState = .calibratingFlat
        calibrationSamples.removeAll()
    }

    /// Process calibration step
    func processCalibrationStep(_ step: CalibrationStep) {
        switch step {
        case .placeFlat:
            calibrationState = .calibratingFlat
        case .holdGrip:
            calibrationState = .calibratingGrip
        case .slowSwing:
            calibrationState = .calibratingSwing
        case .verify:
            validateCalibration()
        }
    }

    /// Get phone-to-bat transform for coordinate space alignment
    func getPhoneToBatTransform() -> simd_float4x4 {
        return phoneToBatTransform
    }

    /// Update latency estimation with round-trip time
    func updateLatency(roundTripTime: TimeInterval, serverTimestamp: TimeInterval) {
        latencyEstimator.update(roundTripTime: roundTripTime, serverTimestamp: serverTimestamp)
    }

    /// Get estimated one-way latency
    func getEstimatedLatency() -> TimeInterval {
        return latencyEstimator.getEstimatedOneWayLatency()
    }

    // MARK: - Private Methods

    private func setupMotionManager() {
        sensorQueue.name = "com.cricketsimulator.sensors"
        sensorQueue.maxConcurrentOperationCount = 1
        sensorQueue.qualityOfService = .userInteractive
    }

    private func setupSwingDetector() {
        swingDetector.onPhaseChange = { [weak self] newPhase in
            Task { @MainActor in
                self?.handleSwingPhaseChange(newPhase)
            }
        }

        swingDetector.onContactDetected = { [weak self] metrics in
            Task { @MainActor in
                self?.handleContactDetected(metrics)
            }
        }
    }

    private func processDeviceMotion(_ motion: CMDeviceMotion) {
        let timestamp = CACurrentMediaTime()

        // Convert to our coordinate system (right-handed, Z up)
        let attitude = motion.attitude
        let quat = simd_quatf(
            ix: Float(attitude.quaternion.x),
            iy: Float(attitude.quaternion.y),
            iz: Float(attitude.quaternion.z),
            r: Float(attitude.quaternion.w)
        )

        // User acceleration (gravity removed)
        let userAccel = motion.userAcceleration
        let accel = SIMD3<Float>(
            Float(userAccel.x),
            Float(userAccel.y),
            Float(userAccel.z)
        ) * 9.80665 // Convert to m/s²

        // Rotation rate
        let rotRate = motion.rotationRate
        let rotation = SIMD3<Float>(
            Float(rotRate.x),
            Float(rotRate.y),
            Float(rotRate.z)
        )

        // Magnetic field (if available)
        var magnetic = SIMD3<Float>(0, 0, 0)
        if let field = motion.magneticField.field {
            magnetic = SIMD3<Float>(
                Float(field.x),
                Float(field.y),
                Float(field.z)
            )
        }

        // Update buffers
        accelerationBuffer.append(accel)
        rotationRateBuffer.append(rotation)
        magneticFieldBuffer.append(magnetic)
        orientationBuffer.append(quat)
        timestampBuffer.append(timestamp)

        // Update published properties
        Task { @MainActor in
            self.currentOrientation = quat
            self.currentAcceleration = accel
            self.currentRotationRate = rotation
            self.currentMagneticField = magnetic
        }

        // Process for swing detection
        swingDetector.process(
            timestamp: timestamp,
            acceleration: accel,
            rotationRate: rotation,
            orientation: quat
        )
    }

    private func handleSwingPhaseChange(_ newPhase: SwingPhase) {
        swingPhase = newPhase

        switch newPhase {
        case .backswing:
            swingStartTime = CACurrentMediaTime()
            peakBatSpeed = 0.0
            swingMetrics = SwingMetrics()

        case .downswing:
            if lastSwingPhase == .backswing {
                swingMetrics.backswingDuration = CACurrentMediaTime() - swingStartTime
            }

        case .contact:
            if lastSwingPhase == .downswing {
                swingMetrics.downswingDuration = CACurrentMediaTime() - swingStartTime - swingMetrics.backswingDuration
            }
            swingMetrics.contactTime = CACurrentMediaTime()

        case .followThrough:
            if lastSwingPhase == .contact {
                // Capture metrics at contact
                swingMetrics.maxBatSpeed = peakBatSpeed
                swingMetrics.batAngleAtContact = calculateBatAngle()
                swingMetrics.wristRotationAtContact = calculateWristRotation()
                swingMetrics.swingPlaneAngle = calculateSwingPlane()
                swingMetrics.tempo = swingMetrics.backswingDuration > 0 ?
                    Float(swingMetrics.backswingDuration / swingMetrics.downswingDuration) : 0.0
            }

        case .recovery:
            // Swing complete, metrics available
            break

        case .idle:
            // Reset for next swing
            break
        }

        lastSwingPhase = newPhase
    }

    private func handleContactDetected(_ metrics: SwingDetector.ContactMetrics) {
        swingMetrics.maxBatSpeed = metrics.batSpeed
        swingMetrics.avgBatSpeed = metrics.avgSpeed
        swingMetrics.batAngleAtContact = metrics.batAngle
        swingMetrics.wristRotationAtContact = metrics.wristRotation
        swingMetrics.swingPlaneAngle = metrics.swingPlane
    }

    private func validateCalibration() {
        guard calibrationSamples.count >= requiredCalibrationSamples else {
            calibrationState = .failed("Insufficient calibration samples")
            return
        }

        // Calculate phone-to-bat transform from calibration data
        computePhoneToBatTransform()

        calibrationState = .calibrated
    }

    private func computePhoneToBatTransform() {
        // Use calibration samples to determine the transform from phone coordinates to bat coordinates
        // This involves finding the grip orientation and aligning with bat's local space

        var sumOrientation = simd_quatf(ix: 0, iy: 0, iz: 0, r: 0)
        var validSamples = 0

        for sample in calibrationSamples {
            if sample.phase == .idle || sample.phase == .backswing {
                sumOrientation = simd_slerp(sumOrientation, sample.orientation, 1.0 / Float(validSamples + 1))
                validSamples += 1
            }
        }

        if validSamples > 0 {
            // The phone-to-bat transform aligns phone's "grip down" with bat's "handle down"
            let gripDown = simd_quatf(ix: 0, iy: 0, iz: 0, r: 1) // Phone held vertically
            let batHandleDown = sumOrientation // Average orientation during grip

            phoneToBatTransform = simd_float4x4(batHandleDown * simd_inverse(gripDown))
        }
    }

    private func calculateBatAngle() -> Float {
        // Calculate bat face angle relative to pitch
        let forward = simd_act(currentOrientation, SIMD3<Float>(0, 0, -1))
        let up = simd_act(currentOrientation, SIMD3<Float>(0, 1, 0))

        // Project onto horizontal plane
        let horizontalForward = simd_normalize(SIMD3<Float>(forward.x, 0, forward.z))
        let angle = atan2(horizontalForward.x, horizontalForward.z)

        return angle * 180.0 / .pi
    }

    private func calculateWristRotation() -> Float {
        // Calculate wrist rotation from orientation change rate
        guard rotationRateBuffer.count >= 2 else { return 0 }

        let recentRotation = rotationRateBuffer[rotationRateBuffer.count - 1] ?? .zero
        let prevRotation = rotationRateBuffer[rotationRateBuffer.count - 2] ?? .zero

        let rotationDelta = simd_length(recentRotation - prevRotation)
        return rotationDelta * 180.0 / .pi
    }

    private func calculateSwingPlane() -> Float {
        // Calculate swing plane angle from acceleration vector
        guard accelerationBuffer.count >= 10 else { return 0 }

        var sumAccel = SIMD3<Float>(0, 0, 0)
        for i in 0..<min(10, accelerationBuffer.count) {
            sumAccel += accelerationBuffer[accelerationBuffer.count - 1 - i] ?? .zero
        }

        let avgAccel = sumAccel / Float(min(10, accelerationBuffer.count))
        let planeAngle = atan2(avgAccel.y, simd_length(SIMD2<Float>(avgAccel.x, avgAccel.z)))

        return planeAngle * 180.0 / .pi
    }
}

// MARK: - Supporting Types

enum CalibrationStep {
    case placeFlat
    case holdGrip
    case slowSwing
    case verify
}

// MARK: - Ring Buffer

struct RingBuffer<T> {
    private var buffer: [T?]
    private var head = 0
    private var count = 0
    let capacity: Int

    init(capacity: Int) {
        self.capacity = capacity
        self.buffer = Array(repeating: nil, count: capacity)
    }

    mutating func append(_ value: T) {
        buffer[head] = value
        head = (head + 1) % capacity
        if count < capacity { count += 1 }
    }

    subscript(index: Int) -> T? {
        guard index >= 0 && index < count else { return nil }
        let actualIndex = (head - count + index + capacity) % capacity
        return buffer[actualIndex]
    }

    var isEmpty: Bool { count == 0 }
    var isFull: Bool { count == capacity }
}

// MARK: - Swing Detector

class SwingDetector {
    struct ContactMetrics {
        let batSpeed: Float
        let avgSpeed: Float
        let batAngle: Float
        let wristRotation: Float
        let swingPlane: Float
        let timestamp: TimeInterval
    }

    var onPhaseChange: ((SensorManager.SwingPhase) -> Void)?
    var onContactDetected: ((ContactMetrics) -> Void)?

    private var currentPhase: SensorManager.SwingPhase = .idle
    private var accelerationHistory: [Float] = []
    private var speedHistory: [Float] = []
    private var orientationHistory: [simd_quatf] = []
    private var timestampHistory: [TimeInterval] = []

    private let historyLength = 50
    private var peakSpeed: Float = 0.0
    private var contactDetected = false

    func reset() {
        currentPhase = .idle
        accelerationHistory.removeAll()
        speedHistory.removeAll()
        orientationHistory.removeAll()
        timestampHistory.removeAll()
        peakSpeed = 0.0
        contactDetected = false
    }

    func process(timestamp: TimeInterval, acceleration: SIMD3<Float>, rotationRate: SIMD3<Float>, orientation: simd_quatf) {
        let accelMagnitude = simd_length(acceleration)
        let speed = estimateBatSpeed(acceleration: acceleration, rotationRate: rotationRate)

        // Maintain history
        accelerationHistory.append(accelMagnitude)
        speedHistory.append(speed)
        orientationHistory.append(orientation)
        timestampHistory.append(timestamp)

        if accelerationHistory.count > historyLength {
            accelerationHistory.removeFirst()
            speedHistory.removeFirst()
            orientationHistory.removeFirst()
            timestampHistory.removeFirst()
        }

        // Detect phase transitions
        detectPhase(speed: speed, accelMagnitude: accelMagnitude, timestamp: timestamp)

        peakSpeed = max(peakSpeed, speed)
    }

    private func detectPhase(speed: Float, accelMagnitude: Float, timestamp: TimeInterval) {
        let newPhase: SensorManager.SwingPhase

        switch currentPhase {
        case .idle:
            if speed > 3.0 && accelMagnitude > 10.0 {
                newPhase = .backswing
            } else {
                newPhase = .idle
            }

        case .backswing:
            if speed > peakSpeed * 0.8 && accelMagnitude < 5.0 {
                // Transition to downswing when speed peaks and acceleration drops
                newPhase = .downswing
            } else if speed < 1.0 {
                newPhase = .idle
            } else {
                newPhase = .backswing
            }

        case .downswing:
            if accelMagnitude > 40.0 && speed > peakSpeed * 0.5 {
                // Sharp deceleration = contact
                newPhase = .contact
                contactDetected = true
            } else if speed < 1.0 {
                newPhase = .recovery
            } else {
                newPhase = .downswing
            }

        case .contact:
            newPhase = .followThrough

        case .followThrough:
            if speed < 2.0 {
                newPhase = .recovery
            } else {
                newPhase = .followThrough
            }

        case .recovery:
            if speed < 0.5 {
                newPhase = .idle
                peakSpeed = 0.0
                contactDetected = false
            } else {
                newPhase = .recovery
            }
        }

        if newPhase != currentPhase {
            currentPhase = newPhase
            onPhaseChange?(newPhase)

            if newPhase == .contact {
                let metrics = computeContactMetrics()
                onContactDetected?(metrics)
            }
        }
    }

    private func estimateBatSpeed(acceleration: SIMD3<Float>, rotationRate: SIMD3<Float>) -> Float {
        // Simple integration of acceleration for speed estimation
        // In practice, would use sensor fusion with gyroscope
        guard timestampHistory.count >= 2 else { return 0 }

        let dt = timestampHistory.last! - timestampHistory[timestampHistory.count - 2]
        let accelAlongBat = simd_dot(acceleration, SIMD3<Float>(0, 0, -1)) // Assuming bat along -Z

        if speedHistory.count > 0 {
            return max(0, speedHistory.last! + accelAlongBat * Float(dt))
        }
        return max(0, accelAlongBat * Float(dt))
    }

    private func computeContactMetrics() -> ContactMetrics {
        let avgSpeed = speedHistory.isEmpty ? 0 : speedHistory.reduce(0, +) / Float(speedHistory.count)
        let batAngle = calculateBatAngle()
        let wristRotation = calculateWristRotation()
        let swingPlane = calculateSwingPlane()

        return ContactMetrics(
            batSpeed: peakSpeed,
            avgSpeed: avgSpeed,
            batAngle: batAngle,
            wristRotation: wristRotation,
            swingPlane: swingPlane,
            timestamp: timestampHistory.last ?? 0
        )
    }

    func predictContactTime() -> TimeInterval? {
        guard currentPhase == .downswing, speedHistory.count >= 5 else { return nil }

        // Linear extrapolation to estimate when speed will peak
        let recentSpeeds = Array(speedHistory.suffix(5))
        let recentTimes = Array(timestampHistory.suffix(5))

        // Fit line to recent speed data
        var sumX: Float = 0, sumY: Float = 0, sumXY: Float = 0, sumXX: Float = 0
        for i in 0..<recentSpeeds.count {
            let x = Float(recentTimes[i] - recentTimes[0])
            let y = recentSpeeds[i]
            sumX += x
            sumY += y
            sumXY += x * y
            sumXX += x * x
        }

        let n = Float(recentSpeeds.count)
        let slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX)

        if slope < 0 {
            // Speed is decreasing, contact imminent
            return timestampHistory.last
        }

        // Estimate time to peak
        let currentSpeed = recentSpeeds.last!
        let timeToPeak = -currentSpeed / slope

        if timeToPeak > 0 && timeToPeak < 0.5 {
            return (timestampHistory.last ?? 0) + TimeInterval(timeToPeak)
        }

        return nil
    }

    func getConfidence() -> Float {
        // Confidence based on data quality and phase clarity
        var confidence: Float = 0.5

        if speedHistory.count >= 10 { confidence += 0.2 }
        if accelerationHistory.count >= 10 { confidence += 0.1 }
        if currentPhase != .idle { confidence += 0.2 }

        return min(1.0, confidence)
    }

    private func calculateBatAngle() -> Float {
        guard let orientation = orientationHistory.last else { return 0 }
        let forward = simd_act(orientation, SIMD3<Float>(0, 0, -1))
        let horizontalForward = simd_normalize(SIMD3<Float>(forward.x, 0, forward.z))
        return atan2(horizontalForward.x, horizontalForward.z) * 180.0 / .pi
    }

    private func calculateWristRotation() -> Float {
        guard orientationHistory.count >= 2 else { return 0 }
        let current = orientationHistory.last!
        let previous = orientationHistory[orientationHistory.count - 2]
        let delta = simd_angle(current * simd_inverse(previous))
        return delta * 180.0 / .pi
    }

    private func calculateSwingPlane() -> Float {
        guard accelerationHistory.count >= 10 else { return 0 }

        var sumAccel = SIMD3<Float>(0, 0, 0)
        for i in 0..<min(10, accelerationHistory.count) {
            // Need full vector, not just magnitude
            sumAccel += SIMD3<Float>(0, 0, 0) // Simplified
        }
        return 0.0
    }
}

// MARK: - Latency Estimator

class LatencyEstimator {
    private var samples: [(roundTrip: TimeInterval, serverTime: TimeInterval, localTime: TimeInterval)] = []
    private let maxSamples = 20
    private var clockOffset: TimeInterval = 0

    func update(roundTripTime: TimeInterval, serverTimestamp: TimeInterval) {
        let localTime = CACurrentMediaTime()
        let oneWay = roundTripTime / 2.0
        let estimatedServerTime = localTime + clockOffset

        // Update clock offset using NTP-style algorithm
        let offset = serverTimestamp - (localTime + oneWay)
        clockOffset = clockOffset * 0.9 + offset * 0.1 // Exponential moving average

        samples.append((roundTripTime, serverTimestamp, localTime))
        if samples.count > maxSamples {
            samples.removeFirst()
        }
    }

    func getEstimatedOneWayLatency() -> TimeInterval {
        guard !samples.isEmpty else { return 50.0 / 1000.0 } // Default 50ms

        let avgRTT = samples.map { $0.roundTrip }.reduce(0, +) / Double(samples.count)
        return avgRTT / 2.0
    }

    func getClockOffset() -> TimeInterval {
        return clockOffset
    }
}