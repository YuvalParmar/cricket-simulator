#include "PhoneBatLinkModule.h"

#define LOCTEXT_NAMESPACE "FPhoneBatLinkModule"

void FPhoneBatLinkModule::StartupModule()
{
    // Initialize UDP socket listener, Bluetooth LE manager
    // Register phone bat receiver component factory
    // Start discovery service (mDNS/Bonjour)
}

void FPhoneBatLinkModule::ShutdownModule()
{
    // Close sockets, stop Bluetooth services
    // Clean up network resources
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPhoneBatLinkModule, PhoneBatLink)