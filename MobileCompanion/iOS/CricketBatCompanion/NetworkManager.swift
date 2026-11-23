//
//  NetworkManager.swift
//  CricketBatCompanion
//
//  Handles UDP and Bluetooth LE communication with Unreal Engine
//

import Foundation
import Network
import CoreBluetooth
import Combine

// MARK: - Network Protocol Types

enum PacketType: UInt8, Codable {
    // Connection
    case handshake = 0
    case handshakeAck = 1
    case pairingRequest = 2
    case pairingResponse = 3
    case disconnect = 4
    case heartbeat = 5

    // Calibration
    case calibrationStart = 10
    case calibrationData = 11
    case calibrationComplete = 12

    // Sensor data
    case sensorFrame = 20
    case sensorBatch = 21

    // Swing analysis
    case swingPhaseUpdate = 30
    case swingContactPredict = 31
    case swingResult = 32

    // Clock sync
    case clockSyncRequest = 40
    case clockSyncResponse = 41

    // Configuration
    case configUpdate = 50
    case settingsRequest = 51
    case settingsResponse = 52
}

enum SwingPhase: UInt8, Codable {
    case idle = 0
    case backswing = 1
    case transition = 2
    case downswing = 3
    case contact = 4
    case followThrough = 5
    case recovery = 6
}

enum ConnectionState: Equatable {
    case disconnected
    case discovering
    case pairing
    case connected
    case calibrating
    case active
    case reconnecting
    case error(String)
}

// MARK: - Packet Structures

struct PacketHeader: Codable {
    let magicNumber: UInt32 = 0x43425048 // "CBPH"
    let version: UInt16 = 1
    let packetType: PacketType
    let sequenceNumber: UInt16
    let payloadSize: UInt32
    let timestamp: UInt64 // Microseconds since epoch
    var serverTimestamp: UInt64 = 0
}

struct HandshakePayload: Codable {
    let deviceId: String
    let deviceName: String
    let appVersion: String
    let platform: String = "iOS"
    let supportsAR: Bool
    let supportsBluetooth: Bool
    let supportedFeatures: [String]
}

struct PairingPayload: Codable {
    let pairingCode: String
    let deviceId: String
    let playerName: String
    let isLeftHanded: Bool
}

struct SensorFramePayload: Codable {
    let timestamp: UInt64
    let accelerometer: SIMD3<Float>
    let gyroscope: SIMD3<Float>
    let magnetometer: SIMD3<Float>
    let orientation: SIMD4<Float> // Quaternion (x, y, z, w)
    let gravity: SIMD3<Float>
    let linearAcceleration: SIMD3<Float>
    let confidence: Float

    // ========================================================================
    // Latency Instrumentation Timestamps (T0-T1 from phone side)
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    let phoneSensorReadTimeUs: UInt64
    // T1: Phone sensor fusion complete time
    let phoneFusionCompleteTimeUs: UInt64
    // T2: Phone serialization/send time
    let phoneSendTimeUs: UInt64
}

struct SwingPhasePayload: Codable {
    let timestamp: UInt64
    let currentPhase: SwingPhase
    let phaseProgress: Float
    let batSpeed: Float
    let maxBatSpeed: Float
    let batVelocity: SIMD3<Float>
    let batAngularVelocity: SIMD3<Float>
    let confidence: Float

    // ========================================================================
    // Latency Instrumentation Timestamps
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    let phoneSensorReadTimeUs: UInt64
    // T1: Phone sensor fusion complete time
    let phoneFusionCompleteTimeUs: UInt64
    // T2: Phone serialization/send time
    let phoneSendTimeUs: UInt64
}

struct ContactPredictPayload: Codable {
    let timestamp: UInt64
    let predictedContactTime: UInt64
    let predictedContactPoint: SIMD3<Float>
    let predictedBatVelocity: SIMD3<Float>
    let predictedBatOrientation: SIMD4<Float>
    let confidence: Float

    // ========================================================================
    // Latency Instrumentation Timestamps
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    let phoneSensorReadTimeUs: UInt64
    // T1: Phone sensor fusion complete time
    let phoneFusionCompleteTimeUs: UInt64
    // T2: Phone serialization/send time
    let phoneSendTimeUs: UInt64
}

struct SwingResultPayload: Codable {
    let contactTimestamp: UInt64
    let shotType: String
    let timingQuality: Float
    let powerRating: Float
    let controlRating: Float
    let contactPoint: SIMD3<Float>
    let batVelocityAtContact: SIMD3<Float>
    let batOrientationAtContact: SIMD4<Float>
    let wristRotationAtContact: Float
    let swingFrames: [SensorFramePayload]

