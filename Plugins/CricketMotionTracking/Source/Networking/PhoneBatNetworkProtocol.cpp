#include "PhoneBatNetworkProtocol.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

bool FPhoneBatPacket::SerializePayload(const FPhoneBatHandshakePayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.DeviceId;
    Writer << Payload.DeviceName;
    Writer << Payload.AppVersion;
    Writer << Payload.Platform;
    Writer << Payload.bSupportsAR;
    Writer << Payload.bSupportsBluetooth;
    Writer << Payload.SupportedFeatures;
    return !Writer.IsError();
}

bool FPhoneBatPacket::SerializePayload(const FPhoneBatPairingPayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.PairingCode;
    Writer << Payload.DeviceId;
    Writer << Payload.PlayerName;
    Writer << Payload.bIsLeftHanded;
    return !Writer.IsError();
}

bool FPhoneBatPacket::SerializePayload(const FPhoneBatSensorFramePayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.Timestamp;
    Writer << Payload.Accelerometer;
    Writer << Payload.Gyroscope;
    Writer << Payload.Magnetometer;
    Writer << Payload.Orientation;
    Writer << Payload.Gravity;
    Writer << Payload.LinearAcceleration;
    Writer << Payload.Confidence;
    return !Writer.IsError();
}

bool FPhoneBatPacket::SerializePayload(const FPhoneBatSwingPhasePayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.Timestamp;
    Writer << static_cast<uint8>(Payload.CurrentPhase);
    Writer << Payload.PhaseProgress;
    Writer << Payload.BatSpeed;
    Writer << Payload.MaxBatSpeed;
    Writer << Payload.BatVelocity;
    Writer << Payload.BatAngularVelocity;
    Writer << Payload.Confidence;
    return !Writer.IsError();
}

bool FPhoneBatPacket::SerializePayload(const FPhoneBatContactPredictPayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.Timestamp;
    Writer << Payload.PredictedContactTime;
    Writer << Payload.PredictedContactPoint;
    Writer << Payload.PredictedBatVelocity;
    Writer << Payload.PredictedBatOrientation;
    Writer << Payload.Confidence;
    return !Writer.IsError();
}

bool FPhoneBatPacket::SerializePayload(const FPhoneBatSwingResultPayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.ContactTimestamp;
    Writer << Payload.ShotType;
    Writer << Payload.TimingQuality;
    Writer << Payload.PowerRating;
    Writer << Payload.ControlRating;
    Writer << Payload.ContactPoint;
    Writer << Payload.BatVelocityAtContact;
    Writer << Payload.BatOrientationAtContact;
    Writer << Payload.WristRotationAtContact;
    Writer << Payload.SwingFrames;
    return !Writer.IsError();
}

bool FPhoneBatPacket::SerializePayload(const FPhoneBatCalibrationPayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.Step;
    Writer << Payload.Instruction;
    Writer << Payload.PhoneToBatTransform;
    Writer << Payload.BatLength;
    Writer << Payload.BatCenterOfMass;
    Writer << Payload.bComplete;
    Writer << Payload.QualityScore;
    return !Writer.IsError();
}

bool FPhoneBatPacket::SerializePayload(const FPhoneBatClockSyncPayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.ClientSendTime;
    Writer << Payload.ServerReceiveTime;
    Writer << Payload.ServerSendTime;
    Writer << Payload.ClientReceiveTime;
    Writer << Payload.RoundTripTime;
    Writer << Payload.ClockOffset;
    return !Writer.IsError();
}

