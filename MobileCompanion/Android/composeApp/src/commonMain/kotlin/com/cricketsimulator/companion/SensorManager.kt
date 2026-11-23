package com.cricketsimulator.companion

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager as AndroidSensorManager
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlin.math.sqrt
import kotlin.math.atan2
import kotlin.math.PI
import kotlin.system.measureTimeMillis

// Type aliases for SIMD-like operations
typealias Vec3 = FloatArray
typealias Quat = FloatArray // [x, y, z, w]
typealias Mat4 = FloatArray // 16 elements column-major

/**
 * Android SensorManager using Kotlin Flow for reactive sensor streams
 * Mirrors iOS SensorManager functionality for cross-platform consistency
 */
class SensorManager(private val context: Context) {

    // MARK: - Published State (StateFlow)

    private val _isRecording = MutableStateFlow(false)
    val isRecording: StateFlow<Boolean> = _isRecording

    private val _currentOrientation = MutableStateFlow<Quat>(floatArrayOf(0f, 0f, 0f, 1f))
    val currentOrientation: StateFlow<Quat> = _currentOrientation

    private val _currentAcceleration = MutableStateFlow<Vec3>(floatArrayOf(0f, 0f, 0f))
    val currentAcceleration: StateFlow<Vec3> = _currentAcceleration

    private val _currentRotationRate = MutableStateFlow<Vec3>(floatArrayOf(0f, 0f, 0f))
    val currentRotationRate: StateFlow<Vec3> = _currentRotationRate

    private val _currentMagneticField = MutableStateFlow<Vec3>(floatArrayOf(0f, 0f, 0f))
    val currentMagneticField: StateFlow<Vec3> = _currentMagneticField

    private val _swingPhase = MutableStateFlow<SwingPhase>(SwingPhase.IDLE)
    val swingPhase: StateFlow<SwingPhase> = _swingPhase

    private val _batSpeed = MutableStateFlow(0f)
    val batSpeed: StateFlow<Float> = _batSpeed

    private val _swingMetrics = MutableStateFlow(SwingMetrics())
    val swingMetrics: StateFlow<SwingMetrics> = _swingMetrics

    private val _calibrationState = MutableStateFlow<CalibrationState>(CalibrationState.NOT_CALIBRATED)
    val calibrationState: StateFlow<CalibrationState> = _calibrationState

    private val _connectionQuality = MutableStateFlow<ConnectionQuality>(ConnectionQuality.UNKNOWN)
    val connectionQuality: StateFlow<ConnectionQuality> = _connectionQuality

    // MARK: - Private Properties

    private val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as AndroidSensorManager
    private val sensorQueue = HandlerThread("SensorThread").apply { start() }.looper
    private val handler = Handler(sensorQueue)

