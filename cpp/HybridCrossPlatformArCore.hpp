#pragma once

#include <memory>
#include "HybridCrossPlatformArCoreSpec.hpp"

namespace margelo::nitro::arcore {

class HybridCrossPlatformArCore : public HybridCrossPlatformArCoreSpec
{
public:
  HybridCrossPlatformArCore() : HybridObject(TAG) {}

  std::shared_ptr<Promise<bool>> initialize(double runtimeId) override;
  bool isDepthModeSupported() override;
  bool isGeospatialModeSupported() override;
};

} // namespace margelo::nitro::arcore
