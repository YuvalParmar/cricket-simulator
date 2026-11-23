#include "PhoneBatSensorFusion.h"
#include "Math/UnrealMathUtility.h"

UPhoneBatSensorFusion::UPhoneBatSensorFusion()
    : CurrentState()
    , Algorithm(ESensorFusionAlgorithm::MadgwickFilter)
    , Beta(0.1f)
    , TwoKp(2.0f)
    , TwoKi(0.0f)
    , ComplementaryAlpha(0.98f)
    , MahonyIntegralError(FVector::ZeroVector)
    , LastTimestampUs(0)
{
}

UPhoneBatSensorFusion& UPhoneBatSensorFusion::Get()
{
    static UPhoneBatSensorFusion* Instance = nullptr;
    if (!Instance)
    {
        Instance = NewObject<UPhoneBatSensorFusion>();
        Instance->AddToRoot();
    }
    return *Instance;
}

void UPhoneBatSensorFusion::Initialize()
{
    Reset();
}

void UPhoneBatSensorFusion::Shutdown()
{
}

FSensorFusionState UPhoneBatSensorFusion::ProcessSensorFrame(const FPhoneBatSensorFramePayload& Payload)
{
    // Calculate delta time
    float DeltaTime = 0.01f; // Default 100Hz
    if (LastTimestampUs > 0 && Payload.Timestamp > LastTimestampUs)
    {
        DeltaTime = (Payload.Timestamp - LastTimestampUs) / 1000000.0f;
        DeltaTime = FMath::Clamp(DeltaTime, 0.001f, 0.1f); // Clamp to reasonable range
    }
    LastTimestampUs = Payload.Timestamp;

    // Convert payload to FVector
    FVector Accel(Payload.Accelerometer.X, Payload.Accelerometer.Y, Payload.Accelerometer.Z);
    FVector Gyro(Payload.Gyroscope.X, Payload.Gyroscope.Y, Payload.Gyroscope.Z);
    FVector Mag(Payload.Magnetometer.X, Payload.Magnetometer.Y, Payload.Magnetometer.Z);
    FQuat Orientation(Payload.Orientation.X, Payload.Orientation.Y, Payload.Orientation.Z, Payload.Orientation.W);

    // Create sensor frame
    FSensorFrame Frame;
    Frame.TimestampUs = Payload.Timestamp;
    Frame.Accelerometer = Accel;
    Frame.Gyroscope = Gyro;
    Frame.Magnetometer = Mag;
    Frame.Orientation = Orientation;
    Frame.Confidence = Payload.Confidence;

    // Run selected filter
    FSensorFusionState NewState;
    switch (Algorithm)
    {
        case ESensorFusionAlgorithm::ComplementaryFilter:
            NewState = UpdateComplementaryFilter(Payload, DeltaTime);
            break;
        case ESensorFusionAlgorithm::MadgwickFilter:
            NewState = UpdateMadgwickFilter(Payload, DeltaTime);
            break;
        case ESensorFusionAlgorithm::MahonyFilter:
            NewState = UpdateMahonyFilter(Payload, DeltaTime);
            break;
        case ESensorFusionAlgorithm::KalmanFilter:
            // Fallback to Madgwick for now
            NewState = UpdateMadgwickFilter(Payload, DeltaTime);
            break;
    }

    // Copy latency timestamps
    NewState.PhoneSensorReadTimeUs = Payload.PhoneSensorReadTimeUs;
    NewState.PhoneFusionCompleteTimeUs = Payload.PhoneFusionCompleteTimeUs;
    NewState.PhoneSendTimeUs = Payload.PhoneSendTimeUs;

    CurrentState = NewState;
    return CurrentState;
}

void UPhoneBatSensorFusion::SetAlgorithm(ESensorFusionAlgorithm InAlgorithm)
{
    Algorithm = InAlgorithm;
    Reset();
}

void UPhoneBatSensorFusion::SetGain(float InGain)
{
    Beta = InGain;
}

void UPhoneBatSensorFusion::SetMagneticGain(float InGain)
{
    // For future use
}

void UPhoneBatSensorFusion::Reset()
{
    CurrentState = FSensorFusionState();
    CurrentState.Orientation = FQuat::Identity;
    CurrentState.Gravity = FVector(0.0f, 0.0f, -9.81f);
    MahonyIntegralError = FVector::ZeroVector;
    LastTimestampUs = 0;
}

