import { createContext } from 'react';

export type ARObjectDescriptor = {
  id: string;
  anchorId: string;
  model: string;
  texture?: string;
  scale?: { x: number; y: number; z: number };
  rotation?: { x: number; y: number; z: number; w: number };
  color?: { r: number; g: number; b: number; a: number };
  visible?: boolean;
};

export type ARSceneContextType = {
  registerObject: (desc: ARObjectDescriptor) => void;
  updateObject: (desc: ARObjectDescriptor) => void;
  unregisterObject: (id: string) => void;
};

export const ARSceneContext = createContext<ARSceneContextType | null>(null);
