#include "PhoneBatCalibration.h"
#include "PhoneBatInput.h"
#include "PhoneBatNetworkProtocol.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

UPhoneBatCalibration::UPhoneBatCalibration()
{
    SetupCalibrationSteps();
}

void UPhoneBatCalibration::Initialize(UPhoneBatInput* InPhoneBatInput)
{
    PhoneBatInput = InPhoneBatInput;

    if (PhoneBatInput)
    {
        PhoneBatInput->OnCalibrationProgress.AddDynamic(this, &UPhoneBatCalibration::ProcessPhoneResponse);
    }
}

void UPhoneBatCalibration::StartCalibration(const FCalibrationProfile& Profile)
{
    CurrentProfile = Profile;
    CurrentStep = ECalibrationStep::Step0_PlacePhoneFlat;
    Status = ECalibrationStatus::InProgress;
    LastResult = FCalibrationResult();

    // Reset validation data
    ValidationOrientationErrors.Empty();
    ValidationPositionErrors.Empty();
    bValidating = false;

    OnStatusChanged.Broadcast(Status);
    OnStepChanged.Broadcast(CurrentStep);
    UpdateProgress();

    // Send first step to phone
    if (PhoneBatInput)
    {
        PhoneBatInput->StartCalibration();
    }
}

void UPhoneBatCalibration::AdvanceStep()
{
    // Move to next step
    int32 CurrentIndex = static_cast<int32>(CurrentStep);
    int32 NextIndex = CurrentIndex + 1;

    if (NextIndex >= static_cast<int32>(ECalibrationStep::Complete))
    {
        // Calibration complete
        FinishCalibration();
        return;
    }

    CurrentStep = static_cast<ECalibrationStep>(NextIndex);
    OnStepChanged.Broadcast(CurrentStep);
    UpdateProgress();

    // Send step to phone
    SendStepToPhone(CurrentStep);
}

void UPhoneBatCalibration::CompleteCurrentStep(const FTransform& MeasuredTransform)
{
    // Store the measured transform for this step
    // In practice, this would come from the phone automatically
    // This function is called when user confirms step completion in UI

    switch (CurrentStep)
    {
        case ECalibrationStep::Step0_PlacePhoneFlat:
            // Store reference orientation (phone flat)
            CurrentProfile.PhoneToBatTransform = MeasuredTransform;
            break;

        case ECalibrationStep::Step1_HoldBatGrip:
            // Store grip transform
            CurrentProfile.PhoneToBatTransform = MeasuredTransform;
            break;

        case ECalibrationStep::Step2_SwingSlowly:
            // Validate swing tracking
            break;

        case ECalibrationStep::Step3_VerifyAlignment:
            // Final verification
            break;

        default:
            break;
    }

    // Advance to next step
    AdvanceStep();
}

void UPhoneBatCalibration::CancelCalibration()
{
    Status = ECalibrationStatus::Cancelled;
    OnStatusChanged.Broadcast(Status);

    if (PhoneBatInput)
    {
        PhoneBatInput->CancelPairing();
    }
}

bool UPhoneBatCalibration::SaveProfile(const FCalibrationProfile& Profile, const FString& SlotName)
{
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);

    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("ProfileId"), Profile.ProfileId);
    Writer->WriteValue(TEXT("PlayerName"), Profile.PlayerName);
    Writer->WriteValue(TEXT("bIsLeftHanded"), Profile.bIsLeftHanded);
    Writer->WriteValue(TEXT("BatDimensions"), FString::Printf(TEXT("%f,%f,%f"), Profile.BatDimensions.X, Profile.BatDimensions.Y, Profile.BatDimensions.Z));
    Writer->WriteValue(TEXT("BatCenterOfMass"), FString::Printf(TEXT("%f,%f,%f"), Profile.BatCenterOfMass.X, Profile.BatCenterOfMass.Y, Profile.BatCenterOfMass.Z));
    Writer->WriteValue(TEXT("GripOffsetAlongHandle"), Profile.GripOffsetAlongHandle);
    Writer->WriteValue(TEXT("QualityScore"), Profile.QualityScore);
    Writer->WriteValue(TEXT("CalibrationDate"), Profile.CalibrationDate.ToIso8601());
    Writer->WriteValue(TEXT("DeviceId"), Profile.DeviceId);
    Writer->WriteValue(TEXT("AppVersion"), Profile.AppVersion);

    // Transform
    Writer->WriteValue(TEXT("PhoneToBatTransform_Location"), FString::Printf(TEXT("%f,%f,%f"), Profile.PhoneToBatTransform.GetLocation().X, Profile.PhoneToBatTransform.GetLocation().Y, Profile.PhoneToBatTransform.GetLocation().Z));
    Writer->WriteValue(TEXT("PhoneToBatTransform_Rotation"), FString::Printf(TEXT("%f,%f,%f,%f"), Profile.PhoneToBatTransform.GetRotation().X, Profile.PhoneToBatTransform.GetRotation().Y, Profile.PhoneToBatTransform.GetRotation().Z, Profile.PhoneToBatTransform.GetRotation().W));
    Writer->WriteValue(TEXT("PhoneToBatTransform_Scale"), FString::Printf(TEXT("%f,%f,%f"), Profile.PhoneToBatTransform.GetScale3D().X, Profile.PhoneToBatTransform.GetScale3D().Y, Profile.PhoneToBatTransform.GetScale3D().Z));

    Writer->WriteObjectEnd();
    Writer->Close();

    FString SavePath = FPaths::ProjectSavedDir() + TEXT("PhoneBatCalibration/") + SlotName + TEXT(".json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SavePath));
    return FFileHelper::SaveStringToFile(JsonString, *SavePath);
}

