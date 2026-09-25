#pragma once

#include <NitroModules/Promise.hpp>
#include <cstdint>

namespace arcore {
std::shared_ptr<margelo::nitro::Promise<bool>> initializeARCore(int64_t runtimeId);
void registerARCoreInitializationNatives();
}
