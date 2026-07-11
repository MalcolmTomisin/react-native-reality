import type {
  HybridView,
  HybridViewProps,
  HybridViewMethods,
} from 'react-native-nitro-modules';

// --- Enums ---

export type ARSessionType = 'world' | 'face';
export type ARDepthMode = 'disabled' | 'automatic' | 'raw' | 'geospatial';
export type ARLightEstimationMode =
  | 'disabled'
  | 'ambientIntensity'
  | 'environmentalHDR';
export type ARPlaneDetectionMode = 'horizontal' | 'vertical' | 'both' | 'none';
export type ARFocusMode = 'auto' | 'fixed';
export type ARShaderMode = 'camera' | 'depth';
export type ARCameraFacing = 'back' | 'front';
export type ARCameraTargetFps = 'fps30' | 'fps60';
export type ARCameraDepthSensorUsage =
  | 'doNotUse'
  | 'useIfAvailable'
  | 'requireAndUse';
export type ARCloudAnchorMode = 'disabled' | 'enabled';
export type ARInstantPlacementMode = 'disabled' | 'enabled';

// --- Event data structs ---

export interface ARError {
  code: string;
  message: string;
}

export interface ARTrackingStateInfo {
  state: string;
  reason?: string;
}

export interface ARPlaneInfo {
  id: string;
  type: string;
  centerX: number;
  centerY: number;
  centerZ: number;
  extentX: number;
  extentZ: number;
}

export interface ARAnchorResult {
  anchorId: string;
  x: number;
  y: number;
  z: number;
}

export interface ARHitTestResult {
  x: number;
  y: number;
  z: number;
  hasHit: boolean;
}

// --- View Props ---

export interface ARViewProps extends HybridViewProps {
  sessionType?: ARSessionType;
  depthMode?: ARDepthMode;
  lightEstimationMode?: ARLightEstimationMode;
  planeDetectionMode?: ARPlaneDetectionMode;
  focusMode?: ARFocusMode;
  shaderMode?: ARShaderMode;
  cameraFacing?: ARCameraFacing;
  cameraTargetFps?: ARCameraTargetFps;
  cameraDepthSensorUsage?: ARCameraDepthSensorUsage;
  cloudAnchorMode?: ARCloudAnchorMode;
  instantPlacementMode?: ARInstantPlacementMode;
  paused?: boolean;
  debugShowPlanes?: boolean;
  debugShowPointCloud?: boolean;
  debugShowWorldOrigin?: boolean;
  debugShowDepthMap?: boolean;
  objectsJSON?: string;

  onARCoreError?: (error: ARError) => void;
  onTrackingStateChange?: (state: ARTrackingStateInfo) => void;
  onPlaneDetected?: (plane: ARPlaneInfo) => void;
  onPlaneUpdated?: (plane: ARPlaneInfo) => void;
  onAnchorCreated?: (anchor: ARAnchorResult) => void;
}

// --- View Methods ---

export interface ARViewMethods extends HybridViewMethods {
  resetSession(): void;
  destroySession(): void;
  cameraPermissionGranted(): void;
  hitTest(x: number, y: number): Promise<ARHitTestResult>;
  createAnchor(x: number, y: number): Promise<string>;
  removeAnchor(anchorId: string): void;
  takeSnapshot(saveToDisk: boolean): Promise<string>;
}

// --- HybridView type ---

export type ARViewHybrid = HybridView<
  ARViewProps,
  ARViewMethods,
  { android: 'kotlin'; ios: 'swift' }
>;
