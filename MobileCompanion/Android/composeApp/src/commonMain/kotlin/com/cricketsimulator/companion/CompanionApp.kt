package com.cricketsimulator.companion

import android.app.Application
import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob

/**
 * Main application class for the Cricket Simulator Companion App
 * Initializes sensor manager, network manager, and coordinates data flow
 */
class CompanionApp : Application() {

    companion object {
        @Volatile private var INSTANCE: CompanionApp? = null
        fun getInstance(): CompanionApp = INSTANCE!!
    }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)

    // Core managers
    val sensorManager: SensorManager by lazy { SensorManager(this) }
    val networkManager: NetworkManager by lazy { NetworkManager.getInstance(scope) }
    val batSimulator: BatSimulator by lazy { BatSimulator(sensorManager, networkManager) }

    override fun onCreate() {
        super.onCreate()
        INSTANCE = this

        // Initialize network callbacks
        setupNetworkCallbacks()
    }

    private fun setupNetworkCallbacks() {
        networkManager.onSensorFrame = { frame ->
            batSimulator.onSensorFrameReceived(frame)
        }

        networkManager.onSwingPhase = { phase ->
            batSimulator.onSwingPhaseReceived(phase)
        }

        networkManager.onContactPredict = { prediction ->
            batSimulator.onContactPredictionReceived(prediction)
        }

        networkManager.onCalibrationData = { calibration ->
            batSimulator.onCalibrationReceived(calibration)
        }

        networkManager.onClockSyncUpdate = { offsetUs, rttUs ->
            sensorManager.updateLatency(rttUs / 1_000_000.0, (System.currentTimeMillis() + offsetUs / 1000.0) / 1000.0)
        }

        networkManager.onConfigUpdate = { config ->
            sensorManager.updateConfig(config)
        }
    }

    override fun onTerminate() {
        sensorManager.stopUpdates()
        networkManager.disconnect()
        scope.cancel()
        super.onTerminate()
    }
}

/**
 * Bat Simulator - Core logic for processing sensor data and simulating bat physics
 * This runs on the phone and sends processed data to the UE game
 */
