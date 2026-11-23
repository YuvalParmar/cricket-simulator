#pragma once

#include "CoreMinimal.h"
#include "GameplayTags.h"

// Shared gameplay enums
UENUM(BlueprintType)
enum class EShotOutcome : uint8
{
    None            UMETA(DisplayName = "None"),
    Missed          UMETA(DisplayName = "Missed"),
    Edged           UMETA(DisplayName = "Edged"),
    ThickEdge       UMETA(DisplayName = "Thick Edge"),
    TopEdge         UMETA(DisplayName = "Top Edge"),
    InsideEdge      UMETA(DisplayName = "Inside Edge"),
    LeadingEdge     UMETA(DisplayName = "Leading Edge"),
    PlayAndMiss     UMETA(DisplayName = "Play and Miss"),
    InsideEdgePad   UMETA(DisplayName = "Inside Edge Pad"),
    Pad             UMETA(DisplayName = "Pad"),
    Gloved          UMETA(DisplayName = "Gloved"),
    CaughtBehind    UMETA(DisplayName = "Caught Behind"),
    Caught          UMETA(DisplayName = "Caught"),
    Bowled          UMETA(DisplayName = "Bowled"),
    LBW             UMETA(DisplayName = "LBW"),
    Stumped         UMETA(DisplayName = "Stumped"),
    RunOut          UMETA(DisplayName = "Run Out"),
    HitWicket       UMETA(DisplayName = "Hit Wicket"),
    Single          UMETA(DisplayName = "Single"),
    Two             UMETA(DisplayName = "Two"),
    Three           UMETA(DisplayName = "Three"),
    Four            UMETA(DisplayName = "Four"),
    Six             UMETA(DisplayName = "Six"),
    Byes            UMETA(DisplayName = "Byes"),
    LegByes         UMETA(DisplayName = "Leg Byes"),
    Wides           UMETA(DisplayName = "Wides"),
    NoBall          UMETA(DisplayName = "No Ball"),
    DeadBall        UMETA(DisplayName = "Dead Ball"),
    Overthrow       UMETA(DisplayName = "Overthrow"),
    FreeHit         UMETA(DisplayName = "Free Hit")
};

UENUM(BlueprintType)
enum class EFootworkType : uint8
{
    None            UMETA(DisplayName = "None"),
    ForwardPress    UMETA(DisplayName = "Forward Press"),
    BackFoot        UMETA(DisplayName = "Back Foot"),
    StepOut         UMETA(DisplayName = "Step Out"),
    ShuffleAcross   UMETA(DisplayName = "Shuffle Across"),
    OpenStance      UMETA(DisplayName = "Open Stance")
};