    private val accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
    private val gyroscope = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
    private val magnetometer = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD)
    private val gameRotationVector = sensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR)
    private val rotationVector = sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)

    // Ring buffers for sensor history (2 seconds at 100Hz = 200 samples)
    private val bufferSize = 200
    private val accelerationBuffer = RingBuffer<Vec3>(bufferSize)
    private val rotationRateBuffer = RingBuffer<Vec3>(bufferSize)
    private val magneticFieldBuffer = RingBuffer<Vec3>(bufferSize)
    private val orientationBuffer = RingBuffer<Quat>(bufferSize)
    private val timestampBuffer = RingBuffer<Long>(bufferSize)

    // Swing detection
    private val swingDetector = SwingDetector()
    private var lastSwingPhase = SwingPhase.IDLE
    private var swingStartTime = 0L
    private var peakBatSpeed = 0f

    // Calibration
    private val calibrationSamples = mutableListOf<CalibrationSample>()
    private val requiredCalibrationSamples = 100
    private var phoneToBatTransform = Mat4(16).apply { this[0] = 1f; this[5] = 1f; this[10] = 1f; this[15] = 1f }

    // Latency compensation
    private val latencyEstimator = LatencyEstimator()

    // Coroutines
    private var sensorJob: Job? = null
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)

    // Configuration
    private val updateIntervalUs = 10_000L // 100Hz = 10ms
    private val swingDetectionThreshold = 15f // m/s²
    private val contactDetectionThreshold = 50f // m/s² deceleration

    // Sensor listener
    private val sensorListener = object : SensorEventListener {
        override fun onSensorChanged(event: SensorEvent) {
            val timestamp = System.nanoTime() / 1_000_000L // Convert to milliseconds

            when (event.sensor.type) {
                Sensor.TYPE_ACCELEROMETER -> {
                    val accel = floatArrayOf(event.values[0], event.values[1], event.values[2])
                    accelerationBuffer.add(accel)
                    timestampBuffer.add(timestamp)
                    _currentAcceleration.value = accel
                }
                Sensor.TYPE_GYROSCOPE -> {
                    val rot = floatArrayOf(event.values[0], event.values[1], event.values[2])
                    rotationRateBuffer.add(rot)
                    _currentRotationRate.value = rot
                }
                Sensor.TYPE_MAGNETIC_FIELD -> {
                    val mag = floatArrayOf(event.values[0], event.values[1], event.values[2])
                    magneticFieldBuffer.add(mag)
                    _currentMagneticField.value = mag
                }
                Sensor.TYPE_GAME_ROTATION_VECTOR, Sensor.TYPE_ROTATION_VECTOR -> {
                    val quat = floatArrayOf(event.values[0], event.values[1], event.values[2], event.values[3])
                    orientationBuffer.add(quat)
                    _currentOrientation.value = quat
                }
            }

            // Process for swing detection when we have all data
            if (event.sensor.type == Sensor.TYPE_ACCELEROMETER) {
                processSwingDetection(timestamp)
            }
        }

        override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {
            // Handle accuracy changes if needed
        }
    }

    // MARK: - Public Methods

    /**
     * Start sensor updates at 100Hz
     */
    fun startUpdates() {
        if (_isRecording.value) return

        sensorJob = scope.launch {
            // Register listeners on sensor thread
            withContext(Dispatchers.IO) {
                val rate = SensorManager.SENSOR_DELAY_FASTEST // ~100Hz on most devices
                accelerometer?.let { sensorManager.registerListener(sensorListener, it, rate, handler) }
                gyroscope?.let { sensorManager.registerListener(sensorListener, it, rate, handler) }
                magnetometer?.let { sensorManager.registerListener(sensorListener, it, rate, handler) }
                // Prefer game rotation vector (no magnetic) for gaming
                (gameRotationVector ?: rotationVector)?.let { sensorManager.registerListener(sensorListener, it, rate, handler) }
            }

            _isRecording.value = true
            swingDetector.reset()
        }
    }

    /**
     * Stop sensor updates
     */
    fun stopUpdates() {
        sensorJob?.cancel()
        sensorJob = null

        scope.launch(Dispatchers.IO) {
            sensorManager.unregisterListener(sensorListener)
        }

        _isRecording.value = false
    }

    /**
     * Get current sensor frame for network transmission
     */
    fun getCurrentFrame(): SensorFrame {
        val nowUs = System.currentTimeMillis() * 1000L // Microseconds
        val predictedContact = swingDetector.predictContactTime()
        val confidence = swingDetector.getConfidence()

        return SensorFrame(
            timestamp = nowUs.toDouble(),
            orientation = _currentOrientation.value,
            acceleration = _currentAcceleration.value,
            rotationRate = _currentRotationRate.value,
            magneticField = _currentMagneticField.value,
            swingPhase = _swingPhase.value,
            batSpeed = _batSpeed.value,
            predictedContactTime = predictedContact?.let { it * 1_000_000.0 },
            confidence = confidence,

            // Latency instrumentation
            phoneSensorReadTimeUs = nowUs,
            phoneFusionCompleteTimeUs = nowUs,
            phoneSendTimeUs = nowUs
        )
    }

    /**
     * Get buffered sensor history for ML inference
     */
    fun getSensorHistory(): List<SensorFrame> {
        val frames = mutableListOf<SensorFrame>()
        val count = minOf(timestampBuffer.size, bufferSize)

        for (i in 0 until count) {
            val idx = timestampBuffer.size - 1 - i
            val frame = SensorFrame(
                timestamp = (timestampBuffer.get(idx) ?: 0L).toDouble() / 1000.0,
                orientation = orientationBuffer.get(idx) ?: floatArrayOf(0f, 0f, 0f, 1f),
                acceleration = accelerationBuffer.get(idx) ?: floatArrayOf(0f, 0f, 0f),
                rotationRate = rotationRateBuffer.get(idx) ?: floatArrayOf(0f, 0f, 0f),
                magneticField = magneticFieldBuffer.get(idx) ?: floatArrayOf(0f, 0f, 0f),
                swingPhase = _swingPhase.value,
                batSpeed = _batSpeed.value,
                predictedContactTime = null,
                confidence = 1f
            )
            frames.add(frame)
        }

        return frames.reversed()
    }

    /**
     * Start calibration process
     */
    fun startCalibration() {
        _calibrationState.value = CalibrationState.CALIBRATING_FLAT
        calibrationSamples.clear()
    }

    /**
     * Process calibration step
     */
    fun processCalibrationStep(step: CalibrationStep) {
        when (step) {
            CalibrationStep.PLACE_FLAT -> _calibrationState.value = CalibrationState.CALIBRATING_FLAT
            CalibrationStep.HOLD_GRIP -> _calibrationState.value = CalibrationState.CALIBRATING_GRIP
            CalibrationStep.SLOW_SWING -> _calibrationState.value = CalibrationState.CALIBRATING_SWING
            CalibrationStep.VERIFY -> validateCalibration()
        }
    }

    /**
     * Get phone-to-bat transform for coordinate space alignment
     */
    fun getPhoneToBatTransform(): Mat4 {
        return phoneToBatTransform.copyOf()
    }

    /**
     * Update latency estimation with round-trip time
     */
    fun updateLatency(roundTripTime: Double, serverTimestamp: Double) {
        latencyEstimator.update(roundTripTime, serverTimestamp)
    }

    /**
     * Get estimated one-way latency in seconds
     */
    fun getEstimatedLatency(): Double {
        return latencyEstimator.getEstimatedOneWayLatency()
    }

    // MARK: - Private Methods

    private fun processSwingDetection(timestamp: Long) {
        val accel = _currentAcceleration.value
        val rotRate = _currentRotationRate.value
        val orientation = _currentOrientation.value

        val accelMagnitude = sqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2])

        swingDetector.process(
            timestamp = timestamp,
            acceleration = accel,
            rotationRate = rotRate,
            orientation = orientation,
            accelMagnitude = accelMagnitude
        )

        // Update bat speed from detector
        _batSpeed.value = swingDetector.currentSpeed
        peakBatSpeed = max(peakBatSpeed, swingDetector.currentSpeed)
    }

    private fun handleSwingPhaseChange(newPhase: SwingPhase) {
        _swingPhase.value = newPhase

        when (newPhase) {
            SwingPhase.BACKSWING -> {
                swingStartTime = System.currentTimeMillis()
                peakBatSpeed = 0f
                _swingMetrics.value = SwingMetrics()
            }
            SwingPhase.DOWNSWING -> {
                if (lastSwingPhase == SwingPhase.BACKSWING) {
                    _swingMetrics.value = _swingMetrics.value.copy(
                        backswingDuration = (System.currentTimeMillis() - swingStartTime).toDouble() / 1000.0
                    )
                }
            }
            SwingPhase.CONTACT -> {
                if (lastSwingPhase == SwingPhase.DOWNSWING) {
                    val backswingDur = _swingMetrics.value.backswingDuration
                    val downswingDur = (System.currentTimeMillis() - swingStartTime).toDouble() / 1000.0 - backswingDur
                    _swingMetrics.value = _swingMetrics.value.copy(
                        downswingDuration = downswingDur,
                        contactTime = System.currentTimeMillis().toDouble() / 1000.0
                    )
                }
            }
            SwingPhase.FOLLOW_THROUGH -> {
                if (lastSwingPhase == SwingPhase.CONTACT) {
                    _swingMetrics.value = _swingMetrics.value.copy(
                        maxBatSpeed = peakBatSpeed,
                        batAngleAtContact = calculateBatAngle(),
                        wristRotationAtContact = calculateWristRotation(),
                        swingPlaneAngle = calculateSwingPlane(),
                        tempo = if (_swingMetrics.value.backswingDuration > 0)
                            (_swingMetrics.value.backswingDuration / _swingMetrics.value.downswingDuration).toFloat()
                        else 0f
                    )
                }
            }
            SwingPhase.RECOVERY -> {
                // Swing complete
            }
            SwingPhase.IDLE -> {
                // Reset for next swing
            }
        }

        lastSwingPhase = newPhase
    }

    private fun validateCalibration() {
        if (calibrationSamples.size < requiredCalibrationSamples) {
            _calibrationState.value = CalibrationState.FAILED("Insufficient calibration samples: ${calibrationSamples.size}/$requiredCalibrationSamples")
            return
        }

        computePhoneToBatTransform()
        _calibrationState.value = CalibrationState.CALIBRATED
    }

    private fun computePhoneToBatTransform() {
        // Compute average orientation during grip phase
        var sumX = 0f
        var sumY = 0f
        var sumZ = 0f
        var sumW = 0f
        var validSamples = 0

        for (sample in calibrationSamples) {
            if (sample.phase == SwingPhase.IDLE || sample.phase == SwingPhase.BACKSWING) {
                val q = sample.orientation
                sumX += q[0]
                sumY += q[1]
                sumZ += q[2]
                sumW += q[3]
                validSamples++
            }
        }

        if (validSamples > 0) {
            val avgQ = floatArrayOf(
                sumX / validSamples,
                sumY / validSamples,
                sumZ / validSamples,
                sumW / validSamples
            )
            normalizeQuat(avgQ)

            // Phone-to-bat transform: align phone "grip down" with bat "handle down"
            val gripDown = floatArrayOf(0f, 0f, 0f, 1f)
            phoneToBatTransform = quatToMatrix(quatMultiply(avgQ, quatInverse(gripDown)))
        }
    }

    private fun calculateBatAngle(): Float {
        val q = _currentOrientation.value
        // Forward vector in phone space (assuming -Z is forward)
        val forward = quatRotate(q, floatArrayOf(0f, 0f, -1f))
        // Project to horizontal plane
        val horizontalForward = floatArrayOf(forward[0], 0f, forward[2])
        val len = sqrt(horizontalForward[0] * horizontalForward[0] + horizontalForward[2] * horizontalForward[2])
        if (len > 0) {
            horizontalForward[0] /= len
            horizontalForward[2] /= len
        }
        return atan2(horizontalForward[0], horizontalForward[2]) * 180f / PI
    }

    private fun calculateWristRotation(): Float {
        if (rotationRateBuffer.size < 2) return 0f

        val recent = rotationRateBuffer.get(rotationRateBuffer.size - 1) ?: floatArrayOf(0f, 0f, 0f)
        val prev = rotationRateBuffer.get(rotationRateBuffer.size - 2) ?: floatArrayOf(0f, 0f, 0f)

        val deltaX = recent[0] - prev[0]
        val deltaY = recent[1] - prev[1]
        val deltaZ = recent[2] - prev[2]

        return sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ) * 180f / PI
    }

    private fun calculateSwingPlane(): Float {
        if (accelerationBuffer.size < 10) return 0f

        var sumX = 0f
        var sumY = 0f
        var sumZ = 0f
        val count = minOf(10, accelerationBuffer.size)

        for (i in 0 until count) {
            val accel = accelerationBuffer.get(accelerationBuffer.size - 1 - i) ?: floatArrayOf(0f, 0f, 0f)
            sumX += accel[0]
            sumY += accel[1]
            sumZ += accel[2]
        }

        val avgX = sumX / count
        val avgY = sumY / count
        val avgZ = sumZ / count

        val horizontalLen = sqrt(avgX * avgX + avgZ * avgZ)
        return atan2(avgY, horizontalLen) * 180f / PI
    }

    // MARK: - Quaternion Math Helpers

    private fun normalizeQuat(q: Quat) {
        val len = sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3])
        if (len > 0) {
            q[0] /= len
            q[1] /= len
            q[2] /= len
            q[3] /= len
        }
    }

    private fun quatMultiply(a: Quat, b: Quat): Quat {
        return floatArrayOf(
            a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]
        )
    }

    private fun quatInverse(q: Quat): Quat {
        val lenSq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]
        return floatArrayOf(-q[0] / lenSq, -q[1] / lenSq, -q[2] / lenSq, q[3] / lenSq)
    }

    private fun quatRotate(q: Quat, v: Vec3): Vec3 {
        // q * v * q^-1
        val qv = floatArrayOf(q[0], q[1], q[2])
        val uv = cross(qv, v)
        val uuv = cross(qv, uv)
        return floatArrayOf(
            v[0] + 2 * (q[3] * uv[0] + uuv[0]),
            v[1] + 2 * (q[3] * uv[1] + uuv[1]),
            v[2] + 2 * (q[3] * uv[2] + uuv[2])
        )
    }

    private fun cross(a: Vec3, b: Vec3): Vec3 {
        return floatArrayOf(
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]
        )
    }

    private fun quatToMatrix(q: Quat): Mat4 {
        val x = q[0]; val y = q[1]; val z = q[2]; val w = q[3]
        val xx = x * x; val yy = y * y; val zz = z * z
        val xy = x * y; val xz = x * z; val yz = y * z
        val wx = w * x; val wy = w * y; val wz = w * z

        return floatArrayOf(
            1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy), 0f,
            2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx), 0f,
            2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy), 0f,
            0f, 0f, 0f, 1f
        )
    }
}