    // ========================================================================
    // Latency Instrumentation Timestamps (Phone side)
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    let phoneSensorReadTimeUs: UInt64
    // T1: Phone sensor fusion complete time (microseconds since epoch)
    let phoneFusionCompleteTimeUs: UInt64
    // T2: Phone serialization/send time (microseconds since epoch)
    let phoneSendTimeUs: UInt64
}

struct CalibrationPayload: Codable {
    let step: Int
    let instruction: String
    let phoneToBatTransform: TransformData
    let batLength: SIMD3<Float>
    let batCenterOfMass: SIMD3<Float>
    let isComplete: Bool
    let qualityScore: Float
}

struct TransformData: Codable {
    let position: SIMD3<Float>
    let rotation: SIMD4<Float> // Quaternion
    let scale: SIMD3<Float>
}

struct ClockSyncPayload: Codable {
    var clientSendTime: UInt64 = 0
    var serverReceiveTime: UInt64 = 0
    var serverSendTime: UInt64 = 0
    var clientReceiveTime: UInt64 = 0
    var roundTripTime: Int32 = 0
    var clockOffset: Int32 = 0

    // ========================================================================
    // Latency Instrumentation Timestamps (for cross-reference)
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    var phoneSensorReadTimeUs: UInt64 = 0
    // T1: Phone sensor fusion complete time
    var phoneFusionCompleteTimeUs: UInt64 = 0
    // T2: Phone serialization/send time
    var phoneSendTimeUs: UInt64 = 0
}

struct ConfigPayload: Codable {
    let sensorRate: Float = 100.0
    let swingDetectionSensitivity: Float = 1.0
    let enableHaptics: Bool = true
    let enableARVisualization: Bool = true
    let jitterBufferSize: Int32 = 4
    let maxLatencyCompensation: Float = 100.0
    let playerProfileId: String
}

// MARK: - Generic Packet

struct NetworkPacket<Payload: Codable>: Codable {
    let header: PacketHeader
    let payload: Payload
}

// MARK: - Network Manager

@MainActor
class NetworkManager: ObservableObject {
    static let shared = NetworkManager()

    // Published state
    @Published var connectionState: ConnectionState = .disconnected
    @Published var discoveredHosts: [NWBrowser.Result] = []
    @Published var latencyMs: Double = 0
    @Published var jitterMs: Double = 0
    @Published var packetLoss: Double = 0
    @Published var isUsingBluetooth: Bool = false

    // UDP
    private var udpConnection: NWConnection?
    private var udpListener: NWListener?
    private let udpQueue = DispatchQueue(label: "com.cricketsimulator.udp", qos: .userInteractive)
    private var sequenceNumber: UInt16 = 0
    private var hostEndpoint: NWEndpoint.Host?
    private var hostPort: NWEndpoint.Port = 7777

    // Bluetooth LE
    private var bluetoothManager: BluetoothManager?
    private var bluetoothPeripheral: CBPeripheral?
    private let bluetoothQueue = DispatchQueue(label: "com.cricketsimulator.bluetooth", qos: .userInteractive)

    // Clock sync
    private var clockOffset: Int64 = 0 // microseconds (server - client)
    private var rttSamples: [Int] = []
    private var lastClockSyncTime: Date = .distantPast
    private let clockSyncInterval: TimeInterval = 5.0 // seconds

    // Jitter buffer
    private var jitterBuffer: [UInt16: Data] = [:]
    private var expectedSequence: UInt16 = 0
    private let maxJitterBufferSize = 8

    // Callbacks
    var onSensorFrame: ((SensorFramePayload) -> Void)?
    var onSwingPhase: ((SwingPhasePayload) -> Void)?
    var onContactPredict: ((ContactPredictPayload) -> Void)?
    var onSwingResult: ((SwingResultPayload) -> Void)?
    var onCalibrationData: ((CalibrationPayload) -> Void)?
    var onConnectionStateChange: ((ConnectionState) -> Void)?
    var onClockSyncUpdate: ((Int64, Int) -> Void)? // offsetUs, rttUs
    var onConfigUpdate: ((ConfigPayload) -> Void)?
    var onDiscoveredHost: ((NWBrowser.Result) -> Void)?

    private init() {
        setupBluetooth()
    }

    // MARK: - Public API

    func startDiscovery(port: UInt16 = 7777) {
        hostPort = NWEndpoint.Port(integerLiteral: port)
        connectionState = .discovering
        onConnectionStateChange?(.discovering)

        // Start UDP listener for discovery responses
        startUDPListener(port: port)

        // Start Bonjour browser for automatic discovery
        startBonjourBrowser()

        // Also scan for Bluetooth peripherals
        startBluetoothScan()
    }

