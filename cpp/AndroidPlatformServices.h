#pragma once

#include <fbjni/fbjni.h>
#include "IPlatformServices.h"
#include "arcore_c_api.h"

class AndroidPlatformServices : public IPlatformServices {
public:

    bool isGooglePlayServicesAvailable() override;
    bool checkARCoreInstallation(JNIEnv* env, jobject context, jobject activity);
    
};