import { NitroModules } from 'react-native-nitro-modules';
import type { ArCore } from './ArCore.nitro';

const ArCoreHybridObject =
  NitroModules.createHybridObject<ArCore>('ArCore');

export function multiply(a: number, b: number): number {
  return ArCoreHybridObject.multiply(a, b);
}