    func stopDiscovery() {
        udpListener?.cancel()
        udpListener = nil
        stopBonjourBrowser()
        stopBluetoothScan()

        if connectionState == .discovering {
            connectionState = .disconnected
            onConnectionStateChange?(.disconnected)
        }
    }

    func connectToHost(_ host: NWEndpoint.Host, port: UInt16 = 7777, useBluetooth: Bool = false) {
        hostEndpoint = host
        hostPort = NWEndpoint.Port(integerLiteral: port)
        isUsingBluetooth = useBluetooth

        if useBluetooth {
            connectBluetooth()
        } else {
            connectUDP()
        }
    }

    func sendPairingRequest(code: String, playerName: String, isLeftHanded: Bool) {
        guard connectionState == .connected || connectionState == .discovering else { return }

        connectionState = .pairing
        onConnectionStateChange?(.pairing)

        let payload = PairingPayload(
            pairingCode: code,
            deviceId: getDeviceId(),
            playerName: playerName,
            isLeftHanded: isLeftHanded
        )

        sendPacket(type: .pairingRequest, payload: payload)
    }

    func startCalibration(steps: Int) {
        guard connectionState == .connected else { return }

        connectionState = .calibrating
        onConnectionStateChange?(.calibrating)

        // Phone will receive calibration steps from server
    }

    func sendCalibrationStep(step: Int, transform: TransformData) {
        let payload = CalibrationPayload(
            step: step,
            instruction: "",
            phoneToBatTransform: transform,
            batLength: SIMD3<Float>(0, 0, 0.965),
            batCenterOfMass: SIMD3<Float>(0, 0, 0.60),
            isComplete: false,
            qualityScore: 0
        )

        sendPacket(type: .calibrationData, payload: payload)
    }

    func sendSensorFrame(_ frame: SensorFramePayload) {
        // Update T2 (send time) just before transmission
        var updatedFrame = frame
        updatedFrame = SensorFramePayload(
            timestamp: frame.timestamp,
            accelerometer: frame.accelerometer,
            gyroscope: frame.gyroscope,
            magnetometer: frame.magnetometer,
            orientation: frame.orientation,
            gravity: frame.gravity,
            linearAcceleration: frame.linearAcceleration,
            confidence: frame.confidence,
            phoneSensorReadTimeUs: frame.phoneSensorReadTimeUs,
            phoneFusionCompleteTimeUs: frame.phoneFusionCompleteTimeUs,
            phoneSendTimeUs: currentTimestampUs() // T2: actual send time
        )
        sendPacket(type: .sensorFrame, payload: updatedFrame)
    }

    func sendSensorBatch(_ frames: [SensorFramePayload]) {
        // For batch sending, we'd need a batch payload type
        // For now, send individually
        for frame in frames {
            sendSensorFrame(frame)
        }
    }

    func sendSwingPhase(_ phase: SwingPhasePayload) {
        var updatedPhase = phase
        updatedPhase = SwingPhasePayload(
            timestamp: phase.timestamp,
            currentPhase: phase.currentPhase,
            phaseProgress: phase.phaseProgress,
            batSpeed: phase.batSpeed,
            maxBatSpeed: phase.maxBatSpeed,
            batVelocity: phase.batVelocity,
            batAngularVelocity: phase.batAngularVelocity,
            confidence: phase.confidence,
            phoneSensorReadTimeUs: phase.phoneSensorReadTimeUs,
            phoneFusionCompleteTimeUs: phase.phoneFusionCompleteTimeUs,
            phoneSendTimeUs: currentTimestampUs() // T2: actual send time
        )
        sendPacket(type: .swingPhaseUpdate, payload: updatedPhase)
    }

    func sendContactPrediction(_ prediction: ContactPredictPayload) {
        var updatedPrediction = prediction
        updatedPrediction = ContactPredictPayload(
            timestamp: prediction.timestamp,
            predictedContactTime: prediction.predictedContactTime,
            predictedContactPoint: prediction.predictedContactPoint,
            predictedBatVelocity: prediction.predictedBatVelocity,
            predictedBatOrientation: prediction.predictedBatOrientation,
            confidence: prediction.confidence,
            phoneSensorReadTimeUs: prediction.phoneSensorReadTimeUs,
            phoneFusionCompleteTimeUs: prediction.phoneFusionCompleteTimeUs,
            phoneSendTimeUs: currentTimestampUs() // T2: actual send time
        )
        sendPacket(type: .swingContactPredict, payload: updatedPrediction)
    }

