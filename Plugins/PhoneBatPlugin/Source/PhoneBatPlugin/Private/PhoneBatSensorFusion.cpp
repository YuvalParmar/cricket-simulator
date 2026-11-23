#include "PhoneBatSensorFusion.h"
#include "Math/UnrealMathUtility.h"

UPhoneBatSensorFusion::UPhoneBatSensorFusion()
{
    CurrentState = FSensorFusionState();
    MahonyIntegralError = FVector::ZeroVector;
    LastTimestampUs = 0;
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
    CurrentState = FSensorFusionState();
    MahonyIntegralError = FVector::ZeroVector;
    LastTimestampUs = 0;
    CurrentState.bIsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("PhoneBatSensorFusion initialized"));
}

void UPhoneBatSensorFusion::Shutdown()
{
    CurrentState = FSensorFusionState();
    UE_LOG(LogTemp, Log, TEXT("PhoneBatSensorFusion shutdown"));
}

FSensorFusionState UPhoneBatSensorFusion::ProcessSensorFrame(const FPhoneBatSensorFramePayload& Payload)
{
    float DeltaTime = 0.016f; // Default 60Hz
    if (LastTimestampUs > 0)
    {
        DeltaTime = (Payload.Timestamp - LastTimestampUs) / 1000000.0f;
    }
    if (DeltaTime <= 0 || DeltaTime > 0.1f) DeltaTime = 0.016f;

    LastTimestampUs = Payload.Timestamp;

    // Update based on selected algorithm
    switch (Algorithm)
    {
        case ESensorFusionAlgorithm::ComplementaryFilter:
            CurrentState = UpdateComplementaryFilter(Payload, DeltaTime);
            break;
        case ESensorFusionAlgorithm::MadgwickFilter:
            CurrentState = UpdateMadgwickFilter(Payload, DeltaTime);
            break;
        case ESensorFusionAlgorithm::MahonyFilter:
            CurrentState = UpdateMahonyFilter(Payload, DeltaTime);
            break;
        default:
            CurrentState = UpdateComplementaryFilter(Payload, DeltaTime);
            break;
    }

    // Compute confidence based on sensor quality
    CurrentState.Confidence = Payload.Confidence;

    OnSensorFrameProcessed.Broadcast(CurrentState);

    return CurrentState;
}

void UPhoneBatSensorFusion::SetAlgorithm(ESensorFusionAlgorithm NewAlgorithm)
{
    if (Algorithm != NewAlgorithm)
    {
        Algorithm = NewAlgorithm;
        UE_LOG(LogTemp, Log, TEXT("PhoneBatSensorFusion: Algorithm changed to %s"), *UEnum::GetValueAsString(Algorithm));
    }
}

void UPhoneBatSensorFusion::SetGain(float NewGain)
{
    Beta = NewGain;
    TwoKp = NewGain * 2.0f;
}

FSensorFusionState UPhoneBatSensorFusion::UpdateComplementaryFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime)
{
    // Gyro integration for orientation
    FVector GyroRad = FVector(
        Payload.Gyroscope.X * 0.0174533f,
        Payload.Gyroscope.Y * 0.0174533f,
        Payload.Gyroscope.Z * 0.0174533f
    );

    FQuat DeltaRotation(GyroRad * DeltaTime);
    FQuat NewOrientation = CurrentState.Orientation * DeltaRotation;

    // Accelerometer correction
    FVector AccelNorm = Payload.Accelerometer.GetSafeNormal();
    FVector ExpectedGravity(0, 0, -1);

    FVector AccelerometerError = FVector::CrossProduct(AccelNorm, ExpectedGravity);
    FQuat CorrectionRotation(FVector(ComplementaryAlpha * AccelerometerError * DeltaTime * 50.0f));

    NewOrientation = NewOrientation * CorrectionRotation;
    NewOrientation.Normalize();

    CurrentState.Orientation = NewOrientation;
    CurrentState.LinearAcceleration = Payload.LinearAcceleration;
    CurrentState.Gravity = AccelNorm;
    CurrentState.AngularVelocity = Payload.Gyroscope;

    return CurrentState;
}