FSensorFusionState UPhoneBatSensorFusion::UpdateComplementaryFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime)
{
    FSensorFusionState NewState = CurrentState;

    FVector Accel(Payload.Accelerometer.X, Payload.Accelerometer.Y, Payload.Accelerometer.Z);
    FVector Gyro(Payload.Gyroscope.X, Payload.Gyroscope.Y, Payload.Gyroscope.Z);

    // Normalize accelerometer
    NormalizeVector(Accel);

    // Integrate gyroscope to get orientation change
    FVector GyroDelta = Gyro * DeltaTime;
    FQuat DeltaQuat = FQuat(GyroDelta);
    FQuat GyroOrientation = CurrentState.Orientation * DeltaQuat;

    // Compute orientation from accelerometer (and magnetometer if available)
    FVector Mag(Payload.Magnetometer.X, Payload.Magnetometer.Y, Payload.Magnetometer.Z);
    bool bHasMag = !Mag.IsNearlyZero();
    FQuat AccelOrientation = ComputeOrientationFromAccelMag(Accel, bHasMag ? Mag : FVector::ZeroVector);

    // Complementary filter: blend gyro and accel/mag orientations
    NewState.Orientation = FQuat::Slerp(GyroOrientation, AccelOrientation, 1.0f - ComplementaryAlpha);

    // Extract gravity vector (world Z axis in sensor frame)
    NewState.Gravity = NewState.Orientation.RotateVector(FVector(0.0f, 0.0f, -9.81f));

    // Linear acceleration = measured accel - gravity
    NewState.LinearAcceleration = Accel * 9.81f - NewState.Gravity; // Convert to m/s^2

    // Angular velocity
    NewState.AngularVelocity = Gyro;

    // Confidence based on accelerometer magnitude closeness to 1g
    float AccelMag = Accel.Size();
    NewState.Confidence = 1.0f - FMath::Abs(AccelMag - 1.0f);
    NewState.Confidence = FMath::Clamp(NewState.Confidence, 0.0f, 1.0f);

    NewState.bIsInitialized = true;

    return NewState;
}