    func sendSwingResult(_ result: SwingResultPayload) {
        // Update T2 (send time) just before transmission
        var updatedResult = result
        updatedResult = SwingResultPayload(
            contactTimestamp: result.contactTimestamp,
            shotType: result.shotType,
            timingQuality: result.timingQuality,
            powerRating: result.powerRating,
            controlRating: result.controlRating,
            contactPoint: result.contactPoint,
            batVelocityAtContact: result.batVelocityAtContact,
            batOrientationAtContact: result.batOrientationAtContact,
            wristRotationAtContact: result.wristRotationAtContact,
            swingFrames: result.swingFrames,
            phoneSensorReadTimeUs: result.phoneSensorReadTimeUs,
            phoneFusionCompleteTimeUs: result.phoneFusionCompleteTimeUs,
            phoneSendTimeUs: currentTimestampUs() // T2: actual send time
        )
        sendPacket(type: .swingResult, payload: updatedResult)
    }

    func requestClockSync() {
        let payload = ClockSyncPayload(
            clientSendTime: currentTimestampUs(),
            serverReceiveTime: 0,
            serverSendTime: 0,
            clientReceiveTime: 0,
            roundTripTime: 0,
            clockOffset: 0
        )

        sendPacket(type: .clockSyncRequest, payload: payload)
        lastClockSyncTime = Date()
    }

    func disconnect() {
        let payload = EmptyPayload()
        sendPacket(type: .disconnect, payload: payload)

        cleanupConnection()
        connectionState = .disconnected
        onConnectionStateChange?(.disconnected)
    }

    // MARK: - UDP Implementation

    private func startUDPListener(port: UInt16) {
        let params = NWParameters.udp
        params.allowLocalEndpointReuse = true

        do {
            udpListener = try NWListener(using: params, on: NWEndpoint.Port(integerLiteral: port))
            udpListener?.stateUpdateHandler = { [weak self] state in
                Task { @MainActor in
                    self?.handleListenerStateChange(state)
                }
            }
            udpListener?.newConnectionHandler = { [weak self] connection in
                self?.handleNewUDPConnection(connection)
            }
            udpListener?.start(queue: udpQueue)
        } catch {
            print("Failed to start UDP listener: \(error)")
            connectionState = .error("UDP listener failed: \(error.localizedDescription)")
            onConnectionStateChange?(connectionState)
        }
    }

    private func connectUDP() {
        guard let host = hostEndpoint else { return }

        let params = NWParameters.udp
        let endpoint = NWEndpoint.hostPort(host: host, port: hostPort)

        udpConnection = NWConnection(to: endpoint, using: params)
        udpConnection?.stateUpdateHandler = { [weak self] state in
            Task { @MainActor in
                self?.handleConnectionStateChange(state)
            }
        }
        udpConnection?.start(queue: udpQueue)

        // Start receiving
        receiveUDP()

        // Send handshake
        sendHandshake()
    }

    private func handleNewUDPConnection(_ connection: NWConnection) {
        connection.stateUpdateHandler = { [weak self] state in
            Task { @MainActor in
                self?.handleConnectionStateChange(state)
            }
        }
        connection.start(queue: udpQueue)
        self.udpConnection = connection
        receiveUDP(on: connection)
        sendHandshake(on: connection)
    }

    private func receiveUDP(on connection: NWConnection? = nil) {
        let conn = connection ?? udpConnection
        conn?.receive(minimumIncompleteLength: 1, maximumLength: 65536) { [weak self] data, context, isComplete, error in
            guard let self = self else { return }

            if let error = error {
                print("UDP receive error: \(error)")
                return
            }

            if let data = data, !data.isEmpty {
                Task { @MainActor in
                    self.processReceivedData(data)
                }
            }

            if !isComplete {
                self.receiveUDP(on: conn)
            }
        }
    }

    private func processReceivedData(_ data: Data) {
        do {
            // Try to decode as generic packet first to get type
            let decoder = JSONDecoder()
            let genericPacket = try decoder.decode(GenericPacket.self, from: data)

            // Handle based on packet type
            switch genericPacket.header.packetType {
            case .handshakeAck:
                handleHandshakeAck(data)
            case .pairingResponse:
                handlePairingResponse(data)
            case .calibrationStart, .calibrationData, .calibrationComplete:
                handleCalibrationData(data)
            case .sensorFrame:
                handleSensorFrame(data)
            case .swingPhaseUpdate:
                handleSwingPhase(data)
            case .swingContactPredict:
                handleContactPredict(data)
            case .swingResult:
                handleSwingResult(data)
            case .clockSyncResponse:
                handleClockSyncResponse(data)
            case .configUpdate:
                handleConfigUpdate(data)
            case .heartbeat:
                // Respond to heartbeat
                sendPacket(type: .heartbeat, payload: EmptyPayload())
            case .disconnect:
                handleDisconnect()
            default:
                print("Unknown packet type: \(genericPacket.header.packetType)")
            }

            // Manage jitter buffer
            manageJitterBuffer(sequence: genericPacket.header.sequenceNumber, data: data)

        } catch {
            print("Failed to decode packet: \(error)")
        }
    }

