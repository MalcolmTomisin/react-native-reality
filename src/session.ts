import { NativeModules, Platform } from 'react-native';
import { NitroModules } from 'react-native-nitro-modules';
import type { CrossPlatformArCore } from './ArCore.nitro';

const ArCoreModule = NitroModules.createHybridObject<CrossPlatformArCore>(
  'CrossPlatformArCore'
);

export function initialize(): Promise<boolean> {
  return ArCoreModule.initialize(
    Platform.OS === 'android' ? NativeModules.ARCoreLifecycle.runtimeId : 0
  );
}

export function isDepthModeSupported(): boolean {
  return ArCoreModule.isDepthModeSupported();
}

export function isGeospatialModeSupported(): boolean {
  return ArCoreModule.isGeospatialModeSupported();
}
