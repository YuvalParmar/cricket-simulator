package com.cricketsimulator.companion

import kotlinx.coroutines.*
import kotlinx.coroutines.channels.*
import kotlinx.coroutines.flow.*
import kotlinx.serialization.*
import kotlinx.serialization.json.*
import java.net.*
import java.nio.ByteBuffer
import java.nio.channels.DatagramChannel
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference

// MARK: - Network Protocol Types

@Serializable
enum PacketType(val value: Byte) {
    HANDSHAKE(0), HANDSHAKE_ACK(1), PAIRING_REQUEST(2), PAIRING_RESPONSE(3),
    DISCONNECT(4), HEARTBEAT(5),
    CALIBRATION_START(10), CALIBRATION_DATA(11), CALIBRATION_COMPLETE(12),
    SENSOR_FRAME(20), SENSOR_BATCH(21),
    SWING_PHASE_UPDATE(30), SWING_CONTACT_PREDICT(31), SWING_RESULT(32),
    CLOCK_SYNC_REQUEST(40), CLOCK_SYNC_RESPONSE(41),
    CONFIG_UPDATE(50), SETTINGS_REQUEST(51), SETTINGS_RESPONSE(52);

    companion object {
        fun fromValue(value: Byte): PacketType? = values().firstOrNull { it.value == value }
    }
}

@Serializable
enum SwingPhase(val value: Byte) {
    IDLE(0), BACKSWING(1), TRANSITION(2), DOWNSWING(3), CONTACT(4),
    FOLLOW_THROUGH(5), RECOVERY(6);

    companion object {
        fun fromValue(value: Byte): SwingPhase? = values().firstOrNull { it.value == value }
    }
}

sealed class ConnectionState {
    data class Disconnected(val reason: String? = null) : ConnectionState()
    object Discovering : ConnectionState()
    object Pairing : ConnectionState()
    object Connected : ConnectionState()
    object Calibrating : ConnectionState()
    object Active : ConnectionState()
    object Reconnecting : ConnectionState()
    data class Error(val message: String) : ConnectionState()
}

// MARK: - Packet Structures

@Serializable
data class PacketHeader(
    val magicNumber: Int = 0x43425048, // "CBPH"
    val version: Short = 1,
    val packetType: PacketType,
    val sequenceNumber: Short,
    val payloadSize: Int,
    val timestamp: Long, // Microseconds since epoch
    val serverTimestamp: Long = 0
)

@Serializable
data class HandshakePayload(
    val deviceId: String,
    val deviceName: String,
    val appVersion: String,
    val platform: String = "Android",
    val supportsAR: Boolean,
    val supportsBluetooth: Boolean,
    val supportedFeatures: List<String>
)

@Serializable
data class PairingPayload(
    val pairingCode: String,
    val deviceId: String,
    val playerName: String,
    val isLeftHanded: Boolean
)

@Serializable
data class SensorFramePayload(
    val timestamp: Long,
    val accelerometer: FloatArray, // [x, y, z]
    val gyroscope: FloatArray,
    val magnetometer: FloatArray,
    val orientation: FloatArray, // Quaternion [x, y, z, w]
    val gravity: FloatArray,
    val linearAcceleration: FloatArray,
    val confidence: Float
)

@Serializable
data class SwingPhasePayload(
    val timestamp: Long,
    val currentPhase: SwingPhase,
    val phaseProgress: Float,
    val batSpeed: Float,
    val maxBatSpeed: Float,
    val batVelocity: FloatArray,
    val batAngularVelocity: FloatArray,
    val confidence: Float
)

@Serializable
data class ContactPredictPayload(
    val timestamp: Long,
    val predictedContactTime: Long,
    val predictedContactPoint: FloatArray,
    val predictedBatVelocity: FloatArray,
    val predictedBatOrientation: FloatArray,
    val confidence: Float
)

@Serializable
data class SwingResultPayload(
    val contactTimestamp: Long,
    val shotType: String,
    val timingQuality: Float,
    val powerRating: Float,
    val controlRating: Float,
    val contactPoint: FloatArray,
    val batVelocityAtContact: FloatArray,
    val batOrientationAtContact: FloatArray,
    val wristRotationAtContact: Float,
    val swingFrames: List<SensorFramePayload>
)

@Serializable
data class TransformData(
    val position: FloatArray,
    val rotation: FloatArray, // Quaternion
    val scale: FloatArray
)

@Serializable
data class CalibrationPayload(
    val step: Int,
    val instruction: String,
    val phoneToBatTransform: TransformData,
    val batLength: FloatArray,
    val batCenterOfMass: FloatArray,
    val isComplete: Boolean,
    val qualityScore: Float
)