bool UPhoneBatCalibration::LoadProfile(FCalibrationProfile& OutProfile, const FString& SlotName)
{
    FString SavePath = FPaths::ProjectSavedDir() + TEXT("PhoneBatCalibration/") + SlotName + TEXT(".json");
    FString JsonString;

    if (!FFileHelper::LoadFileToString(JsonString, *SavePath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    OutProfile.ProfileId = JsonObject->GetStringField(TEXT("ProfileId"));
    OutProfile.PlayerName = JsonObject->GetStringField(TEXT("PlayerName"));
    OutProfile.bIsLeftHanded = JsonObject->GetBoolField(TEXT("bIsLeftHanded"));
    OutProfile.QualityScore = JsonObject->GetNumberField(TEXT("QualityScore"));
    OutProfile.CalibrationDate = FDateTime::ParseIso8601(*JsonObject->GetStringField(TEXT("CalibrationDate")));
    OutProfile.DeviceId = JsonObject->GetStringField(TEXT("DeviceId"));
    OutProfile.AppVersion = JsonObject->GetIntegerField(TEXT("AppVersion"));

    // Parse vectors
    auto ParseVector = [](const FString& Str) -> FVector
    {
        TArray<FString> Parts;
        Str.ParseIntoArray(Parts, TEXT(","), true);
        if (Parts.Num() == 3)
        {
            return FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
        }
        return FVector::ZeroVector;
    };

    auto ParseQuat = [](const FString& Str) -> FQuat
    {
        TArray<FString> Parts;
        Str.ParseIntoArray(Parts, TEXT(","), true);
        if (Parts.Num() == 4)
        {
            return FQuat(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]), FCString::Atof(*Parts[3]));
        }
        return FQuat::Identity;
    };

    OutProfile.BatDimensions = ParseVector(JsonObject->GetStringField(TEXT("BatDimensions")));
    OutProfile.BatCenterOfMass = ParseVector(JsonObject->GetStringField(TEXT("BatCenterOfMass")));
    OutProfile.GripOffsetAlongHandle = JsonObject->GetNumberField(TEXT("GripOffsetAlongHandle"));

    OutProfile.PhoneToBatTransform.SetLocation(ParseVector(JsonObject->GetStringField(TEXT("PhoneToBatTransform_Location"))));
    OutProfile.PhoneToBatTransform.SetRotation(ParseQuat(JsonObject->GetStringField(TEXT("PhoneToBatTransform_Rotation"))));
    OutProfile.PhoneToBatTransform.SetScale3D(ParseVector(JsonObject->GetStringField(TEXT("PhoneToBatTransform_Scale"))));

    return true;
}

TArray<FString> UPhoneBatCalibration::GetSavedProfileNames() const
{
    TArray<FString> ProfileNames;
    FString SaveDir = FPaths::ProjectSavedDir() + TEXT("PhoneBatCalibration/");

    IFileManager::Get().FindFiles(ProfileNames, *SaveDir, TEXT("*.json"));

    // Remove .json extension
    for (FString& Name : ProfileNames)
    {
        Name = FPaths::GetBaseFilename(Name);
    }

    return ProfileNames;
}

void UPhoneBatCalibration::StartValidation()
{
    bValidating = true;
    ValidationOrientationErrors.Empty();
    ValidationPositionErrors.Empty();
    Status = ECalibrationStatus::InProgress;
    OnStatusChanged.Broadcast(Status);
}