bool FPhoneBatPacket::SerializePayload(const FPhoneBatConfigPayload& Payload)
{
    FMemoryWriter Writer(PayloadData);
    Writer << Payload.SensorRate;
    Writer << Payload.SwingDetectionSensitivity;
    Writer << Payload.bEnableHaptics;
    Writer << Payload.bEnableARVisualization;
    Writer << Payload.JitterBufferSize;
    Writer << Payload.MaxLatencyCompensation;
    Writer << Payload.PlayerProfileId;
    return !Writer.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatHandshakePayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    Reader << OutPayload.DeviceId;
    Reader << OutPayload.DeviceName;
    Reader << OutPayload.AppVersion;
    Reader << OutPayload.Platform;
    Reader << OutPayload.bSupportsAR;
    Reader << OutPayload.bSupportsBluetooth;
    Reader << OutPayload.SupportedFeatures;
    return !Reader.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatPairingPayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    Reader << OutPayload.PairingCode;
    Reader << OutPayload.DeviceId;
    Reader << OutPayload.PlayerName;
    Reader << OutPayload.bIsLeftHanded;
    return !Reader.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatSensorFramePayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    Reader << OutPayload.Timestamp;
    Reader << OutPayload.Accelerometer;
    Reader << OutPayload.Gyroscope;
    Reader << OutPayload.Magnetometer;
    Reader << OutPayload.Orientation;
    Reader << OutPayload.Gravity;
    Reader << OutPayload.LinearAcceleration;
    Reader << OutPayload.Confidence;
    return !Reader.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatSwingPhasePayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    uint8 PhaseByte;
    Reader << OutPayload.Timestamp;
    Reader << PhaseByte;
    OutPayload.CurrentPhase = static_cast<EPhoneBatSwingPhase>(PhaseByte);
    Reader << OutPayload.PhaseProgress;
    Reader << OutPayload.BatSpeed;
    Reader << OutPayload.MaxBatSpeed;
    Reader << OutPayload.BatVelocity;
    Reader << OutPayload.BatAngularVelocity;
    Reader << OutPayload.Confidence;
    return !Reader.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatContactPredictPayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    Reader << OutPayload.Timestamp;
    Reader << OutPayload.PredictedContactTime;
    Reader << OutPayload.PredictedContactPoint;
    Reader << OutPayload.PredictedBatVelocity;
    Reader << OutPayload.PredictedBatOrientation;
    Reader << OutPayload.Confidence;
    return !Reader.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatSwingResultPayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    Reader << OutPayload.ContactTimestamp;
    Reader << OutPayload.ShotType;
    Reader << OutPayload.TimingQuality;
    Reader << OutPayload.PowerRating;
    Reader << OutPayload.ControlRating;
    Reader << OutPayload.ContactPoint;
    Reader << OutPayload.BatVelocityAtContact;
    Reader << OutPayload.BatOrientationAtContact;
    Reader << OutPayload.WristRotationAtContact;
    Reader << OutPayload.SwingFrames;
    return !Reader.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatCalibrationPayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    Reader << OutPayload.Step;
    Reader << OutPayload.Instruction;
    Reader << OutPayload.PhoneToBatTransform;
    Reader << OutPayload.BatLength;
    Reader << OutPayload.BatCenterOfMass;
    Reader << OutPayload.bComplete;
    Reader << OutPayload.QualityScore;
    return !Reader.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatClockSyncPayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    Reader << OutPayload.ClientSendTime;
    Reader << OutPayload.ServerReceiveTime;
    Reader << OutPayload.ServerSendTime;
    Reader << OutPayload.ClientReceiveTime;
    Reader << OutPayload.RoundTripTime;
    Reader << OutPayload.ClockOffset;
    return !Reader.IsError();
}

bool FPhoneBatPacket::DeserializePayload(FPhoneBatConfigPayload& OutPayload) const
{
    FMemoryReader Reader(PayloadData);
    Reader << OutPayload.SensorRate;
    Reader << OutPayload.SwingDetectionSensitivity;
    Reader << OutPayload.bEnableHaptics;
    Reader << OutPayload.bEnableARVisualization;
    Reader << OutPayload.JitterBufferSize;
    Reader << OutPayload.MaxLatencyCompensation;
    Reader << OutPayload.PlayerProfileId;
    return !Reader.IsError();
}