@Serializable
data class ClockSyncPayload(
    var clientSendTime: Long = 0,
    var serverReceiveTime: Long = 0,
    var serverSendTime: Long = 0,
    var clientReceiveTime: Long = 0,
    var roundTripTime: Int = 0,
    var clockOffset: Int = 0
)

@Serializable
data class ConfigPayload(
    val sensorRate: Float = 100.0f,
    val swingDetectionSensitivity: Float = 1.0f,
    val enableHaptics: Boolean = true,
    val enableARVisualization: Boolean = true,
    val jitterBufferSize: Int = 4,
    val maxLatencyCompensation: Float = 100.0f,
    val playerProfileId: String
)

@Serializable
data class PairingResponsePayload(
    val success: Boolean,
    val message: String,
    val sessionId: String?
)

@Serializable
data class HandshakeAckPayload(
    val serverTime: Long,
    val assignedId: String,
    val supportedFeatures: List<String>
)

// Generic packet wrapper
@Serializable
data class NetworkPacket<P: Any>(
    val header: PacketHeader,
    val payload: P
)

// Empty payload for simple packets
@Serializable
data class EmptyPayload()

// MARK: - Network Manager

class NetworkManager private constructor(
    private val scope: CoroutineScope,
    private val json: Json = Json { ignoreUnknownKeys = true }
) {
    // UDP
    private var udpChannel: DatagramChannel? = null
    private var serverAddress: InetSocketAddress? = null
    private val sequenceNumber = AtomicInteger(0)
    private val expectedSequence = AtomicInteger(0)
    private val jitterBuffer = ConcurrentHashMap<Short, ByteArray>()
    private val maxJitterBufferSize = 8

    // Bluetooth (placeholder - would use Android Bluetooth APIs)
    private var bluetoothConnected = false

    // Clock sync
    private val clockOffset = AtomicLong(0)
    private val rttSamples = mutableListOf<Int>()
    private var lastClockSyncTime = 0L
    private val clockSyncIntervalMs = 5000L

    // State
    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Disconnected())
    val connectionState: StateFlow<ConnectionState> = _connectionState

    private val _discoveredHosts = MutableStateFlow<List<InetSocketAddress>>(emptyList())
    val discoveredHosts: StateFlow<List<InetSocketAddress>> = _discoveredHosts

    private val _latencyMs = MutableStateFlow(0.0)
    val latencyMs: StateFlow<Double> = _latencyMs

    private val _jitterMs = MutableStateFlow(0.0)
    val jitterMs: StateFlow<Double> = _jitterMs

    private val _packetLoss = MutableStateFlow(0.0)
    val packetLoss: StateFlow<Double> = _packetLoss

    // Callbacks
    var onSensorFrame: ((SensorFramePayload) -> Unit)? = null
    var onSwingPhase: ((SwingPhasePayload) -> Unit)? = null
    var onContactPredict: ((ContactPredictPayload) -> Unit)? = null
    var onSwingResult: ((SwingResultPayload) -> Unit)? = null
    var onCalibrationData: ((CalibrationPayload) -> Unit)? = null
    var onClockSyncUpdate: ((Long, Int) -> Unit)? = null // offsetUs, rttUs
    var onConfigUpdate: ((ConfigPayload) -> Unit)? = null
    var onDiscoveredHost: ((InetSocketAddress) -> Unit)? = null

    // Discovery
    private var discoveryJob: Job? = null
    private var receiveJob: Job? = null
    private var clockSyncJob: Job? = null
    private var heartbeatJob: Job? = null

    companion object {
        @Volatile private var INSTANCE: NetworkManager? = null
        fun getInstance(scope: CoroutineScope = CoroutineScope(Dispatchers.Default)): NetworkManager {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: NetworkManager(scope).also { INSTANCE = it }
            }
        }
    }

    // MARK: - Public API

    fun startDiscovery(port: Int = 7777) {
        _connectionState.value = ConnectionState.Discovering()
        startUDPListener(port)
        startBonjourDiscovery(port)
        startBluetoothScan()
    }

    fun stopDiscovery() {
        discoveryJob?.cancel()
        discoveryJob = null
        stopUDPListener()
        stopBonjourDiscovery()
        stopBluetoothScan()

        if (_connectionState.value is ConnectionState.Discovering) {
            _connectionState.value = ConnectionState.Disconnected()
        }
    }

    fun connectToHost(host: String, port: Int = 7777, useBluetooth: Boolean = false) {
        serverAddress = InetSocketAddress(host, port)
        bluetoothConnected = useBluetooth

        if (useBluetooth) {
            connectBluetooth()
        } else {
            connectUDP()
        }
    }

    fun sendPairingRequest(code: String, playerName: String, isLeftHanded: Boolean) {
        val state = _connectionState.value
        if (state !is ConnectionState.Connected && state !is ConnectionState.Discovering) return

        _connectionState.value = ConnectionState.Pairing()

        val payload = PairingPayload(
            pairingCode = code,
            deviceId = getDeviceId(),
            playerName = playerName,
            isLeftHanded = isLeftHanded
        )

        sendPacket(PacketType.PAIRING_REQUEST, payload)
    }

    fun startCalibration(steps: Int) {
        if (_connectionState.value !is ConnectionState.Connected) return
        _connectionState.value = ConnectionState.Calibrating()
    }

    fun sendCalibrationStep(step: Int, transform: TransformData) {
        val payload = CalibrationPayload(
            step = step,
            instruction = "",
            phoneToBatTransform = transform,
            batLength = floatArrayOf(0f, 0f, 0.965f),
            batCenterOfMass = floatArrayOf(0f, 0f, 0.60f),
            isComplete = false,
            qualityScore = 0f
        )
        sendPacket(PacketType.CALIBRATION_DATA, payload)
    }

    fun sendSensorFrame(frame: SensorFramePayload) {
        sendPacket(PacketType.SENSOR_FRAME, frame)
    }

    fun sendSensorBatch(frames: List<SensorFramePayload>) {
        for (frame in frames) {
            sendSensorFrame(frame)
        }
    }

    fun sendSwingPhase(phase: SwingPhasePayload) {
        sendPacket(PacketType.SWING_PHASE_UPDATE, phase)
    }

    fun sendContactPrediction(prediction: ContactPredictPayload) {
        sendPacket(PacketType.SWING_CONTACT_PREDICT, prediction)
    }

    fun sendSwingResult(result: SwingResultPayload) {
        sendPacket(PacketType.SWING_RESULT, result)
    }

    fun requestClockSync() {
        val payload = ClockSyncPayload(
            clientSendTime = currentTimestampUs()
        )
        sendPacket(PacketType.CLOCK_SYNC_REQUEST, payload)
        lastClockSyncTime = System.currentTimeMillis()
    }

    fun disconnect() {
        sendPacket(PacketType.DISCONNECT, EmptyPayload())
        cleanupConnection()
        _connectionState.value = ConnectionState.Disconnected()
    }

    fun getServerTimestamp(): Long = currentTimestampUs() + clockOffset.get()

    // MARK: - UDP Implementation

    private fun startUDPListener(port: Int) {
        discoveryJob = scope.launch(Dispatchers.IO) {
            try {
                udpChannel = DatagramChannel.open().apply {
                    socket.bind(InetSocketAddress(port))
                    socket.setReuseAddress(true)
                    configureBlocking(false)
                }

                val buffer = ByteBuffer.allocateDirect(65536)

                while (isActive) {
                    buffer.clear()
                    val sender = udpChannel?.receive(buffer)
                    if (sender != null) {
                        buffer.flip()
                        val data = ByteArray(buffer.remaining())
                        buffer.get(data)
                        scope.launch { processReceivedData(data, sender as InetSocketAddress) }
                    } else {
                        delay(1) // Yield
                    }
                }
            } catch (e: Exception) {
                println("UDP listener error: $e")
                _connectionState.value = ConnectionState.Error("UDP listener failed: ${e.message}")
            }
        }
    }

    private fun stopUDPListener() {
        discoveryJob?.cancel()
        udpChannel?.close()
        udpChannel = null
    }

    private fun connectUDP() {
        scope.launch(Dispatchers.IO) {
            try {
                udpChannel?.close()
                udpChannel = DatagramChannel.open().apply {
                    configureBlocking(false)
                    socket.connect(serverAddress)
                }

                _connectionState.value = ConnectionState.Connected()
                sendHandshake()
                startReceiveLoop()
                startClockSyncTimer()
                startHeartbeat()
            } catch (e: Exception) {
                println("UDP connect error: $e")
                handleConnectionFailure()
            }
        }
    }

    private fun startReceiveLoop() {
        receiveJob = scope.launch(Dispatchers.IO) {
            val buffer = ByteBuffer.allocateDirect(65536)
            while (isActive) {
                buffer.clear()
                val read = udpChannel?.read(buffer)
                if (read != null && read > 0) {
                    buffer.flip()
                    val data = ByteArray(buffer.remaining())
                    buffer.get(data)
                    scope.launch { processReceivedData(data, serverAddress!!) }
                } else {
                    delay(1)
                }
            }
        }
    }

    private fun processReceivedData(data: ByteArray, from: InetSocketAddress) {
        try {
            val jsonString = String(data, Charsets.UTF_8)
            val genericPacket = json.decodeFromString<JsonObject>(jsonString)

            val packetType = PacketType.fromValue(genericPacket.getPrimitive("packetType")?.int?.toByte() ?: return)

            when (packetType) {
                PacketType.HANDSHAKE_ACK -> handleHandshakeAck(jsonString)
                PacketType.PAIRING_RESPONSE -> handlePairingResponse(jsonString)
                PacketType.CALIBRATION_START, PacketType.CALIBRATION_DATA, PacketType.CALIBRATION_COMPLETE -> handleCalibrationData(jsonString)
                PacketType.SENSOR_FRAME -> handleSensorFrame(jsonString)
                PacketType.SWING_PHASE_UPDATE -> handleSwingPhase(jsonString)
                PacketType.SWING_CONTACT_PREDICT -> handleContactPredict(jsonString)
                PacketType.SWING_RESULT -> handleSwingResult(jsonString)
                PacketType.CLOCK_SYNC_RESPONSE -> handleClockSyncResponse(jsonString)
                PacketType.CONFIG_UPDATE -> handleConfigUpdate(jsonString)
                PacketType.HEARTBEAT -> sendPacket(PacketType.HEARTBEAT, EmptyPayload())
                PacketType.DISCONNECT -> handleDisconnect()
                else -> println("Unknown packet type: $packetType")
            }

            manageJitterBuffer(genericPacket.getPrimitive("sequenceNumber")?.short ?: return)
        } catch (e: Exception) {
            println("Failed to decode packet: $e")
        }
    }

    private fun handleHandshakeAck(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<HandshakeAckPayload>>(jsonString)
        println("Handshake acknowledged by server")

        if (_connectionState.value is ConnectionState.Discovering) {
            _connectionState.value = ConnectionState.Connected()
            startClockSyncTimer()
        }
    }

    private fun handlePairingResponse(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<PairingResponsePayload>>(jsonString)
        if (packet.payload.success) {
            _connectionState.value = ConnectionState.Active()
            println("Pairing successful: ${packet.payload.message}")
        } else {
            _connectionState.value = ConnectionState.Error("Pairing failed: ${packet.payload.message}")
        }
    }

    private fun handleCalibrationData(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<CalibrationPayload>>(jsonString)
        onCalibrationData?.invoke(packet.payload)

        if (packet.payload.isComplete) {
            _connectionState.value = ConnectionState.Active()
        }
    }

    private fun handleSensorFrame(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<SensorFramePayload>>(jsonString)
        onSensorFrame?.invoke(packet.payload)
    }

    private fun handleSwingPhase(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<SwingPhasePayload>>(jsonString)
        onSwingPhase?.invoke(packet.payload)
    }

    private fun handleContactPredict(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<ContactPredictPayload>>(jsonString)
        onContactPredict?.invoke(packet.payload)
    }

    private fun handleSwingResult(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<SwingResultPayload>>(jsonString)
        onSwingResult?.invoke(packet.payload)
    }

    private fun handleClockSyncResponse(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<ClockSyncPayload>>(jsonString)
        val payload = packet.payload

        val clientReceiveTime = currentTimestampUs()
        val rtt = (clientReceiveTime - payload.clientSendTime).toInt()
        val offset = (payload.serverReceiveTime + payload.serverSendTime) / 2 - (clientReceiveTime - rtt / 2L)

        // Smooth clock offset
        clockOffset.updateAndGet { (it * 3 + offset) / 4 }
        rttSamples.add(rtt)
        if (rttSamples.size > 10) rttSamples.removeAt(0)

        val avgRtt = rttSamples.average().toInt()
        _latencyMs.value = avgRtt / 2000.0 // Half RTT in ms

        // Calculate jitter
        if (rttSamples.size > 1) {
            val variance = rttSamples.map { (it - avgRtt).toDouble().pow(2) }.average()
            _jitterMs.value = Math.sqrt(variance) / 1000.0
        }

        onClockSyncUpdate?.invoke(clockOffset.get(), avgRtt)
    }

    private fun handleConfigUpdate(jsonString: String) {
        val packet = json.decodeFromString<NetworkPacket<ConfigPayload>>(jsonString)
        onConfigUpdate?.invoke(packet.payload)
    }

    private fun handleDisconnect() {
        cleanupConnection()
        _connectionState.value = ConnectionState.Disconnected()
    }

    // MARK: - Sending Packets

    private fun sendHandshake() {
        val payload = HandshakePayload(
            deviceId = getDeviceId(),
            deviceName = android.os.Build.MODEL,
            appVersion = BuildConfig.VERSION_NAME,
            supportsAR = checkARSupport(),
            supportsBluetooth = true,
            supportedFeatures = listOf("sensor_fusion", "swing_detection", "ar_tracking", "haptics")
        )
        sendPacket(PacketType.HANDSHAKE, payload)
    }

    private fun <P: Any> sendPacket(type: PacketType, payload: P) {
        scope.launch(Dispatchers.IO) {
            val channel = udpChannel ?: return@launch
            if (!channel.isOpen) return@launch

            val header = PacketHeader(
                packetType = type,
                sequenceNumber = sequenceNumber.getAndIncrement().toShort(),
                payloadSize = 0, // Filled after encoding
                timestamp = currentTimestampUs()
            )

            try {
                val packet = NetworkPacket(header, payload)
                val payloadJson = json.encodeToString(payload)
                val payloadBytes = payloadJson.toByteArray(Charsets.UTF_8)

                val fullPacket = packet.copy(header = header.copy(payloadSize = payloadBytes.size))
                val fullJson = json.encodeToString(fullPacket)
                val fullBytes = fullJson.toByteArray(Charsets.UTF_8)

                val buffer = ByteBuffer.wrap(fullBytes)
                channel.write(buffer)
            } catch (e: Exception) {
                println("Send error: $e")
            }
        }
    }

    // MARK: - Clock Sync

    private fun startClockSyncTimer() {
        clockSyncJob?.cancel()
        clockSyncJob = scope.launch {
            while (isActive) {
                delay(clockSyncIntervalMs)
                requestClockSync()
            }
        }
    }

    private fun currentTimestampUs(): Long = System.currentTimeMillis() * 1000

    // MARK: - Jitter Buffer

    private fun manageJitterBuffer(sequence: Short) {
        // Simplified - in practice would buffer out-of-order packets
        expectedSequence.updateAndGet { it + 1 }

        // Calculate packet loss
        val expected = expectedSequence.get()
        val received = jitterBuffer.size
        if (expected > 0) {
            _packetLoss.value = (expected - received).toDouble() / expected
        }
    }

    // MARK: - Bonjour/MDNS Discovery (simplified)

    private fun startBonjourDiscovery(port: Int) {
        // Would use JmDns or similar for service discovery
        // For now, placeholder
    }

    private fun stopBonjourDiscovery() {
        // Stop MDNS browser
    }

    // MARK: - Bluetooth (Placeholder)

    private fun startBluetoothScan() {
        // Would use Android Bluetooth APIs
        // BluetoothAdapter.startLeScan()
    }

    private fun stopBluetoothScan() {
        // BluetoothAdapter.stopLeScan()
    }

    private fun connectBluetooth() {
        bluetoothConnected = true
        // Connect to paired device
    }

    // MARK: - Connection Management

    private fun handleConnectionFailure() {
        if (bluetoothConnected) {
            bluetoothConnected = false
            connectUDP()
        } else {
            _connectionState.value = ConnectionState.Error("Connection failed")
            attemptReconnect()
        }
    }

    private fun attemptReconnect() {
        _connectionState.value = ConnectionState.Reconnecting()
        scope.launch {
            delay(2000)
            connectUDP()
        }
    }

    private fun cleanupConnection() {
        receiveJob?.cancel()
        clockSyncJob?.cancel()
        heartbeatJob?.cancel()
        udpChannel?.close()
        udpChannel = null
        jitterBuffer.clear()
        expectedSequence.set(0)
        rttSamples.clear()
    }

    private fun startHeartbeat() {
        heartbeatJob = scope.launch {
            while (isActive) {
                delay(2000)
                sendPacket(PacketType.HEARTBEAT, EmptyPayload())
            }
        }
    }

    // MARK: - Helpers

    private fun getDeviceId(): String {
        return android.provider.Settings.Secure.getString(
            android.app.Application.getApplication<android.app.Application>().contentResolver,
            android.provider.Settings.Secure.ANDROID_ID
        ) ?: "unknown_device"
    }

    private fun checkARSupport(): Boolean {
        // Check ARCore support
        return com.google.ar.core.ArCoreApk.getInstance()
            .checkAvailability(android.app.Application.getApplication<android.app.Application>())
            .isSupported
    }
}

// MARK: - Helper Extensions

// BuildConfig placeholder
object BuildConfig {
    const val VERSION_NAME = "1.0"
}