void UPhoneBatCalibration::RecordValidationSample(const FTransform& ExpectedTransform, const FTransform& ActualTransform)
{
    if (!bValidating)
    {
        return;
    }

    float OriError = CalculateOrientationError(ExpectedTransform.GetRotation(), ActualTransform.GetRotation());
    float PosError = CalculatePositionError(ExpectedTransform.GetLocation(), ActualTransform.GetLocation());

    ValidationOrientationErrors.Add(OriError);
    ValidationPositionErrors.Add(PosError);

    LastResult.ValidationSamples.Add(ActualTransform.GetLocation());
}

FCalibrationResult UPhoneBatCalibration::FinishValidation()
{
    bValidating = false;
    LastResult = CalculateResult();
    Status = LastResult.bSuccess ? ECalibrationStatus::Completed : ECalibrationStatus::Failed;
    OnStatusChanged.Broadcast(Status);
    OnComplete.Broadcast(LastResult);

    return LastResult;
}

FCalibrationStepData UPhoneBatCalibration::GetCurrentStepData() const
{
    if (CalibrationSteps.IsValidIndex(static_cast<int32>(CurrentStep)))
    {
        return CalibrationSteps[static_cast<int32>(CurrentStep)];
    }
    return FCalibrationStepData();
}

float UPhoneBatCalibration::GetOverallProgress() const
{
    int32 TotalSteps = static_cast<int32>(ECalibrationStep::Complete);
    int32 CurrentIndex = static_cast<int32>(CurrentStep);

    if (bValidating)
    {
        return 1.0f; // Validation is final phase
    }

    return TotalSteps > 0 ? static_cast<float>(CurrentIndex) / TotalSteps : 0.0f;
}

void UPhoneBatCalibration::SetupCalibrationSteps()
{
    CalibrationSteps.SetNum(static_cast<int32>(ECalibrationStep::Complete));

    // Step 0: Place phone flat
    CalibrationSteps[0] = FCalibrationStepData();
    CalibrationSteps[0].Step = ECalibrationStep::Step0_PlacePhoneFlat;
    CalibrationSteps[0].Instruction = TEXT("Place your phone flat on a level surface");
    CalibrationSteps[0].DetailInstruction = TEXT("Screen facing up, on a table or floor. This establishes the gravity reference frame.");
    CalibrationSteps[0].EstimatedDurationSeconds = 5.0f;
    CalibrationSteps[0].ValidationCriteria = { TEXT("Phone stationary"), TEXT("Z-axis aligned with gravity") };

    // Step 1: Hold in bat grip
    CalibrationSteps[1] = FCalibrationStepData();
    CalibrationSteps[1].Step = ECalibrationStep::Step1_HoldBatGrip;
    CalibrationSteps[1].Instruction = TEXT("Pick up phone and hold it like a cricket bat");
    CalibrationSteps[1].DetailInstruction = TEXT("Grip the phone as you would a bat handle. Top hand near top, bottom hand near bottom. Hold steady for 3 seconds.");
    CalibrationSteps[1].EstimatedDurationSeconds = 10.0f;
    CalibrationSteps[1].ValidationCriteria = { TEXT("Stable orientation"), TEXT("Grip matches expected angles") };

    // Step 2: Slow practice swing
    CalibrationSteps[2] = FCalibrationStepData();
    CalibrationSteps[2].Step = ECalibrationStep::Step2_SwingSlowly;
    CalibrationSteps[2].Instruction = TEXT("Perform a slow, controlled practice swing");
    CalibrationSteps[2].DetailInstruction = TEXT("Swing smoothly from backswing to follow-through. Focus on form, not speed. This calibrates the swing plane tracking.");
    CalibrationSteps[2].EstimatedDurationSeconds = 15.0f;
    CalibrationSteps[2].ValidationCriteria = { TEXT("Smooth swing arc"), TEXT("Consistent bat face angle") };

    // Step 3: Verify alignment
    CalibrationSteps[3] = FCalibrationStepData();
    CalibrationSteps[3].Step = ECalibrationStep::Step3_VerifyAlignment;
    CalibrationSteps[3].Instruction = TEXT("Verify bat alignment in the game view");
    CalibrationSteps[3].DetailInstruction = TEXT("Check that the virtual bat matches your phone orientation. Adjust grip if needed, then confirm.");
    CalibrationSteps[3].EstimatedDurationSeconds = 10.0f;
    CalibrationSteps[3].ValidationCriteria = { TEXT("Virtual bat matches real"), TEXT("No visible offset") };
    CalibrationSteps[3].bAutoAdvance = true;
}

FCalibrationStepData UPhoneBatCalibration::CreateStepData(ECalibrationStep Step) const
{
    if (CalibrationSteps.IsValidIndex(static_cast<int32>(Step)))
    {
        return CalibrationSteps[static_cast<int32>(Step)];
    }
    return FCalibrationStepData();
}

