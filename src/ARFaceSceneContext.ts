import { createContext } from 'react';
import type { ARFaceFilterDescriptor } from './ARView.nitro';

export type ARFaceSceneContextType = {
  registerFilter: (desc: ARFaceFilterDescriptor) => void;
  updateFilter: (desc: ARFaceFilterDescriptor) => void;
  unregisterFilter: (id: string) => void;
};

export const ARFaceSceneContext = createContext<ARFaceSceneContextType | null>(
  null
);
