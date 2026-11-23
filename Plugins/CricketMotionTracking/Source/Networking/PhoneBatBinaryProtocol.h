#pragma once

#include "CoreMinimal.h"
#include "PhoneBatNetworkProtocol.h"

// Forward declaration for protobuf messages
// In a real implementation, this would include the generated protobuf headers
// For now, we provide a binary serialization interface that can be implemented
// with protobuf or a custom binary format

// Binary protocol magic and version
namespace PhoneBatBinaryProtocol
{
    constexpr uint32 MAGIC_NUMBER = 0x43425048; // "CBPH"
    constexpr uint16 CURRENT_VERSION = 1;
    constexpr uint16 HEADER_SIZE = 24; // Fixed header size

    // Packet types matching the protobuf enum
    enum class EPacketType : uint8
    {
        // Connection
        Handshake = 1,
        HandshakeAck = 2,
        PairingRequest = 3,
        PairingResponse = 4,
        Disconnect = 5,
        Heartbeat = 6,

        // Calibration
        CalibrationStart = 10,
        CalibrationData = 11,
        CalibrationComplete = 12,

        // Sensor data
        SensorFrame = 20,
        SensorBatch = 21,

        // Swing analysis
        SwingPhaseUpdate = 30,
        SwingContactPredict = 31,
        SwingResult = 32,

        // Clock sync
        ClockSyncRequest = 40,
        ClockSyncResponse = 41,

        // Configuration
        ConfigUpdate = 50,
        SettingsRequest = 51,
        SettingsResponse = 52
    };
}

// Binary packet header (packed, 24 bytes)
#pragma pack(push, 1)
struct FPhoneBatBinaryHeader
{
    uint32 MagicNumber = PhoneBatBinaryProtocol::MAGIC_NUMBER;
    uint16 Version = PhoneBatBinaryProtocol::CURRENT_VERSION;
    uint8 PacketType = 0;
    uint8 Flags = 0;              // Reserved for future use
    uint32 SequenceNumber = 0;
    uint32 PayloadSize = 0;
    uint64 Timestamp = 0;         // Microseconds since epoch
    uint64 ServerTimestamp = 0;   // Microseconds since epoch (server)

    // Validate header
    bool IsValid() const
    {
        return MagicNumber == PhoneBatBinaryProtocol::MAGIC_NUMBER &&
               Version == PhoneBatBinaryProtocol::CURRENT_VERSION &&
               PayloadSize <= 1400; // MTU safe
    }

    // Network byte order conversion
    void ToNetworkOrder()
    {
        // In UE, we handle endianness properly
        // For now, assume little-endian (most platforms)
    }

    void FromNetworkOrder()
    {
        // Reverse of ToNetworkOrder
    }
};
#pragma pack(pop)

// Binary serialization helpers
class PHONEBAT_API FPhoneBatBinarySerializer
{
public:
    // Serialize a sensor frame payload to binary (very fast, ~100 bytes vs ~500 JSON)
    static bool SerializeSensorFrame(const FPhoneBatSensorFramePayload& Payload, TArray<uint8>& OutData);

    // Deserialize sensor frame from binary
    static bool DeserializeSensorFrame(const uint8* Data, int32 Size, FPhoneBatSensorFramePayload& OutPayload);

    // Serialize swing phase payload
    static bool SerializeSwingPhase(const FPhoneBatSwingPhasePayload& Payload, TArray<uint8>& OutData);

    // Deserialize swing phase
    static bool DeserializeSwingPhase(const uint8* Data, int32 Size, FPhoneBatSwingPhasePayload& OutPayload);

    // Serialize contact prediction
    static bool SerializeContactPredict(const FPhoneBatContactPredictPayload& Payload, TArray<uint8>& OutData);

    // Deserialize contact prediction
    static bool DeserializeContactPredict(const uint8* Data, int32 Size, FPhoneBatContactPredictPayload& OutPayload);

    // Serialize clock sync
    static bool SerializeClockSync(const FPhoneBatClockSyncPayload& Payload, TArray<uint8>& OutData);

    // Deserialize clock sync
    static bool DeserializeClockSync(const uint8* Data, int32 Size, FPhoneBatClockSyncPayload& OutPayload);

