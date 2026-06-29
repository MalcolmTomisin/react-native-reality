#include "HybridCrossPlatformArCore.hpp"
#include "ARSessionManager.h"
#include "arcore_c_api.h"

namespace margelo::nitro::arcore {

std::shared_ptr<Promise<bool>> HybridCrossPlatformArCore::initialize()
{
  return Promise<bool>::async([]() {
    return true;
  });
}

bool HybridCrossPlatformArCore::isDepthModeSupported()
{
  ArSession *session = ArSessionManager::Instance().Get();
  if (session == nullptr)
    return false;
  int32_t is_supported = 0;
  ArSession_isDepthModeSupported(session, AR_DEPTH_MODE_AUTOMATIC, &is_supported);
  return is_supported != 0;
}

bool HybridCrossPlatformArCore::isGeospatialModeSupported()
{
  // Geospatial mode requires ARCore 1.31+ and device support.
  // For now, return false as this feature requires additional configuration.
  return false;
}

} // namespace margelo::nitro::arcore
