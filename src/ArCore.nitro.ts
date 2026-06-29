import type { HybridObject } from 'react-native-nitro-modules';

export interface CrossPlatformArCore
  extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  initialize(): Promise<boolean>;
  isDepthModeSupported(): boolean;
  isGeospatialModeSupported(): boolean;
}