// MARK: - Data Classes

enum class SwingPhase {
    IDLE, BACKSWING, DOWNSWING, CONTACT, FOLLOW_THROUGH, RECOVERY
}

data class SwingMetrics(
    val maxBatSpeed: Float = 0f,
    val avgBatSpeed: Float = 0f,
    val backswingDuration: Double = 0.0,
    val downswingDuration: Double = 0.0,
    val contactTime: Double = 0.0,
    val batAngleAtContact: Float = 0f,
    val wristRotationAtContact: Float = 0f,
    val swingPlaneAngle: Float = 0f,
    val tempo: Float = 0f
)

enum class CalibrationState {
    NOT_CALIBRATED,
    CALIBRATING_FLAT,
    CALIBRATING_GRIP,
    CALIBRATING_SWING,
    CALIBRATED,
    FAILED(String)
}

enum class CalibrationStep {
    PLACE_FLAT, HOLD_GRIP, SLOW_SWING, VERIFY
}

enum class ConnectionQuality {
    UNKNOWN, EXCELLENT, GOOD, FAIR, POOR, DISCONNECTED
}

data class CalibrationSample(
    val timestamp: Long,
    val orientation: Quat,
    val acceleration: Vec3,
    val rotationRate: Vec3,
    val phase: SwingPhase
)

