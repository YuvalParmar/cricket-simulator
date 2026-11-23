#include "CricketSimulatorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FCricketSimulatorModule"

void FCricketSimulatorModule::StartupModule()
{
    // Register game feature actions, settings, etc.
    UE_LOG(LogTemp, Log, TEXT("CricketSimulator module started"));
}

void FCricketSimulatorModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("CricketSimulator module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCricketSimulatorModule, CricketSimulator)