#pragma once

class IPlatformServices {
public:
    virtual ~IPlatformServices() = default;
    
    // Pure virtual method - must be implemented by Android/iOS specific code
    virtual bool isGooglePlayServicesAvailable() = 0;
};