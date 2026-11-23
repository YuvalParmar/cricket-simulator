#include "PhoneBatBinaryProtocol.h"
#include "PhoneBatReceiverComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

bool FPhoneBatBinarySerializer::SerializeSensorFrame(const FPhoneBatSensorFramePayload& Payload, TArray<uint8>& OutData)
{
    FMemoryWriter Writer(OutData);

    // Write all fields in fixed order for maximum speed
    Writer << Payload.Timestamp;
    WriteVector(Writer, Payload.Accelerometer);
    WriteVector(Writer, Payload.Gyroscope);
    WriteVector(Writer, Payload.Magnetometer);
    WriteQuat(Writer, Payload.Orientation);
    WriteVector(Writer, Payload.Gravity);
    WriteVector(Writer, Payload.LinearAcceleration);
    Writer << Payload.Confidence;

    // Latency instrumentation timestamps
    Writer << Payload.PhoneSensorReadTimeUs;
    Writer << Payload.PhoneFusionCompleteTimeUs;
    Writer << Payload.PhoneSendTimeUs;

    return !Writer.IsError();
}

bool FPhoneBatBinarySerializer::DeserializeSensorFrame(const uint8* Data, int32 Size, FPhoneBatSensorFramePayload& OutPayload)
{
    FMemoryReader Reader(Data, Size);

    Reader << OutPayload.Timestamp;
    ReadVector(Reader, OutPayload.Accelerometer);
    ReadVector(Reader, OutPayload.Gyroscope);
    ReadVector(Reader, OutPayload.Magnetometer);
    ReadQuat(Reader, OutPayload.Orientation);
    ReadVector(Reader, OutPayload.Gravity);
    ReadVector(Reader, OutPayload.LinearAcceleration);
    Reader << OutPayload.Confidence;

    // Latency instrumentation timestamps
    Reader << OutPayload.PhoneSensorReadTimeUs;
    Reader << OutPayload.PhoneFusionCompleteTimeUs;
    Reader << OutPayload.PhoneSendTimeUs;

    return !Reader.IsError();
}

bool FPhoneBatBinarySerializer::SerializeSwingPhase(const FPhoneBatSwingPhasePayload& Payload, TArray<uint8>& OutData)
{
    FMemoryWriter Writer(OutData);

    Writer << Payload.Timestamp;
    Writer << static_cast<uint8>(Payload.CurrentPhase);
    Writer << Payload.PhaseProgress;
    Writer << Payload.BatSpeed;
    Writer << Payload.MaxBatSpeed;
    WriteVector(Writer, Payload.BatVelocity);
    WriteVector(Writer, Payload.BatAngularVelocity);
    Writer << Payload.Confidence;

    // Latency instrumentation timestamps
    Writer << Payload.PhoneSensorReadTimeUs;
    Writer << Payload.PhoneFusionCompleteTimeUs;
    Writer << Payload.PhoneSendTimeUs;

    return !Writer.IsError();
}

bool FPhoneBatBinarySerializer::DeserializeSwingPhase(const uint8* Data, int32 Size, FPhoneBatSwingPhasePayload& OutPayload)
{
    FMemoryReader Reader(Data, Size);

    uint8 PhaseByte;
    Reader << OutPayload.Timestamp;
    Reader << PhaseByte;
    OutPayload.CurrentPhase = static_cast<EPhoneBatSwingPhase>(PhaseByte);
    Reader << OutPayload.PhaseProgress;
    Reader << OutPayload.BatSpeed;
    Reader << OutPayload.MaxBatSpeed;
    ReadVector(Reader, OutPayload.BatVelocity);
    ReadVector(Reader, OutPayload.BatAngularVelocity);
    Reader << OutPayload.Confidence;

    // Latency instrumentation timestamps
    Reader << OutPayload.PhoneSensorReadTimeUs;
    Reader << OutPayload.PhoneFusionCompleteTimeUs;
    Reader << OutPayload.PhoneSendTimeUs;

    return !Reader.IsError();
}

bool FPhoneBatBinarySerializer::SerializeContactPredict(const FPhoneBatContactPredictPayload& Payload, TArray<uint8>& OutData)
{
    FMemoryWriter Writer(OutData);

    Writer << Payload.Timestamp;
    Writer << Payload.PredictedContactTime;
    WriteVector(Writer, Payload.PredictedContactPoint);
    WriteVector(Writer, Payload.PredictedBatVelocity);
    WriteQuat(Writer, Payload.PredictedBatOrientation);
    Writer << Payload.Confidence;

    // Latency instrumentation timestamps
    Writer << Payload.PhoneSensorReadTimeUs;
    Writer << Payload.PhoneFusionCompleteTimeUs;
    Writer << Payload.PhoneSendTimeUs;

    return !Writer.IsError();
}

