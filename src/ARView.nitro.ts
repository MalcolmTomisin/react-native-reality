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

export type ARFaceAttachmentPoint =
  | 'forehead'
  | 'foreheadLeft'
  | 'foreheadRight'
  | 'noseTip'
  | 'noseBridge'
  | 'leftEye'
  | 'rightEye'
  | 'leftEar'
  | 'rightEar'
  | 'chin'
  | 'mouthCenter';

// --- Shared structs ---

export interface ARVector3 {
  x: number;
  y: number;
  z: number;
}

export interface ARVector4 {
  x: number;
  y: number;
  z: number;
  w: number;
}

export interface ARColor {
  r: number;
  g: number;
  b: number;
  a: number;
}

// --- Descriptor structs (JS → Native) ---

export interface ARObjectDescriptor {
  id: string;
  anchorId: string;
  model: string;
  texture?: string;
  scale?: ARVector3;
  rotation?: ARVector4;
  color?: ARColor;
  visible?: boolean;
}

export interface ARFaceFilterDescriptor {
  id: string;
  attachmentPoint: string;
  model: string;
  scale?: ARVector3;
  offset?: ARVector3;
  rotation?: ARVector3;
  visible?: boolean;
}

// --- Event data structs ---

export interface ARError {
  code: string;
  message: string;
}

export interface ARTrackingStateInfo {
  /** Canonical `ARTrackingState`: 'tracking' | 'limited' | 'unavailable'. */
  state: string;
  /** Canonical `ARTrackingReason` (omitted when 'tracking'). */
  reason?: string;
}

export interface ARPlaneInfo {
  id: string;
  /** Canonical `ARPlaneType`: 'horizontal' | 'vertical'. */
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

export interface ARTapResult {
  x: number;
  y: number;
  hasHit: boolean;
  anchorId?: string;
}

export interface ARFaceInfo {
  faceId: string;
  transform: number[];
}

export interface ARBlendShapes {
  faceId: string;
  shapeKeys: string[];
  shapeValues: number[];
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
  debugShowFaceMesh?: boolean;
  objects?: ARObjectDescriptor[];
  faceFilters?: ARFaceFilterDescriptor[];
  faceTextureURI?: string;

  /** Fires on session lifecycle transitions. `state` is a canonical `ARSessionState`. */
  onSessionStateChange?: (state: string) => void;
  /** Fires when a session (re)configures, reporting the active `ARSessionType` ('world' | 'face'). */
  onSessionTypeChange?: (type: string) => void;
  onARCoreError?: (error: ARError) => void;
  onTrackingStateChange?: (state: ARTrackingStateInfo) => void;
  onPlaneDetected?: (plane: ARPlaneInfo) => void;
  onPlaneUpdated?: (plane: ARPlaneInfo) => void;
  onAnchorCreated?: (anchor: ARAnchorResult) => void;
  onTap?: (result: ARTapResult) => void;
  onFaceDetected?: (face: ARFaceInfo) => void;
  onFaceUpdated?: (face: ARFaceInfo) => void;
  onFaceLost?: (faceId: string) => void;
  onBlendShapesUpdate?: (shapes: ARBlendShapes) => void;
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