    // Serialize any payload with header
    template<typename PayloadType>
    static bool SerializePacket(PhoneBatBinaryProtocol::EPacketType PacketType, uint32 SequenceNumber, const PayloadType& Payload, TArray<uint8>& OutData)
    {
        // Serialize payload first
        TArray<uint8> PayloadData;
        bool bSuccess = false;

        if constexpr (std::is_same_v<PayloadType, FPhoneBatSensorFramePayload>)
        {
            bSuccess = SerializeSensorFrame(static_cast<const FPhoneBatSensorFramePayload&>(Payload), PayloadData);
        }
        else if constexpr (std::is_same_v<PayloadType, FPhoneBatSwingPhasePayload>)
        {
            bSuccess = SerializeSwingPhase(static_cast<const FPhoneBatSwingPhasePayload&>(Payload), PayloadData);
        }
        else if constexpr (std::is_same_v<PayloadType, FPhoneBatContactPredictPayload>)
        {
            bSuccess = SerializeContactPredict(static_cast<const FPhoneBatContactPredictPayload&>(Payload), PayloadData);
        }
        else if constexpr (std::is_same_v<PayloadType, FPhoneBatClockSyncPayload>)
        {
            bSuccess = SerializeClockSync(static_cast<const FPhoneBatClockSyncPayload&>(Payload), PayloadData);
        }
        else
        {
            // Fallback: use JSON for other types (calibration, handshake, etc.)
            FMemoryWriter Writer(PayloadData);
            Writer << Payload;
            bSuccess = !Writer.IsError();
        }

        if (!bSuccess || PayloadData.Num() > 1400)
        {
            return false;
        }

        // Build header
        FPhoneBatBinaryHeader Header;
        Header.PacketType = static_cast<uint8>(PacketType);
        Header.SequenceNumber = SequenceNumber;
        Header.PayloadSize = PayloadData.Num();
        Header.Timestamp = FPhoneBatReceiverComponent::GetCurrentTimestampUSStatic();
        Header.ServerTimestamp = 0; // Filled by server

        // Combine header + payload
        OutData.SetNumUninitialized(sizeof(FPhoneBatBinaryHeader) + PayloadData.Num());
        FMemory::Memcpy(OutData.GetData(), &Header, sizeof(FPhoneBatBinaryHeader));
        FMemory::Memcpy(OutData.GetData() + sizeof(FPhoneBatBinaryHeader), PayloadData.GetData(), PayloadData.Num());

        return true;
    }

    // Deserialize packet from binary (header + payload)
    static bool DeserializePacket(const uint8* Data, int32 Size, PhoneBatBinaryProtocol::EPacketType& OutPacketType,
                                  uint32& OutSequenceNumber, uint64& OutTimestamp, uint64& OutServerTimestamp,
                                  TArray<uint8>& OutPayloadData)
    {
        if (Size < static_cast<int32>(sizeof(FPhoneBatBinaryHeader)))
        {
            return false;
        }

        const FPhoneBatBinaryHeader* Header = reinterpret_cast<const FPhoneBatBinaryHeader*>(Data);
        if (!Header->IsValid())
        {
            return false;
        }

        if (Size != static_cast<int32>(sizeof(FPhoneBatBinaryHeader)) + Header->PayloadSize)
        {
            return false; // Size mismatch
        }

        OutPacketType = static_cast<PhoneBatBinaryProtocol::EPacketType>(Header->PacketType);
        OutSequenceNumber = Header->SequenceNumber;
        OutTimestamp = Header->Timestamp;
        OutServerTimestamp = Header->ServerTimestamp;

        OutPayloadData.SetNumUninitialized(Header->PayloadSize);
        FMemory::Memcpy(OutPayloadData.GetData(), Data + sizeof(FPhoneBatBinaryHeader), Header->PayloadSize);

        return true;
    }

    // Fast vector/quaternion serialization
    static void WriteVector(FMemoryWriter& Writer, const FVector& Vec);
    static void ReadVector(FMemoryReader& Reader, FVector& OutVec);
    static void WriteQuat(FMemoryWriter& Writer, const FQuat& Quat);
    static void ReadQuat(FMemoryReader& Reader, FQuat& OutQuat);
    static void WriteTransform(FMemoryWriter& Writer, const FTransform& Transform);
    static void ReadTransform(FMemoryReader& Reader, FTransform& OutTransform);
};