data class SensorFrame(
    val timestamp: Double,                    // Microseconds since epoch
    val orientation: Quat,
    val acceleration: Vec3,
    val rotationRate: Vec3,
    val magneticField: Vec3,
    val swingPhase: SwingPhase,
    val batSpeed: Float,
    val predictedContactTime: Double?,        // Microseconds since epoch
    val confidence: Float,

    // ========================================================================
    // Latency Instrumentation Timestamps (Phone side)
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    val phoneSensorReadTimeUs: Long = 0,
    // T1: Phone sensor fusion complete time (microseconds since epoch)
    val phoneFusionCompleteTimeUs: Long = 0,
    // T2: Phone serialization/send time (microseconds since epoch)
    val phoneSendTimeUs: Long = 0
)

// MARK: - Ring Buffer

class RingBuffer<T>(capacity: Int) {
    private val buffer = Array<T?>(capacity) { null }
    private var head = 0
    private var size = 0

    fun add(item: T) {
        buffer[head] = item
        head = (head + 1) % capacity
        if (size < capacity) size++
    }

    fun get(index: Int): T? {
        if (index < 0 || index >= size) return null
        val actualIndex = (head - size + index + capacity) % capacity
        return buffer[actualIndex]
    }

    val count: Int get() = size
    val isEmpty: Boolean get() = size == 0
    val isFull: Boolean get() = size == capacity
}