FSensorFusionState UPhoneBatSensorFusion::UpdateMadgwickFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime)
{
    FSensorFusionState NewState = CurrentState;

    FVector Accel(Payload.Accelerometer.X, Payload.Accelerometer.Y, Payload.Accelerometer.Z);
    FVector Gyro(Payload.Gyroscope.X, Payload.Gyroscope.Y, Payload.Gyroscope.Z);
    FVector Mag(Payload.Magnetometer.X, Payload.Magnetometer.Y, Payload.Magnetometer.Z);

    // Normalize measurements
    NormalizeVector(Accel);
    bool bHasMag = !Mag.IsNearlyZero();
    if (bHasMag)
    {
        NormalizeVector(Mag);
    }

    FQuat q = NewState.Orientation;
    float q0 = q.W, q1 = q.X, q2 = q.Y, q3 = q.Z;

    // Gradient descent algorithm corrective step
    float _2q0 = 2.0f * q0;
    float _2q1 = 2.0f * q1;
    float _2q2 = 2.0f * q2;
    float _2q3 = 2.0f * q3;
    float _4q0 = 4.0f * q0;
    float _4q1 = 4.0f * q1;
    float _4q2 = 4.0f * q2;
    float _8q1 = 8.0f * q1;
    float _8q2 = 8.0f * q2;
    float q0q0 = q0 * q0;
    float q1q1 = q1 * q1;
    float q2q2 = q2 * q2;
    float q3q3 = q3 * q3;

    // Gradient descent algorithm corrective step
    float s0, s1, s2, s3;

    if (bHasMag)
    {
        // Magnetometer correction
        float _2q0mx = 2.0f * q0 * Mag.X;
        float _2q0my = 2.0f * q0 * Mag.Y;
        float _2q0mz = 2.0f * q0 * Mag.Z;
        float _2q1mx = 2.0f * q1 * Mag.X;
        float _2q3mx = 2.0f * q3 * Mag.X;

        float hx = Mag.X * q0q0 - _2q0my * q3 + _2q0mz * q2 + Mag.X * q1q1 + _2q1mx * q2 + Mag.X * q2q2 - _2q1mx * q3 - Mag.X * q3q3;
        float hy = _2q0mx * q3 + Mag.Y * q0q0 - _2q0mz * q1 + _2q1my * q2 - Mag.Y * q1q1 + Mag.Y * q2q2 + _2q3mz * q3 + Mag.Y * q3q3;
        float hz = -_2q0mx * q2 + _2q0my * q1 + Mag.Z * q0q0 - _2q0mz * q0 + _2q1mz * q3 + Mag.Z * q1q1 + _2q2mz * q3 - Mag.Z * q2q2 + Mag.Z * q3q3;
        float bx = FMath::Sqrt(hx * hx + hy * hy);
        float bz = hz;

        s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - Accel.X) + _2q1 * (2.0f * q0q1 + _2q2q3 - Accel.Y) - _2q0 * (2.0f * q0q2 - _2q1q3 - Accel.Z) + _2q1 * bx - _2q0 * bz;
        s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - Accel.X) + _2q0 * (2.0f * q0q1 + _2q2q3 - Accel.Y) - 4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - Accel.Z) + _2q2 * bx - _2q3 * bz;
        s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - Accel.X) + _2q3 * (2.0f * q0q1 + _2q2q3 - Accel.Y) - 4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - Accel.Z) + _2q3 * bx - _2q0 * bz;
        s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - Accel.X) + _2q2 * (2.0f * q0q1 + _2q2q3 - Accel.Y) - 4.0f * q3 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - Accel.Z) - _2q1 * bx - _2q2 * bz;
    }
    else
    {
        // Accelerometer only
        s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - Accel.X) + _2q1 * (2.0f * q0q1 + _2q2q3 - Accel.Y) - _2q0 * (2.0f * q0q2 - _2q1q3 - Accel.Z);
        s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - Accel.X) + _2q0 * (2.0f * q0q1 + _2q2q3 - Accel.Y) - 4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - Accel.Z);
        s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - Accel.X) + _2q3 * (2.0f * q0q1 + _2q2q3 - Accel.Y) - 4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - Accel.Z);
        s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - Accel.X) + _2q2 * (2.0f * q0q1 + _2q2q3 - Accel.Y) - 4.0f * q3 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - Accel.Z);
    }

    float norm = FMath::Sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (norm > 0.0f)
    {
        norm = Beta / norm;
        s0 *= norm;
        s1 *= norm;
        s2 *= norm;
        s3 *= norm;
    }

    // Compute rate of change of quaternion
    float qDot0 = 0.5f * (-q1 * Gyro.X - q2 * Gyro.Y - q3 * Gyro.Z) - s0;
    float qDot1 = 0.5f * (q0 * Gyro.X + q2 * Gyro.Z - q3 * Gyro.Y) - s1;
    float qDot2 = 0.5f * (q0 * Gyro.Y - q1 * Gyro.Z + q3 * Gyro.X) - s2;
    float qDot3 = 0.5f * (q0 * Gyro.Z + q1 * Gyro.Y - q2 * Gyro.X) - s3;

    // Integrate to yield quaternion
    q0 += qDot0 * DeltaTime;
    q1 += qDot1 * DeltaTime;
    q2 += qDot2 * DeltaTime;
    q3 += qDot3 * DeltaTime;

    // Normalize quaternion
    norm = FMath::Sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 /= norm;
    q1 /= norm;
    q2 /= norm;
    q3 /= norm;

    NewState.Orientation = FQuat(q1, q2, q3, q0);

    // Extract gravity
    NewState.Gravity = NewState.Orientation.RotateVector(FVector(0.0f, 0.0f, -9.81f));

    // Linear acceleration
    NewState.LinearAcceleration = Accel * 9.81f - NewState.Gravity;

    // Angular velocity
    NewState.AngularVelocity = Gyro;

    // Confidence
    float AccelMag = Accel.Size();
    NewState.Confidence = 1.0f - FMath::Abs(AccelMag - 1.0f);
    NewState.Confidence = FMath::Clamp(NewState.Confidence, 0.0f, 1.0f);

    NewState.bIsInitialized = true;

    return NewState;
}

