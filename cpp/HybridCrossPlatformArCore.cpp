#include "HybridCrossPlatformArCore.hpp"

#ifdef __ANDROID__
#include "ARSessionManager.h"
#include "arcore_c_api.h"
#endif

namespace margelo::nitro::arcore {

std::shared_ptr<Promise<bool>> HybridCrossPlatformArCore::initialize()
{
  return Promise<bool>::async([]() {
    return true;
  });
}

bool HybridCrossPlatformArCore::isDepthModeSupported()
{
#ifdef __ANDROID__
  ArSession *session = ArSessionManager::Instance().Get();
  if (session == nullptr)
    return false;
  int32_t is_supported = 0;
  ArSession_isDepthModeSupported(session, AR_DEPTH_MODE_AUTOMATIC, &is_supported);
  return is_supported != 0;
#else
  return false;
#endif
}

bool HybridCrossPlatformArCore::isGeospatialModeSupported()
{
  return false;
}

} // namespace margelo::nitro::arcore