// MARK: - Swing Detector

class SwingDetector {
    data class ContactMetrics(
        val batSpeed: Float,
        val avgSpeed: Float,
        val batAngle: Float,
        val wristRotation: Float,
        val swingPlane: Float,
        val timestamp: Double
    )

    var onPhaseChange: ((SwingPhase) -> Unit)? = null
    var onContactDetected: ((ContactMetrics) -> Unit)? = null

    private var currentPhase = SwingPhase.IDLE
    private val accelerationHistory = mutableListOf<Float>()
    private val speedHistory = mutableListOf<Float>()
    private val orientationHistory = mutableListOf<Quat>()
    private val timestampHistory = mutableListOf<Long>()

    private val historyLength = 50
    var currentSpeed = 0f
    private var peakSpeed = 0f
    private var contactDetected = false

    fun reset() {
        currentPhase = SwingPhase.IDLE
        accelerationHistory.clear()
        speedHistory.clear()
        orientationHistory.clear()
        timestampHistory.clear()
        peakSpeed = 0f
        contactDetected = false
        currentSpeed = 0f
    }

    fun process(timestamp: Long, acceleration: Vec3, rotationRate: Vec3, orientation: Quat, accelMagnitude: Float) {
        val speed = estimateBatSpeed(acceleration, rotationRate, timestamp)
        currentSpeed = speed

        accelerationHistory.add(accelMagnitude)
        speedHistory.add(speed)
        orientationHistory.add(orientation.copyOf())
        timestampHistory.add(timestamp)

        if (accelerationHistory.size > historyLength) {
            accelerationHistory.removeAt(0)
            speedHistory.removeAt(0)
            orientationHistory.removeAt(0)
            timestampHistory.removeAt(0)
        }

        detectPhase(speed, accelMagnitude, timestamp)
        peakSpeed = max(peakSpeed, speed)
    }