    // Generic packet for type detection
    private struct GenericPacket: Codable {
        let header: PacketHeader
        let payload: JSONValue
    }

    private enum JSONValue: Codable {
        case string(String)
        case number(Double)
        case bool(Bool)
        case object([String: JSONValue])
        case array([JSONValue])
        case null

        init(from decoder: Decoder) throws {
            let container = try decoder.singleValueContainer()
            if let string = try? container.decode(String.self) { self = .string(string) }
            else if let number = try? container.decode(Double.self) { self = .number(number) }
            else if let bool = try? container.decode(Bool.self) { self = .bool(bool) }
            else if let object = try? container.decode([String: JSONValue].self) { self = .object(object) }
            else if let array = try? container.decode([JSONValue].self) { self = .array(array) }
            else { self = .null }
        }

        func encode(to encoder: Encoder) throws {
            var container = encoder.singleValueContainer()
            switch self {
            case .string(let s): try container.encode(s)
            case .number(let n): try container.encode(n)
            case .bool(let b): try container.encode(b)
            case .object(let o): try container.encode(o)
            case .array(let a): try container.encode(a)
            case .null: try container.encodeNil()
            }
        }
    }

    // MARK: - Packet Handlers

    private func handleHandshakeAck(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<HandshakeAckPayload>.self, from: data)
            print("Handshake acknowledged by server")

