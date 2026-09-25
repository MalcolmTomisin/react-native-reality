#pragma once

#include <fbjni/fbjni.h>
#include "IPlatformServices.h"

class AndroidPlatformServices : public IPlatformServices {
public:
    ArAvailability checkAvailability() override;
    ARCoreInstallResult requestInstall(int taskId, bool userRequested) override;
    bool hasResumedHost(int taskId) override;
    bool isGooglePlayServicesAvailable() override;
    bool checkARCoreInstallation(JNIEnv* env, jobject context, jobject activity);
};