FSensorFusionState UPhoneBatSensorFusion::UpdateMahonyFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime)
{
    FSensorFusionState NewState = CurrentState;

    FVector Accel(Payload.Accelerometer.X, Payload.Accelerometer.Y, Payload.Accelerometer.Z);
    FVector Gyro(Payload.Gyroscope.X, Payload.Gyroscope.Y, Payload.Gyroscope.Z);
    FVector Mag(Payload.Magnetometer.X, Payload.Magnetometer.Y, Payload.Magnetometer.Z);

    NormalizeVector(Accel);
    bool bHasMag = !Mag.IsNearlyZero();
    if (bHasMag)
    {
        NormalizeVector(Mag);
    }

    FQuat q = NewState.Orientation;
    float q0 = q.W, q1 = q.X, q2 = q.Y, q3 = q.Z;

    // Estimated direction of gravity and magnetic field
    float vx = 2.0f * (q1 * q3 - q0 * q2);
    float vy = 2.0f * (q0 * q1 + q2 * q3);
    float vz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

    // Error is cross product between estimated and measured direction of gravity
    float ex = (Accel.Y * vz - Accel.Z * vy);
    float ey = (Accel.Z * vx - Accel.X * vz);
    float ez = (Accel.X * vy - Accel.Y * vx);

    if (bHasMag)
    {
        // Estimated direction of magnetic field
        float wx = 2.0f * bx * (0.5f - q2q2 - q3q3) + 2.0f * bz * (q1q3 - q0q2);
        float wy = 2.0f * bx * (q1q2 - q0q3) + 2.0f * bz * (q0q1 + q2q3);
        float wz = 2.0f * bx * (q0q2 + q1q3) + 2.0f * bz * (0.5f - q1q1 - q2q2);

        // Error is cross product between estimated and measured direction of magnetic field
        ex += (Mag.Y * wz - Mag.Z * wy);
        ey += (Mag.Z * wx - Mag.X * wz);
        ez += (Mag.X * wy - Mag.Y * wx);
    }

    // Compute and apply integral feedback if enabled
    if (TwoKi > 0.0f)
    {
        MahonyIntegralError.X += ex * DeltaTime;
        MahonyIntegralError.Y += ey * DeltaTime;
        MahonyIntegralError.Z += ez * DeltaTime;
        Gyro.X += TwoKi * MahonyIntegralError.X;
        Gyro.Y += TwoKi * MahonyIntegralError.Y;
        Gyro.Z += TwoKi * MahonyIntegralError.Z;
    }

    // Apply proportional feedback
    Gyro.X += TwoKp * ex;
    Gyro.Y += TwoKp * ey;
    Gyro.Z += TwoKp * ez;

    // Integrate rate of change of quaternion
    float qDot0 = 0.5f * (-q1 * Gyro.X - q2 * Gyro.Y - q3 * Gyro.Z);
    float qDot1 = 0.5f * (q0 * Gyro.X + q2 * Gyro.Z - q3 * Gyro.Y);
    float qDot2 = 0.5f * (q0 * Gyro.Y - q1 * Gyro.Z + q3 * Gyro.X);
    float qDot3 = 0.5f * (q0 * Gyro.Z + q1 * Gyro.Y - q2 * Gyro.X);

    q0 += qDot0 * DeltaTime;
    q1 += qDot1 * DeltaTime;
    q2 += qDot2 * DeltaTime;
    q3 += qDot3 * DeltaTime;

    // Normalize quaternion
    float norm = FMath::Sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 /= norm;
    q1 /= norm;
    q2 /= norm;
    q3 /= norm;

    NewState.Orientation = FQuat(q1, q2, q3, q0);

    // Extract gravity
    NewState.Gravity = NewState.Orientation.RotateVector(FVector(0.0f, 0.0f, -9.81f));

    // Linear acceleration
    NewState.LinearAcceleration = Accel * 9.81f - NewState.Gravity;

    // Angular velocity
    NewState.AngularVelocity = Gyro;

    // Confidence
    float AccelMag = Accel.Size();
    NewState.Confidence = 1.0f - FMath::Abs(AccelMag - 1.0f);
    NewState.Confidence = FMath::Clamp(NewState.Confidence, 0.0f, 1.0f);

    NewState.bIsInitialized = true;

    return NewState;
}

FQuat UPhoneBatSensorFusion::ComputeOrientationFromAccelMag(const FVector& Accel, const FVector& Mag) const
{
    // Compute orientation from accelerometer and magnetometer
    // This is a simplified version - in practice use a more robust method

    FVector Up = Accel;
    Up.Normalize();

    FVector East;
    if (!Mag.IsNearlyZero())
    {
        East = FVector::CrossProduct(Up, Mag);
        East.Normalize();
    }
    else
    {
        // No magnetometer - assume arbitrary East
        East = FVector(1.0f, 0.0f, 0.0f);
        if (FMath::Abs(FVector::DotProduct(Up, East)) > 0.9f)
        {
            East = FVector(0.0f, 1.0f, 0.0f);
        }
    }

    FVector North = FVector::CrossProduct(East, Up);
    North.Normalize();

    // Create rotation matrix
    FMatrix RotMatrix(
        FPlane(East.X, East.Y, East.Z, 0),
        FPlane(North.X, North.Y, North.Z, 0),
        FPlane(Up.X, Up.Y, Up.Z, 0),
        FPlane(0, 0, 0, 1)
    );

    return RotMatrix.ToQuat();
}

void UPhoneBatSensorFusion::NormalizeVector(FVector& Vec) const
{
    float Norm = Vec.Size();
    if (Norm > 0.0f)
    {
        Vec /= Norm;
    }
}

float UPhoneBatSensorFusion::InvSqrt(float x) const
{
    return 1.0f / FMath::Sqrt(x);
}