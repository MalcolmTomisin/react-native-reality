/**
 * Canonical cross-platform string vocabularies emitted by the native layers.
 *
 * These are documentation/ergonomic aliases only — the Nitro callback signatures
 * intentionally use plain `string`, but iOS (ARKit) and Android (ARCore) both emit
 * exactly these values so JS code can rely on one vocabulary across platforms.
 */

/** Value passed to `onSessionStateChange`. */
export type ARSessionState =
  | 'initializing'
  | 'ready'
  | 'paused'
  | 'destroying'
  | 'destroyed'
  | 'failed';

/** `ARTrackingStateInfo.state` — the current camera tracking quality. */
export type ARTrackingState = 'tracking' | 'limited' | 'unavailable';

/** `ARTrackingStateInfo.reason` — why tracking is `limited`/`unavailable` (omitted when `tracking`). */
export type ARTrackingReason =
  | 'initializing'
  | 'excessiveMotion'
  | 'insufficientFeatures'
  | 'insufficientLight'
  | 'relocalizing'
  | 'cameraUnavailable'
  | 'badState';

/** `ARPlaneInfo.type` — plane orientation (common denominator across platforms). */
export type ARPlaneType = 'horizontal' | 'vertical';
