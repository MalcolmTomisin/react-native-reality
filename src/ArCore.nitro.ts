import type { HybridObject } from 'react-native-nitro-modules';

export interface CrossPlatformArCore
  extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  initialize(runtimeId: number): Promise<boolean>;
  isDepthModeSupported(): boolean;
  isGeospatialModeSupported(): boolean;
}
