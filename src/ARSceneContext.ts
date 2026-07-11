import { createContext } from 'react';
import type { ARObjectDescriptor } from './ARView.nitro';

export type ARSceneContextType = {
  registerObject: (desc: ARObjectDescriptor) => void;
  updateObject: (desc: ARObjectDescriptor) => void;
  unregisterObject: (id: string) => void;
};

export const ARSceneContext = createContext<ARSceneContextType | null>(null);