bool FPhoneBatBinarySerializer::DeserializeContactPredict(const uint8* Data, int32 Size, FPhoneBatContactPredictPayload& OutPayload)
{
    FMemoryReader Reader(Data, Size);

    Reader << OutPayload.Timestamp;
    Reader << OutPayload.PredictedContactTime;
    ReadVector(Reader, OutPayload.PredictedContactPoint);
    ReadVector(Reader, OutPayload.PredictedBatVelocity);
    ReadQuat(Reader, OutPayload.PredictedBatOrientation);
    Reader << OutPayload.Confidence;

    // Latency instrumentation timestamps
    Reader << OutPayload.PhoneSensorReadTimeUs;
    Reader << OutPayload.PhoneFusionCompleteTimeUs;
    Reader << OutPayload.PhoneSendTimeUs;

    return !Reader.IsError();
}

bool FPhoneBatBinarySerializer::SerializeClockSync(const FPhoneBatClockSyncPayload& Payload, TArray<uint8>& OutData)
{
    FMemoryWriter Writer(OutData);

    Writer << Payload.ClientSendTime;
    Writer << Payload.ServerReceiveTime;
    Writer << Payload.ServerSendTime;
    Writer << Payload.ClientReceiveTime;
    Writer << Payload.RoundTripTime;
    Writer << Payload.ClockOffset;

    // Latency instrumentation timestamps
    Writer << Payload.PhoneSensorReadTimeUs;
    Writer << Payload.PhoneFusionCompleteTimeUs;
    Writer << Payload.PhoneSendTimeUs;

    return !Writer.IsError();
}

bool FPhoneBatBinarySerializer::DeserializeClockSync(const uint8* Data, int32 Size, FPhoneBatClockSyncPayload& OutPayload)
{
    FMemoryReader Reader(Data, Size);

    Reader << OutPayload.ClientSendTime;
    Reader << OutPayload.ServerReceiveTime;
    Reader << OutPayload.ServerSendTime;
    Reader << OutPayload.ClientReceiveTime;
    Reader << OutPayload.RoundTripTime;
    Reader << OutPayload.ClockOffset;

    // Latency instrumentation timestamps
    Reader << OutPayload.PhoneSensorReadTimeUs;
    Reader << OutPayload.PhoneFusionCompleteTimeUs;
    Reader << OutPayload.PhoneSendTimeUs;

    return !Reader.IsError();
}

// Fast vector serialization (3 floats)
void FPhoneBatBinarySerializer::WriteVector(FMemoryWriter& Writer, const FVector& Vec)
{
    Writer << Vec.X << Vec.Y << Vec.Z;
}

void FPhoneBatBinarySerializer::ReadVector(FMemoryReader& Reader, FVector& OutVec)
{
    Reader << OutVec.X << OutVec.Y << OutVec.Z;
}

// Fast quaternion serialization (4 floats)
void FPhoneBatBinarySerializer::WriteQuat(FMemoryWriter& Writer, const FQuat& Quat)
{
    Writer << Quat.X << Quat.Y << Quat.Z << Quat.W;
}

void FPhoneBatBinarySerializer::ReadQuat(FMemoryReader& Reader, FQuat& OutQuat)
{
    Reader << OutQuat.X << OutQuat.Y << OutQuat.Z << OutQuat.W;
}

// Fast transform serialization
void FPhoneBatBinarySerializer::WriteTransform(FMemoryWriter& Writer, const FTransform& Transform)
{
    WriteVector(Writer, Transform.GetLocation());
    WriteQuat(Writer, Transform.GetRotation());
    WriteVector(Writer, Transform.GetScale3D());
}

void FPhoneBatBinarySerializer::ReadTransform(FMemoryReader& Reader, FTransform& OutTransform)
{
    FVector Pos, Scale;
    FQuat Rot;
    ReadVector(Reader, Pos);
    ReadQuat(Reader, Rot);
    ReadVector(Reader, Scale);
    OutTransform = FTransform(Rot, Pos, Scale);
}