FSensorFusionState UPhoneBatSensorFusion::UpdateMadgwickFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime)
{
    FVector a = Payload.LinearAcceleration.GetSafeNormal();
    FVector g = Payload.Gyroscope;

    float invDt = 1.0f / DeltaTime;
    float q0 = CurrentState.Orientation.W;
    float q1 = CurrentState.Orientation.X;
    float q2 = CurrentState.Orientation.Y;
    float q3 = CurrentState.Orientation.Z;

    // Gradient descent algorithm corrective step
    float hx = 2.0f * (q1 * q3 - q0 * q2);
    float hy = 2.0f * (q0 * q1 + q2 * q3);
    float bx = FMath::Sqrt((1.0f + hx) * (1.0f + hx) + (1.0f + hy) * (1.0f + hy));
    float bz = 2.0f * (q2 * q3 - q0 * q1);

    float s0 = -2.0f * bx * (1.732f * q1 + gz * q2);
    float s1 = 2.0f * bx * (1.732f * q0 - gz * q3) + 2.0f * bz * q3;
    float s2 = -2.0f * bx * q2 + 2.0f * bz * (1.732f * q0 - gz * q1);
    float s3 = 2.0f * bx * q3 + 2.0f * bz * q1;

    // Apply feedback step
    float Ks = 2.0f;
    s0 *= Ks; s1 *= Ks; s2 *= Ks; s3 *= Ks;

    // Integrate the filter equation
    float gx = g.X * invDt + s0;
    float gy = g.Y * invDt + s1;
    float gz = g.Z * invDt + s2;

    q0 += (-q1 * gx - q2 * gy - q3 * gz) * (Beta / 2.0f);
    q1 += (q0 * gx + q2 * gz - q3 * gy) * (Beta / 2.0f);
    q2 += (q0 * gy - q1 * gz + q3 * gx) * (Beta / 2.0f);
    q3 += (q0 * gz + q1 * gy - q2 * gx) * (Beta / 2.0f);

    // Normalize quaternion
    float norm = FMath::InvSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= norm; q1 *= norm; q2 *= norm; q3 *= norm;

    CurrentState.Orientation = FQuat(q1, q2, q3, q0);
    CurrentState.AngularVelocity = g;
    CurrentState.LinearAcceleration = Payload.LinearAcceleration;
    CurrentState.Gravity = FVector(0, 0, -9.81f);

    return CurrentState;
}

FSensorFusionState UPhoneBatSensorFusion::UpdateMahonyFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime)
{
    FVector g = Payload.Gyroscope;

    // Compute error
    FVector accel = Payload.LinearAcceleration.GetSafeNormal();
    FVector expected = FVector(0, 0, -1);
    FVector error = FVector::CrossProduct(accel, expected);

    // Apply integral feedback
    MahonyIntegralError += error * TwoKi * DeltaTime;

    FVector correction = TwoKp * error + MahonyIntegralError;

    // Integrate
    FQuat DeltaRotation(g * DeltaTime);
    CurrentState.Orientation = CurrentState.Orientation * DeltaRotation * FQuat(correction * DeltaTime * 0.5f);
    CurrentState.Orientation.Normalize();

    CurrentState.AngularVelocity = g;
    CurrentState.LinearAcceleration = Payload.LinearAcceleration;
    CurrentState.Gravity = expected;

    return CurrentState;
}

void UPhoneBatSensorFusion::NormalizeVector(FVector& Vec) const
{
    float Len = FMath::Sqrt(Vec.X * Vec.X + Vec.Y * Vec.Y + Vec.Z * Vec.Z);
    if (Len > 0)
    {
        Vec.X /= Len;
        Vec.Y /= Len;
        Vec.Z /= Len;
    }
}

float UPhoneBatSensorFusion::InvSqrt(float x) const
{
    float xhalf = 0.5f * x;
    int32 i = *reinterpret_cast<int32*>(&x);
    i = 0x5f3759df - (i >> 1);
    x = *reinterpret_cast<float*>(&i);
    x = x * (1.5f - xhalf * x * x);
    return x;
}