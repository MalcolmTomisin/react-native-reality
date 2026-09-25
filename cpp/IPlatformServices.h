#pragma once

#include "arcore_c_api.h"

struct ARCoreInstallResult {
    ArStatus status;
    ArInstallStatus installation;
};

class IPlatformServices {
public:
    virtual ~IPlatformServices() = default;
    virtual ArAvailability checkAvailability() = 0;
    virtual ARCoreInstallResult requestInstall(int taskId, bool userRequested) = 0;
    virtual bool hasResumedHost(int taskId) = 0;
    virtual bool isGooglePlayServicesAvailable() = 0;
};