            if connectionState == .discovering {
                connectionState = .connected
                onConnectionStateChange?(.connected)

                // Start clock sync
                startClockSyncTimer()
            }
        } catch {
            print("Failed to decode handshake ack: \(error)")
        }
    }

    private struct HandshakeAckPayload: Codable {
        let serverTime: UInt64
        let assignedId: String
        let supportedFeatures: [String]
    }

    private func handlePairingResponse(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<PairingResponsePayload>.self, from: data)

            if packet.payload.success {
                connectionState = .active
                onConnectionStateChange?(.active)
                print("Pairing successful: \(packet.payload.message)")
            } else {
                connectionState = .error("Pairing failed: \(packet.payload.message)")
                onConnectionStateChange?(connectionState)
            }
        } catch {
            print("Failed to decode pairing response: \(error)")
        }
    }

    private struct PairingResponsePayload: Codable {
        let success: Bool
        let message: String
        let sessionId: String?
    }

    private func handleCalibrationData(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<CalibrationPayload>.self, from: data)
            onCalibrationData?(packet.payload)

            if packet.payload.isComplete {
                connectionState = .active
                onConnectionStateChange?(.active)
            }
        } catch {
            print("Failed to decode calibration data: \(error)")
        }
    }

    private func handleSensorFrame(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<SensorFramePayload>.self, from: data)
            onSensorFrame?(packet.payload)
        } catch {
            print("Failed to decode sensor frame: \(error)")
        }
    }

    private func handleSwingPhase(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<SwingPhasePayload>.self, from: data)
            onSwingPhase?(packet.payload)
        } catch {
            print("Failed to decode swing phase: \(error)")
        }
    }

    private func handleContactPredict(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<ContactPredictPayload>.self, from: data)
            onContactPredict?(packet.payload)
        } catch {
            print("Failed to decode contact prediction: \(error)")
        }
    }

    private func handleSwingResult(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<SwingResultPayload>.self, from: data)
            onSwingResult?(packet.payload)
        } catch {
            print("Failed to decode swing result: \(error)")
        }
    }

    private func handleClockSyncResponse(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<ClockSyncPayload>.self, from: data)
            let payload = packet.payload

            let clientReceiveTime = currentTimestampUs()
            let rtt = Int(clientReceiveTime - payload.clientSendTime)
            let offset = Int64(payload.serverReceiveTime + payload.serverSendTime) / 2 - Int64(clientReceiveTime - rtt / 2)

            // Update clock offset with smoothing
            clockOffset = (clockOffset * 3 + offset) / 4
            rttSamples.append(rtt)
            if rttSamples.count > 10 { rttSamples.removeFirst() }

            let avgRtt = rttSamples.reduce(0, +) / rttSamples.count
            latencyMs = Double(avgRtt) / 2000.0 // Convert to ms (half RTT)

            // Calculate jitter
            if rttSamples.count > 1 {
                let variance = rttSamples.map { Double($0 - avgRtt) * Double($0 - avgRtt) }.reduce(0, +) / Double(rttSamples.count)
                jitterMs = sqrt(variance) / 1000.0 // Convert to ms
            }

            onClockSyncUpdate?(clockOffset, avgRtt)
        } catch {
            print("Failed to decode clock sync: \(error)")
        }
    }

    private func handleConfigUpdate(_ data: Data) {
        do {
            let packet = try JSONDecoder().decode(NetworkPacket<ConfigPayload>.self, from: data)
            onConfigUpdate?(packet.payload)
        } catch {
            print("Failed to decode config update: \(error)")
        }
    }

    private func handleDisconnect() {
        cleanupConnection()
        connectionState = .disconnected
        onConnectionStateChange?(.disconnected)
    }

    // MARK: - Sending Packets

    private func sendHandshake(on connection: NWConnection? = nil) {
        let payload = HandshakePayload(
            deviceId: getDeviceId(),
            deviceName: UIDevice.current.name,
            appVersion: Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0",
            supportsAR: ARWorldTrackingConfiguration.isSupported,
            supportsBluetooth: true,
            supportedFeatures: ["sensor_fusion", "swing_detection", "ar_tracking", "haptics"]
        )

        sendPacket(type: .handshake, payload: payload, on: connection)
    }

    private func sendPacket<Payload: Codable>(type: PacketType, payload: Payload, on connection: NWConnection? = nil) {
        let conn = connection ?? udpConnection
        guard conn?.state == .ready else { return }

        let header = PacketHeader(
            packetType: type,
            sequenceNumber: nextSequenceNumber(),
            payloadSize: 0, // Will be set after encoding
            timestamp: currentTimestampUs()
        )

        do {
            var packet = NetworkPacket(header: header, payload: payload)
            let encoder = JSONEncoder()
            let payloadData = try encoder.encode(payload)

            // Update header with actual payload size
            packet.header.payloadSize = UInt32(payloadData.count)

            let fullData = try encoder.encode(packet)

            conn?.send(content: fullData, completion: .contentProcessed { error in
                if let error = error {
                    print("Send error: \(error)")
                }
            })
        } catch {
            print("Failed to encode packet: \(error)")
        }
    }

    private struct EmptyPayload: Codable {}

    // MARK: - Clock Sync

    private func startClockSyncTimer() {
        Timer.scheduledTimer(withTimeInterval: clockSyncInterval, repeats: true) { [weak self] _ in
            self?.requestClockSync()
        }
    }

    private func currentTimestampUs() -> UInt64 {
        return UInt64(Date().timeIntervalSince1970 * 1_000_000)
    }

    func getServerTimestamp() -> UInt64 {
        return currentTimestampUs() + UInt64(clockOffset)
    }

    // MARK: - Jitter Buffer

    private func manageJitterBuffer(sequence: UInt16, data: Data) {
        jitterBuffer[sequence] = data

        // Remove old entries
        while jitterBuffer.count > maxJitterBufferSize {
            if let oldestKey = jitterBuffer.keys.min() {
                jitterBuffer.removeValue(forKey: oldestKey)
            }
        }

        // Process in-order packets
        while let nextData = jitterBuffer[expectedSequence] {
            jitterBuffer.removeValue(forKey: expectedSequence)
            expectedSequence = expectedSequence &+ 1
            // Process nextData here if needed for ordered delivery
        }

        // Calculate packet loss
        let received = UInt16(jitterBuffer.count)
        let expected = expectedSequence
        if expected > 0 {
            packetLoss = Double(expected - received) / Double(expected)
        }
    }

    // MARK: - Bonjour Discovery

    private var bonjourBrowser: NWBrowser?

    private func startBonjourBrowser() {
        let parameters = NWParameters.udp
        let browser = NWBrowser(for: .bonjour(type: "_cricketbat._udp", domain: nil), using: parameters)

        browser.browseResultsChangedHandler = { [weak self] results, changes in
            Task { @MainActor in
                self?.handleBonjourResults(results, changes: changes)
            }
        }

        browser.stateUpdateHandler = { [weak self] state in
            Task { @MainActor in
                self?.handleBrowserStateChange(state)
            }
        }

        browser.start(queue: udpQueue)
        bonjourBrowser = browser
    }

    private func stopBonjourBrowser() {
        bonjourBrowser?.cancel()
        bonjourBrowser = nil
    }

    private func handleBonjourResults(_ results: Set<NWBrowser.Result>, changes: [NWBrowser.Result.Change]) {
        for change in changes {
            switch change {
            case .added(let result):
                discoveredHosts.append(result)
                onDiscoveredHost?(result)
            case .removed(let result):
                discoveredHosts.removeAll { $0 == result }
            case .changed(let result, _):
                if let index = discoveredHosts.firstIndex(where: { $0 == result }) {
                    discoveredHosts[index] = result
                }
            @unknown default:
                break
            }
        }
    }

    private func handleBrowserStateChange(_ state: NWBrowser.State) {
        switch state {
        case .failed(let error):
            print("Bonjour browser failed: \(error)")
        case .cancelled:
            print("Bonjour browser cancelled")
        default:
            break
        }
    }

    // MARK: - Bluetooth LE

    private func setupBluetooth() {
        bluetoothManager = BluetoothManager()
        bluetoothManager?.delegate = self
    }

    private func startBluetoothScan() {
        bluetoothManager?.startScan()
    }

    private func stopBluetoothScan() {
        bluetoothManager?.stopScan()
    }

    private func connectBluetooth() {
        guard let peripheral = bluetoothPeripheral else { return }
        bluetoothManager?.connect(peripheral)
    }

    // MARK: - Connection State Handling

    private func handleListenerStateChange(_ state: NWListener.State) {
        switch state {
        case .ready:
            print("UDP listener ready")
        case .failed(let error):
            print("UDP listener failed: \(error)")
            connectionState = .error("Listener failed: \(error.localizedDescription)")
            onConnectionStateChange?(connectionState)
        case .cancelled:
            print("UDP listener cancelled")
        default:
            break
        }
    }

    private func handleConnectionStateChange(_ state: NWConnection.State) {
        switch state {
        case .ready:
            print("UDP connection ready")
            if connectionState == .discovering || connectionState == .reconnecting {
                connectionState = .connected
                onConnectionStateChange?(.connected)
                startClockSyncTimer()
            }
        case .waiting(let error):
            print("UDP connection waiting: \(error)")
            connectionState = .reconnecting
            onConnectionStateChange?(.reconnecting)
        case .failed(let error):
            print("UDP connection failed: \(error)")
            handleConnectionFailure()
        case .cancelled:
            print("UDP connection cancelled")
            cleanupConnection()
        default:
            break
        }
    }

    private func handleConnectionFailure() {
        if isUsingBluetooth {
            // Try fallback to UDP
            isUsingBluetooth = false
            connectUDP()
        } else {
            connectionState = .error("Connection failed")
            onConnectionStateChange?(connectionState)

            // Attempt reconnection
            attemptReconnect()
        }
    }

    private func attemptReconnect() {
        connectionState = .reconnecting
        onConnectionStateChange?(.reconnecting)

        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) { [weak self] in
            self?.connectUDP()
        }
    }

    private func cleanupConnection() {
        udpConnection?.cancel()
        udpConnection = nil
        udpListener?.cancel()
        udpListener = nil
        stopBonjourBrowser()
        stopBluetoothScan()
        stopBluetooth()

        jitterBuffer.removeAll()
        expectedSequence = 0
        rttSamples.removeAll()
    }

    private func stopBluetooth() {
        bluetoothManager?.disconnect()
        bluetoothPeripheral = nil
    }

    // MARK: - Helpers

    private func nextSequenceNumber() -> UInt16 {
        defer { sequenceNumber = sequenceNumber &+ 1 }
        return sequenceNumber
    }

    private func getDeviceId() -> String {
        if let uuid = UIDevice.current.identifierForVendor?.uuidString {
            return uuid
        }
        return "unknown_device"
    }
}