    private fun detectPhase(speed: Float, accelMagnitude: Float, timestamp: Long) {
        val newPhase = when (currentPhase) {
            SwingPhase.IDLE -> if (speed > 3f && accelMagnitude > 10f) SwingPhase.BACKSWING else SwingPhase.IDLE
            SwingPhase.BACKSWING -> if (speed > peakSpeed * 0.8 && accelMagnitude < 5f) SwingPhase.DOWNSWING
                else if (speed < 1f) SwingPhase.IDLE else SwingPhase.BACKSWING
            SwingPhase.DOWNSWING -> if (accelMagnitude > 40f && speed > peakSpeed * 0.5) {
                contactDetected = true
                SwingPhase.CONTACT
            } else if (speed < 1f) SwingPhase.RECOVERY else SwingPhase.DOWNSWING
            SwingPhase.CONTACT -> SwingPhase.FOLLOW_THROUGH
            SwingPhase.FOLLOW_THROUGH -> if (speed < 2f) SwingPhase.RECOVERY else SwingPhase.FOLLOW_THROUGH
            SwingPhase.RECOVERY -> if (speed < 0.5f) {
                peakSpeed = 0f
                contactDetected = false
                SwingPhase.IDLE
            } else SwingPhase.RECOVERY
        }

        if (newPhase != currentPhase) {
            currentPhase = newPhase
            onPhaseChange?.invoke(newPhase)
            if (newPhase == SwingPhase.CONTACT) {
                onContactDetected?.invoke(computeContactMetrics())
            }
        }
    }

    private fun estimateBatSpeed(acceleration: Vec3, rotationRate: Vec3, timestamp: Long): Float {
        if (timestampHistory.size < 2) return 0f

        val dt = (timestamp - timestampHistory.last()) / 1_000_000f // Convert to seconds
        // Project acceleration along bat axis (assuming -Z)
        val accelAlongBat = acceleration[2] * -1 // Negative Z is forward

        return if (speedHistory.isNotEmpty()) {
            max(0f, speedHistory.last() + accelAlongBat * dt)
        } else {
            max(0f, accelAlongBat * dt)
        }
    }

    private fun computeContactMetrics(): ContactMetrics {
        val avgSpeed = if (speedHistory.isEmpty()) 0f else speedHistory.average()
        return ContactMetrics(
            batSpeed = peakSpeed,
            avgSpeed = avgSpeed,
            batAngle = calculateBatAngle(),
            wristRotation = calculateWristRotation(),
            swingPlane = calculateSwingPlane(),
            timestamp = (timestampHistory.lastOrNull() ?: 0L) / 1000.0
        )
    }