void UPhoneBatCalibration::SendStepToPhone(ECalibrationStep Step)
{
    if (!PhoneBatInput)
    {
        return;
    }

    FCalibrationStepData StepData = GetCurrentStepData();

    FPhoneBatCalibrationPayload Payload;
    Payload.Step = static_cast<int32>(Step);
    Payload.Instruction = StepData.Instruction;
    Payload.PhoneToBatTransform = CurrentProfile.PhoneToBatTransform;
    Payload.BatLength = CurrentProfile.BatDimensions;
    Payload.BatCenterOfMass = CurrentProfile.BatCenterOfMass;
    Payload.bComplete = (Step == ECalibrationStep::Complete);
    Payload.QualityScore = 0.0f; // Will be updated after validation

    // PhoneBatInput would send this via its receiver component
    // PhoneBatInput->ReceiverComponent->SendCalibrationStep(Payload);
}

void UPhoneBatCalibration::ProcessPhoneResponse(const FPhoneBatCalibrationPayload& Payload)
{
    // Called when phone sends calibration data back
    if (Payload.bComplete)
    {
        FinishCalibration();
    }
    else
    {
        // Phone completed a step
        CurrentProfile.PhoneToBatTransform = Payload.PhoneToBatTransform;
        AdvanceStep();
    }
}

void UPhoneBatCalibration::FinishCalibration()
{
    // Run validation automatically
    StartValidation();

    // In a real implementation, this would collect validation samples
    // For now, simulate validation
    FTransform IdentityTransform = FTransform::Identity;
    for (int32 i = 0; i < 10; i++)
    {
        FTransform TestTransform = IdentityTransform;
        TestTransform.SetRotation(FQuat(FRotator(FMath::RandRange(-2.0f, 2.0f), FMath::RandRange(-2.0f, 2.0f), FMath::RandRange(-2.0f, 2.0f))));
        TestTransform.SetLocation(FVector(FMath::RandRange(-1.0f, 1.0f), FMath::RandRange(-1.0f, 1.0f), FMath::RandRange(-1.0f, 1.0f)));
        RecordValidationSample(IdentityTransform, TestTransform);
    }

    LastResult = FinishValidation();

    if (LastResult.bSuccess)
    {
        // Apply calibration to PhoneBatInput
        if (PhoneBatInput)
        {
            // The calibration is applied via OnCalibrationComplete in PhoneBatInput
        }

        // Auto-save profile
        SaveProfile(CurrentProfile, CurrentProfile.PlayerName);
    }
}

FCalibrationResult UPhoneBatCalibration::CalculateResult()
{
    FCalibrationResult Result;
    Result.PhoneToBatTransform = CurrentProfile.PhoneToBatTransform;
    Result.BatLength = CurrentProfile.BatDimensions;
    Result.BatCenterOfMass = CurrentProfile.BatCenterOfMass;

    if (ValidationOrientationErrors.Num() > 0)
    {
        float SumOri = 0.0f;
        float SumPos = 0.0f;

        for (float Err : ValidationOrientationErrors)
        {
            SumOri += Err;
        }
        for (float Err : ValidationPositionErrors)
        {
            SumPos += Err;
        }

        Result.AverageOrientationError = SumOri / ValidationOrientationErrors.Num();
        Result.AveragePositionError = SumPos / ValidationPositionErrors.Num();

        // Quality score: inverse of errors, clamped
        float OriScore = FMath::Clamp(1.0f - Result.AverageOrientationError / 10.0f, 0.0f, 1.0f); // 10 deg = 0 score
        float PosScore = FMath::Clamp(1.0f - Result.AveragePositionError / 5.0f, 0.0f, 1.0f); // 5 cm = 0 score
        Result.QualityScore = (OriScore + PosScore) / 2.0f;

        Result.bSuccess = Result.QualityScore > 0.7f; // Minimum 70% quality

        if (!Result.bSuccess)
        {
            Result.ErrorMessage = FString::Printf(TEXT("Calibration quality too low: %.1f%% (orientation error: %.1f°, position error: %.1fcm)"),
                Result.QualityScore * 100.0f, Result.AverageOrientationError, Result.AveragePositionError);
        }
    }
    else
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("No validation samples collected");
    }

    return Result;
}

float UPhoneBatCalibration::CalculateOrientationError(const FQuat& Expected, const FQuat& Actual) const
{
    FQuat Delta = Expected.Inverse() * Actual;
    float Angle;
    FVector Axis;
    Delta.ToAxisAndAngle(Axis, Angle);
    return FMath::RadiansToDegrees(FMath::Abs(Angle));
}

float UPhoneBatCalibration::CalculatePositionError(const FVector& Expected, const FVector& Actual) const
{
    return FVector::Dist(Expected, Actual);
}