class BatSimulator(
    private val sensorManager: SensorManager,
    private val networkManager: NetworkManager
) {

    // Bat physical properties
    private val batLength = 0.965f // meters (standard)
    private val batMass = 1.2f // kg
    private val sweetSpotDistance = 0.18f // meters from handle

    // State
    private var currentSwingPhase: SwingPhase = SwingPhase.IDLE
    private var swingStartTime = 0L
    private var peakBatSpeed = 0f
    private var phoneToBatTransform = Mat4(16).apply {
        this[0] = 1f; this[5] = 1f; this[10] = 1f; this[15] = 1f
    }
    private var isCalibrated = false

    // Physics state
    private var batPosition = Vec3(0f, 0f, 0f)
    private var batVelocity = Vec3(0f, 0f, 0f)
    private var batRotation = Quat(0f, 0f, 0f, 1f)
    private var batAngularVelocity = Vec3(0f, 0f, 0f)

    // Contact prediction
    private var predictedContactTime: Double? = null
    private var predictedContactPoint = Vec3(0f, 0f, 0f)
    private var contactConfidence = 0f

    // Swing metrics
    private var backswingDuration = 0.0
    private var downswingDuration = 0.0
    private var batAngleAtContact = 0f
    private var wristRotationAtContact = 0f
    private var swingPlaneAngle = 0f

    // Frame counter for sequence numbers
    private var frameSequence = 0

    /**
     * Process incoming sensor frame from sensor manager
     * Runs sensor fusion, updates bat physics, predicts contact
     */
    fun onSensorFrameReceived(frame: SensorFrame) {
        if (!isCalibrated) return

        // Convert phone sensor data to bat coordinate space
        val batTransform = transformPhoneToBat(frame)

        // Update bat physics state
        updateBatPhysics(batTransform, frame)

        // Predict contact if in downswing
        if (currentSwingPhase == SwingPhase.DOWNSWING) {
            predictContact(frame)
        }

        // Send bat state to UE game
        sendBatStateToGame(frame)
    }

    /**
     * Transform phone sensor data to bat coordinate space using calibration
     */
    private fun transformPhoneToBat(frame: SensorFrame): BatTransformData {
        // Apply phone-to-bat transform
        val phoneOrientation = frame.orientation
        val batOrientation = quatMultiply(phoneToBatTransform.toQuat(), phoneOrientation)

        // Transform acceleration to bat space
        val phoneAccel = frame.acceleration
        val batAccel = rotateVectorByQuat(phoneAccel, batOrientation)

        // Transform rotation rate
        val phoneRotRate = frame.rotationRate
        val batRotRate = rotateVectorByQuat(phoneRotRate, batOrientation)

        // Calculate bat position (grip position + bat offset)
        val gripOffset = phoneToBatTransform.getTranslation()
        val batDirection = quatRotate(batOrientation, floatArrayOf(0f, 0f, -1f)) // Bat extends along -Z
        val batPos = Vec3(
            gripOffset[0] + batDirection[0] * sweetSpotDistance,
            gripOffset[1] + batDirection[1] * sweetSpotDistance,
            gripOffset[2] + batDirection[2] * sweetSpotDistance
        )

        return BatTransformData(
            position = batPos,
            orientation = batOrientation,
            linearVelocity = batVelocity,
            angularVelocity = batRotRate,
            linearAcceleration = batAccel
        )
    }

    /**
     * Update bat physics using sensor data
     */
    private fun updateBatPhysics(batTransform: BatTransformData, frame: SensorFrame) {
        val dt = if (frameSequence > 0) {
            (frame.timestamp - lastFrameTimestamp) / 1_000_000.0 // Convert to seconds
        } else {
            0.01 // 100Hz default
        }
        lastFrameTimestamp = frame.timestamp

        // Integrate acceleration to get velocity
        batVelocity = Vec3(
            batVelocity[0] + batTransform.linearAcceleration[0] * dt,
            batVelocity[1] + batTransform.linearAcceleration[1] * dt,
            batVelocity[2] + batTransform.linearAcceleration[2] * dt
        )

        // Integrate velocity to get position
        batPosition = Vec3(
            batPosition[0] + batVelocity[0] * dt,
            batPosition[1] + batVelocity[1] * dt,
            batPosition[2] + batVelocity[2] * dt
        )

        // Update rotation
        batRotation = batTransform.orientation
        batAngularVelocity = batTransform.angularVelocity

        frameSequence++
    }

    /**
     * Predict ball contact time and position
     */
    private fun predictContact(frame: SensorFrame) {
        // Use swing detector's prediction
        predictedContactTime = sensorManager.swingDetector.predictContactTime()
        contactConfidence = sensorManager.swingDetector.getConfidence()

        if (predictedContactTime != null) {
            // Predict where bat will be at contact time
            val timeToContact = (predictedContactTime!! * 1_000_000 - frame.timestamp) / 1_000_000.0
            if (timeToContact > 0 && timeToContact < 0.5) {
                predictedContactPoint = Vec3(
                    batPosition[0] + batVelocity[0] * timeToContact,
                    batPosition[1] + batVelocity[1] * timeToContact,
                    batPosition[2] + batVelocity[2] * timeToContact
                )
            }
        }
    }

    /**
     * Send bat state to UE game via network
     */
    private fun sendBatStateToGame(frame: SensorFrame) {
        // Send swing phase update
        val phasePayload = SwingPhasePayload(
            timestamp = frame.timestamp.toLong(),
            currentPhase = currentSwingPhase,
            phaseProgress = calculatePhaseProgress(),
            batSpeed = sensorManager.batSpeed.value,
            maxBatSpeed = peakBatSpeed,
            batVelocity = batVelocity,
            batAngularVelocity = batAngularVelocity,
            confidence = contactConfidence,
            phoneSensorReadTimeUs = frame.phoneSensorReadTimeUs,
            phoneFusionCompleteTimeUs = frame.phoneFusionCompleteTimeUs,
            phoneSendTimeUs = System.currentTimeMillis() * 1000
        )
        networkManager.sendSwingPhase(phasePayload)

        // Send contact prediction if available
        if (predictedContactTime != null && currentSwingPhase == SwingPhase.DOWNSWING) {
            val predictPayload = ContactPredictPayload(
                timestamp = frame.timestamp.toLong(),
                predictedContactTime = (predictedContactTime!! * 1_000_000).toLong(),
                predictedContactPoint = predictedContactPoint,
                predictedBatVelocity = batVelocity,
                predictedBatOrientation = batRotation,
                confidence = contactConfidence,
                phoneSensorReadTimeUs = frame.phoneSensorReadTimeUs,
                phoneFusionCompleteTimeUs = frame.phoneFusionCompleteTimeUs,
                phoneSendTimeUs = System.currentTimeMillis() * 1000
            )
            networkManager.sendContactPrediction(predictPayload)
        }
    }

    /**
     * Handle swing phase change from sensor manager
     */
    fun onSwingPhaseReceived(phase: SwingPhasePayload) {
        val newPhase = phase.currentPhase

        if (newPhase != currentSwingPhase) {
            handlePhaseTransition(currentSwingPhase, newPhase, phase)
            currentSwingPhase = newPhase
            peakBatSpeed = max(peakBatSpeed, phase.batSpeed)
        }
    }

    private fun handlePhaseTransition(oldPhase: SwingPhase, newPhase: SwingPhase, phase: SwingPhasePayload) {
        when (newPhase) {
            SwingPhase.BACKSWING -> {
                swingStartTime = System.currentTimeMillis()
                peakBatSpeed = 0f
            }
            SwingPhase.DOWNSWING -> {
                if (oldPhase == SwingPhase.BACKSWING) {
                    backswingDuration = (System.currentTimeMillis() - swingStartTime) / 1000.0
                }
            }
            SwingPhase.CONTACT -> {
                if (oldPhase == SwingPhase.DOWNSWING) {
                    downswingDuration = (System.currentTimeMillis() - swingStartTime) / 1000.0 - backswingDuration
                    // Capture contact metrics
                    batAngleAtContact = calculateBatAngle()
                    wristRotationAtContact = calculateWristRotation()
                    swingPlaneAngle = calculateSwingPlane()
                }
            }
            SwingPhase.FOLLOW_THROUGH -> {
                // Swing complete, send result
                sendSwingResult()
            }
            SwingPhase.IDLE -> {
                // Reset for next swing
                backswingDuration = 0.0
                downswingDuration = 0.0
                peakBatSpeed = 0f
            }
        }
    }

    /**
     * Send final swing result to game
     */
    private fun sendSwingResult() {
        val result = SwingResultPayload(
            contactTimestamp = System.currentTimeMillis() * 1000,
            shotType = classifyShot(),
            timingQuality = calculateTimingQuality(),
            powerRating = calculatePowerRating(),
            controlRating = calculateControlRating(),
            contactPoint = predictedContactPoint,
            batVelocityAtContact = batVelocity,
            batOrientationAtContact = batRotation,
            wristRotationAtContact = wristRotationAtContact,
            swingFrames = sensorManager.getSensorHistory()
        )
        networkManager.sendSwingResult(result)
    }

    /**
     * Handle calibration data from game
     */
    fun onCalibrationReceived(calibration: CalibrationPayload) {
        if (calibration.isComplete) {
            phoneToBatTransform = calibration.phoneToBatTransform
            isCalibrated = true
            sensorManager._calibrationState.value = SensorManager.CalibrationState.CALIBRATED
        }
    }

    /**
     * Handle contact prediction from game (server-side prediction)
     */
    fun onContactPredictionReceived(prediction: ContactPredictPayload) {
        // Could use server prediction to refine local prediction
        // For now, just log
    }

    // Helper functions
    private var lastFrameTimestamp = 0L

    private fun calculatePhaseProgress(): Float {
        return when (currentSwingPhase) {
            SwingPhase.BACKSWING -> minOf(1f, (System.currentTimeMillis() - swingStartTime) / 1000f)
            SwingPhase.DOWNSWING -> minOf(1f, (System.currentTimeMillis() - swingStartTime - backswingDuration * 1000) / 300f)
            SwingPhase.CONTACT -> 1f
            SwingPhase.FOLLOW_THROUGH -> minOf(1f, (System.currentTimeMillis() - swingStartTime - (backswingDuration + downswingDuration) * 1000) / 500f)
            else -> 0f
        }
    }

    private fun calculateBatAngle(): Float {
        val forward = quatRotate(batRotation, floatArrayOf(0f, 0f, -1f))
        val horizontalForward = floatArrayOf(forward[0], 0f, forward[2])
        val len = kotlin.math.sqrt(horizontalForward[0] * horizontalForward[0] + horizontalForward[2] * horizontalForward[2])
        if (len > 0) {
            horizontalForward[0] /= len
            horizontalForward[2] /= len
        }
        return kotlin.math.atan2(horizontalForward[0], horizontalForward[2]) * 180f / kotlin.math.PI
    }

    private fun calculateWristRotation(): Float {
        // Simplified: use angular velocity magnitude
        return kotlin.math.sqrt(
            batAngularVelocity[0] * batAngularVelocity[0] +
            batAngularVelocity[1] * batAngularVelocity[1] +
            batAngularVelocity[2] * batAngularVelocity[2]
        ) * 180f / kotlin.math.PI
    }

    private fun calculateSwingPlane(): Float = 0f // Simplified

    private fun classifyShot(): String {
        // Simple classification based on bat angle and swing plane
        return when {
            batAngleAtContact > 30 -> "Cover Drive"
            batAngleAtContact < -30 -> "On Drive"
            swingPlaneAngle > 20 -> "Pull Shot"
            swingPlaneAngle < -20 -> "Cut Shot"
            else -> "Straight Drive"
        }
    }

    private fun calculateTimingQuality(): Float = contactConfidence
    private fun calculatePowerRating(): Float = minOf(1f, peakBatSpeed / 30f) // 30 m/s = 100%
    private fun calculateControlRating(): Float = 1f - abs(batAngleAtContact) / 90f

    // Math helpers
    private fun quatMultiply(a: Quat, b: Quat): Quat = floatArrayOf(
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]
    )

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

    private fun rotateVectorByQuat(v: Vec3, q: Quat): Vec3 {
        return quatRotate(q, v)
    }

    // Extension to get translation from Mat4
    private fun Mat4.getTranslation(): Vec3 = floatArrayOf(this[12], this[13], this[14])

    private fun Mat4.toQuat(): Quat {
        // Extract rotation quaternion from matrix (simplified)
        return floatArrayOf(this[0], this[5], this[10], this[15])
    }
}

// Data classes
data class BatTransformData(
    val position: Vec3,
    val orientation: Quat,
    val linearVelocity: Vec3,
    val angularVelocity: Vec3,
    val linearAcceleration: Vec3
)

typealias Vec3 = FloatArray
typealias Quat = FloatArray
typealias Mat4 = FloatArray