    fun predictContactTime(): Double? {
        if (currentPhase != SwingPhase.DOWNSWING || speedHistory.size < 5) return null

        val recentSpeeds = speedHistory.takeLast(5).toFloatArray()
        val recentTimes = timestampHistory.takeLast(5).map { it.toDouble() / 1000.0 }.toDoubleArray()

        // Linear regression
        var sumX = 0.0; var sumY = 0.0; var sumXY = 0.0; var sumXX = 0.0
        val baseTime = recentTimes[0]
        for (i in recentSpeeds.indices) {
            val x = recentTimes[i] - baseTime
            val y = recentSpeeds[i].toDouble()
            sumX += x
            sumY += y
            sumXY += x * y
            sumXX += x * x
        }

        val n = recentSpeeds.size.toDouble()
        val slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX)

        if (slope < 0) {
            return timestampHistory.lastOrNull()?.toDouble()?.div(1000.0)
        }

        val currentSpeed = recentSpeeds.last().toDouble()
        val timeToPeak = -currentSpeed / slope

        if (timeToPeak > 0 && timeToPeak < 0.5) {
            return (timestampHistory.lastOrNull()?.toDouble()?.div(1000.0) ?: 0.0) + timeToPeak
        }

        return null
    }

    fun getConfidence(): Float {
        var confidence = 0.5f
        if (speedHistory.size >= 10) confidence += 0.2f
        if (accelerationHistory.size >= 10) confidence += 0.1f
        if (currentPhase != SwingPhase.IDLE) confidence += 0.2f
        return minOf(1f, confidence)
    }

    private fun calculateBatAngle(): Float {
        val q = orientationHistory.lastOrNull() ?: floatArrayOf(0f, 0f, 0f, 1f)
        val forward = quatRotate(q, floatArrayOf(0f, 0f, -1f))
        val horizontalForward = floatArrayOf(forward[0], 0f, forward[2])
        val len = sqrt(horizontalForward[0] * horizontalForward[0] + horizontalForward[2] * horizontalForward[2])
        if (len > 0) {
            horizontalForward[0] /= len
            horizontalForward[2] /= len
        }
        return atan2(horizontalForward[0], horizontalForward[2]) * 180f / PI
    }

    private fun calculateWristRotation(): Float {
        if (orientationHistory.size < 2) return 0f
        val current = orientationHistory.last()
        val previous = orientationHistory[orientationHistory.size - 2]
        val delta = quatMultiply(current, quatInverse(previous))
        val angle = 2 * acos(minOf(1f, maxOf(-1f, delta[3])))
        return angle * 180f / PI
    }

    private fun calculateSwingPlane(): Float = 0f // Simplified

    // Quaternion helpers (same as SensorManager)
    private fun quatMultiply(a: Quat, b: Quat): Quat = floatArrayOf(
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]
    )

    private fun quatInverse(q: Quat): Quat {
        val lenSq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]
        return floatArrayOf(-q[0] / lenSq, -q[1] / lenSq, -q[2] / lenSq, q[3] / lenSq)
    }

    private fun quatRotate(q: Quat, v: Vec3): Vec3 {
        val qv = floatArrayOf(q[0], q[1], q[2])
        val uv = cross(qv, v)
        val uuv = cross(qv, uv)
        return floatArrayOf(
            v[0] + 2 * (q[3] * uv[0] + uuv[0]),
            v[1] + 2 * (q[3] * uv[1] + uuv[1]),
            v[2] + 2 * (q[3] * uv[2] + uuv[2])
        )
    }

    private fun cross(a: Vec3, b: Vec3): Vec3 = floatArrayOf(
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    )
}

// MARK: - Latency Estimator

class LatencyEstimator {
    private val samples = mutableListOf<LatencySample>()
    private val maxSamples = 20
    private var clockOffset = 0.0

    data class LatencySample(val roundTrip: Double, val serverTime: Double, val localTime: Double)

    fun update(roundTripTime: Double, serverTimestamp: Double) {
        val localTime = System.currentTimeMillis() / 1000.0
        val oneWay = roundTripTime / 2.0
        val offset = serverTimestamp - (localTime + oneWay)
        clockOffset = clockOffset * 0.9 + offset * 0.1

        samples.add(LatencySample(roundTripTime, serverTimestamp, localTime))
        if (samples.size > maxSamples) samples.removeAt(0)
    }

    fun getEstimatedOneWayLatency(): Double {
        if (samples.isEmpty()) return 0.05 // 50ms default
        return samples.map { it.roundTrip }.average() / 2.0
    }

    fun getClockOffset(): Double = clockOffset
}