// MARK: - Bluetooth Manager

protocol BluetoothManagerDelegate: AnyObject {
    func didDiscoverPeripheral(_ peripheral: CBPeripheral, rssi: NSNumber)
    func didConnectPeripheral(_ peripheral: CBPeripheral)
    func didDisconnectPeripheral(_ peripheral: CBPeripheral, error: Error?)
    func didReceiveData(_ data: Data, from characteristic: CBCharacteristic)
}

class BluetoothManager: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private var centralManager: CBCentralManager!
    private var discoveredPeripherals: [CBPeripheral] = []
    private var targetPeripheral: CBPeripheral?
    private var writeCharacteristic: CBCharacteristic?
    private var notifyCharacteristic: CBCharacteristic?

    weak var delegate: BluetoothManagerDelegate?

    private let serviceUUID = CBUUID(string: "CBCB0000-0000-1000-8000-00805F9B34FB") // Custom service
    private let writeCharacteristicUUID = CBUUID(string: "CBCB0001-0000-1000-8000-00805F9B34FB")
    private let notifyCharacteristicUUID = CBUUID(string: "CBCB0002-0000-1000-8000-00805F9B34FB")

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    func startScan() {
        guard centralManager.state == .poweredOn else { return }
        centralManager.scanForPeripherals(withServices: [serviceUUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func stopScan() {
        centralManager.stopScan()
    }

    func connect(_ peripheral: CBPeripheral) {
        targetPeripheral = peripheral
        centralManager.connect(peripheral, options: nil)
    }

    func disconnect() {
        if let peripheral = targetPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }

    func sendData(_ data: Data) {
        guard let peripheral = targetPeripheral,
              let characteristic = writeCharacteristic else { return }
        peripheral.writeValue(data, for: characteristic, type: .withoutResponse)
    }

    // MARK: - CBCentralManagerDelegate

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            print("Bluetooth powered on")
        } else {
            print("Bluetooth state: \(central.state.rawValue)")
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        if !discoveredPeripherals.contains(peripheral) {
            discoveredPeripherals.append(peripheral)
        }
        delegate?.didDiscoverPeripheral(peripheral, rssi: RSSI)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("Connected to peripheral: \(peripheral.name ?? "unknown")")
        peripheral.delegate = self
        peripheral.discoverServices([serviceUUID])
        delegate?.didConnectPeripheral(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        print("Failed to connect: \(error?.localizedDescription ?? "unknown")")
        delegate?.didDisconnectPeripheral(peripheral, error: error)
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("Disconnected from peripheral")
        delegate?.didDisconnectPeripheral(peripheral, error: error)
    }

    // MARK: - CBPeripheralDelegate

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }

        for service in services {
            if service.uuid == serviceUUID {
                peripheral.discoverCharacteristics([writeCharacteristicUUID, notifyCharacteristicUUID], for: service)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }

        for characteristic in characteristics {
            if characteristic.uuid == writeCharacteristicUUID {
                writeCharacteristic = characteristic
            } else if characteristic.uuid == notifyCharacteristicUUID {
                notifyCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            print("Characteristic update error: \(error)")
            return
        }

        if let data = characteristic.value {
            delegate?.didReceiveData(data, from: characteristic)
        }
    }
}

// MARK: - NetworkManager Bluetooth Delegate Extension

extension NetworkManager: BluetoothManagerDelegate {
    func didDiscoverPeripheral(_ peripheral: CBPeripheral, rssi: NSNumber) {
        // Check if this is our cricket bat device
        if peripheral.name?.contains("CricketBat") == true || peripheral.name?.contains("CricketSim") == true {
            bluetoothPeripheral = peripheral
            stopBluetoothScan()
            connectBluetooth()
        }
    }

    func didConnectPeripheral(_ peripheral: CBPeripheral) {
        print("Bluetooth connected, sending handshake")
        // Send handshake over Bluetooth
        let payload = HandshakePayload(
            deviceId: getDeviceId(),
            deviceName: UIDevice.current.name,
            appVersion: Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0",
            supportsAR: ARWorldTrackingConfiguration.isSupported,
            supportsBluetooth: true,
            supportedFeatures: ["sensor_fusion", "swing_detection", "ar_tracking", "haptics"]
        )
        sendBluetoothPacket(type: .handshake, payload: payload)
    }

    func didDisconnectPeripheral(_ peripheral: CBPeripheral, error: Error?) {
        print("Bluetooth disconnected: \(error?.localizedDescription ?? "no error")")
        cleanupConnection()
        connectionState = .disconnected
        onConnectionStateChange?(.disconnected)
    }

    func didReceiveData(_ data: Data, from characteristic: CBCharacteristic) {
        processReceivedData(data)
    }

    private func sendBluetoothPacket<Payload: Codable>(type: PacketType, payload: Payload) {
        do {
            let header = PacketHeader(
                packetType: type,
                sequenceNumber: nextSequenceNumber(),
                payloadSize: 0,
                timestamp: currentTimestampUs()
            )

            var packet = NetworkPacket(header: header, payload: payload)
            let encoder = JSONEncoder()
            let payloadData = try encoder.encode(payload)
            packet.header.payloadSize = UInt32(payloadData.count)
            let fullData = try encoder.encode(packet)

            sendData(fullData)
        } catch {
            print("Failed to encode Bluetooth packet: \(error)")
        }
    }
}

// Need to import ARKit for ARWorldTrackingConfiguration
import ARKit