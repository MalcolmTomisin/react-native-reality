# react-native-reality

Native augmented reality for React Native — world & face tracking, plane detection, hit-testing, and 3D content on iOS (ARKit / RealityKit) and Android (ARCore), powered by [Nitro Modules](https://nitro.margelo.com/).

One declarative `<ARView>` component, a small set of child components for content, and typed callbacks for everything the AR session reports — with the same JavaScript API on both platforms.

## Features

- **World tracking** — plane detection, hit-testing, world anchors, and anchored 3D models (`<ARObject>`).
- **Face tracking** — augmented faces with 3D filters attached to facial landmarks (`<ARFaceFilter>`), plus blend shapes (iOS).
- **Camera is driven by the session type** — `sessionType="world"` uses the back camera, `sessionType="face"` uses the front camera. Switching at runtime reconfigures the session in place.
- **Rich session events** — session lifecycle, tracking state, planes, taps, anchors, and face events, all as typed callbacks.
- **No JSON bridge** — all data crosses the native boundary as Nitro structs.
- **Cross-platform, consistent vocabulary** — session/tracking/plane strings are normalized so JS code doesn't branch per platform.

## Requirements

| | Minimum |
|---|---|
| iOS | 14.0, a device with ARKit (A9+); face tracking needs a TrueDepth camera |
| Android | API 24 (Android 7.0), an [ARCore-supported device](https://developers.google.com/ar/devices); ARCore 1.33 is bundled |
| Peer dependency | `react-native-nitro-modules` |

> AR does not run in the iOS Simulator or Android emulator — test on a physical device.

## Installation

```sh
npm install react-native-reality react-native-nitro-modules
# or
yarn add react-native-reality react-native-nitro-modules
```

`react-native-nitro-modules` is required — this library is built on [Nitro Modules](https://nitro.margelo.com/).

### iOS

```sh
cd ios && pod install
```

Add a camera usage description to your app's `Info.plist`:

```xml
<key>NSCameraUsageDescription</key>
<string>This app uses the camera for augmented reality.</string>
```

### Android

The library manifest already declares the `CAMERA` permission and the `com.google.ar.core` "required" meta-data, so they merge into your app automatically. You still need to **request the camera permission at runtime** before starting a session (see below). To make ARCore optional instead of required, override the meta-data in your app manifest with `android:value="optional"`.

## Quick start

```tsx
import { useEffect, useRef, useState } from 'react';
import { PermissionsAndroid, Platform, StyleSheet } from 'react-native';
import {
  ARView,
  ARObject,
  initialize,
  type ARViewHandle,
  type ARAnchorResult,
} from 'react-native-reality';

export default function App() {
  const ref = useRef<ARViewHandle>(null);
  const [ready, setReady] = useState(false);
  const [anchors, setAnchors] = useState<ARAnchorResult[]>([]);

  // Check AR availability once.
  useEffect(() => {
    initialize().then(setReady);
  }, []);

  // Request the camera permission, then tell the view it can start.
  useEffect(() => {
    (async () => {
      if (Platform.OS === 'android') {
        const status = await PermissionsAndroid.request(
          PermissionsAndroid.PERMISSIONS.CAMERA
        );
        if (status !== PermissionsAndroid.RESULTS.GRANTED) return;
      }
      ref.current?.cameraPermissionGranted();
    })();
  }, []);

  if (!ready) return null;

  return (
    <ARView
      ref={ref}
      style={StyleSheet.absoluteFill}
      sessionType="world"
      planeDetectionMode="both"
      debugShowPlanes
      onAnchorCreated={(a) => setAnchors((prev) => [...prev, a])}
      onTap={(t) => console.log('tap', t.x, t.y, 'hit:', t.hasHit)}
    >
      {anchors.map((a) => (
        <ARObject
          key={a.anchorId}
          anchorId={a.anchorId}
          model="andy"
          scale={{ x: 0.1, y: 0.1, z: 0.1 }}
        />
      ))}
    </ARView>
  );
}
```

Tapping a detected plane creates an anchor (`onAnchorCreated`); rendering an `<ARObject>` at that `anchorId` places a model there.

## Usage

### Starting a session

`initialize()` checks AR availability and should resolve before you render an `<ARView>`. The view starts its session once the camera permission is granted — call `ref.current.cameraPermissionGranted()` after you've obtained it (on Android request it with `PermissionsAndroid`; on iOS ARKit prompts using your `NSCameraUsageDescription`).

### World tracking

```tsx
<ARView
  sessionType="world"
  planeDetectionMode="both"          // 'horizontal' | 'vertical' | 'both' | 'none'
  depthMode="automatic"
  lightEstimationMode="environmentalHDR"
  onPlaneDetected={(p) => console.log('plane', p.type, p.extentX, p.extentZ)}
  onTrackingStateChange={(s) => console.log(s.state, s.reason)}
/>
```

Place content by creating anchors (via taps or `createAnchor`) and rendering an `<ARObject>` for each:

```tsx
<ARObject anchorId={anchor.anchorId} model="andy" scale={{ x: 0.1, y: 0.1, z: 0.1 }} />
```

### Face tracking

Set `sessionType="face"` (front camera) and add `<ARFaceFilter>` children anchored to facial landmarks:

```tsx
<ARView
  sessionType="face"
  onFaceDetected={(f) => console.log('face', f.faceId)}
  onBlendShapesUpdate={(b) => console.log(b.shapeKeys, b.shapeValues)} // iOS only
>
  <ARFaceFilter attachmentPoint="noseTip" model="nose" scale={{ x: 1, y: 1, z: 1 }} />
  <ARFaceFilter attachmentPoint="foreheadLeft" model="hat" />
</ARView>
```

### Switching between world and face

Toggle the `sessionType` prop on the **same** `<ARView>` — the native layer reconfigures the session and camera in place (no remount needed). Because the session that actually comes up can differ from the request (e.g. a device without a TrueDepth camera falls back to world), listen to `onSessionTypeChange` for the *active* type:

```tsx
const [mode, setMode] = useState<'world' | 'face'>('world');
// ...
<ARView
  sessionType={mode}
  onSessionStateChange={(s) => console.log('session:', s)}    // initializing → ready → …
  onSessionTypeChange={(t) => console.log('active type:', t)} // 'world' | 'face'
/>
```

> Hit-testing and taps are **world-only** — in a face session the AR view ignores taps (your overlay buttons keep working), and `hitTest`/`createAnchor` resolve to "no hit".

### Imperative methods

Call these on the `ref`:

```tsx
const ref = useRef<ARViewHandle>(null);

await ref.current?.hitTest(x, y);        // { x, y, z, hasHit }
const id = await ref.current?.createAnchor(x, y);
ref.current?.removeAnchor(id);
const snap = await ref.current?.takeSnapshot(false); // base64 (file path if `true`)
ref.current?.resetSession();             // clears anchors/tracking, restarts
ref.current?.destroySession();
```

### Model assets

The `model` prop is a base name resolved per platform — add the asset to each app:

- **Android** — `android/app/src/main/assets/models/<name>.obj` (+ optional `<name>.png` texture).
- **iOS** — add `<name>.usdz` to the app bundle (or an `ArCoreAssets.bundle`).

So `model="andy"` loads `models/andy.obj` on Android and `andy.usdz` on iOS.

## API reference

### Functions

| Function | Returns | Description |
|---|---|---|
| `initialize()` | `Promise<boolean>` | Checks AR availability; resolve before rendering an `ARView`. |
| `isDepthModeSupported()` | `boolean` | Whether the device supports depth. |
| `isGeospatialModeSupported()` | `boolean` | Whether the device supports geospatial mode. |

### `<ARView>`

Accepts all `ARViewProps` below plus standard `ViewProps` (e.g. `style`) and `children` (`<ARObject>` / `<ARFaceFilter>`). Attach a `ref` typed as `ARViewHandle` for the imperative methods.

**Configuration props**

| Prop | Type | Notes |
|---|---|---|
| `sessionType` | `'world' \| 'face'` | Drives the camera (world→back, face→front). Default `'world'`. |
| `planeDetectionMode` | `'horizontal' \| 'vertical' \| 'both' \| 'none'` | World only. |
| `depthMode` | `'disabled' \| 'automatic' \| 'raw' \| 'geospatial'` | |
| `lightEstimationMode` | `'disabled' \| 'ambientIntensity' \| 'environmentalHDR'` | |
| `focusMode` | `'auto' \| 'fixed'` | |
| `shaderMode` | `'camera' \| 'depth'` | |
| `cameraFacing` | `'back' \| 'front'` | Optional override; normally inferred from `sessionType`. |
| `cameraTargetFps` | `'fps30' \| 'fps60'` | |
| `cameraDepthSensorUsage` | `'doNotUse' \| 'useIfAvailable' \| 'requireAndUse'` | |
| `cloudAnchorMode` | `'disabled' \| 'enabled'` | |
| `instantPlacementMode` | `'disabled' \| 'enabled'` | |
| `paused` | `boolean` | Pause/resume the session. |
| `faceTextureURI` | `string` | Texture for a full face-mesh overlay. |
| `debugShowPlanes` / `debugShowPointCloud` / `debugShowWorldOrigin` / `debugShowDepthMap` / `debugShowFaceMesh` | `boolean` | Debug overlays. |

**Callbacks**

| Prop | Payload | Fires when |
|---|---|---|
| `onSessionStateChange` | `state: ARSessionState` | Session lifecycle transitions. |
| `onSessionTypeChange` | `type: 'world' \| 'face'` | A session (re)configures — the *active* type. |
| `onTrackingStateChange` | `ARTrackingStateInfo` | Tracking quality changes. |
| `onPlaneDetected` / `onPlaneUpdated` | `ARPlaneInfo` | A plane is found / updated (world). |
| `onAnchorCreated` | `ARAnchorResult` | An anchor is created (e.g. from a tap on a plane). |
| `onTap` | `ARTapResult` | Any screen tap (world); includes `hasHit` and `anchorId?`. |
| `onFaceDetected` / `onFaceUpdated` | `ARFaceInfo` | A face enters / updates (face). |
| `onFaceLost` | `faceId: string` | A tracked face leaves. |
| `onBlendShapesUpdate` | `ARBlendShapes` | Per-frame expression coefficients (**iOS only**). |
| `onARCoreError` | `ARError` | An error occurs (e.g. permission denied). |

### `ARViewHandle` (ref)

| Method | Signature |
|---|---|
| `cameraPermissionGranted` | `() => void` |
| `hitTest` | `(x, y) => Promise<ARHitTestResult>` |
| `createAnchor` | `(x, y) => Promise<string>` |
| `removeAnchor` | `(anchorId) => void` |
| `takeSnapshot` | `(saveToDisk: boolean) => Promise<string>` — base64, or a file path when `true` |
| `resetSession` | `() => void` |
| `destroySession` | `() => void` |

### `<ARObject>` (world content)

Renders a model at a world anchor. Props (`ARObjectDescriptor` without `id`):

| Prop | Type | |
|---|---|---|
| `anchorId` | `string` | Anchor to attach to (required). |
| `model` | `string` | Model base name (required). |
| `texture` | `string?` | Optional texture name. |
| `scale` | `ARVector3?` | |
| `rotation` | `ARVector4?` | Quaternion `{ x, y, z, w }`. |
| `color` | `ARColor?` | `{ r, g, b, a }`. |
| `visible` | `boolean?` | |

### `<ARFaceFilter>` (face content)

Renders a model at a facial landmark. Props (`ARFaceFilterDescriptor` without `id`):

| Prop | Type | |
|---|---|---|
| `attachmentPoint` | `ARFaceAttachmentPoint` | `forehead`, `foreheadLeft`/`foreheadRight`, `noseTip`, `noseBridge`, `leftEye`/`rightEye`, `leftEar`/`rightEar`, `chin`, `mouthCenter`. |
| `model` | `string` | Model base name (required). |
| `scale` | `ARVector3?` | |
| `offset` | `ARVector3?` | Local offset from the landmark. |
| `rotation` | `ARVector3?` | Euler radians `{ x, y, z }`. |
| `visible` | `boolean?` | |

### Canonical string vocabularies

Emitted identically on both platforms:

- **`ARSessionState`** — `'initializing' | 'ready' | 'paused' | 'destroying' | 'destroyed' | 'failed'`
- **`ARTrackingState`** — `'tracking' | 'limited' | 'unavailable'`
- **`ARTrackingReason`** — `'initializing' | 'excessiveMotion' | 'insufficientFeatures' | 'insufficientLight' | 'relocalizing' | 'cameraUnavailable' | 'badState'`
- **`ARPlaneType`** — `'horizontal' | 'vertical'`

### Data types

`ARVector3`, `ARVector4`, `ARColor`, `ARObjectDescriptor`, `ARFaceFilterDescriptor`, `ARError`, `ARTrackingStateInfo`, `ARPlaneInfo`, `ARAnchorResult`, `ARHitTestResult`, `ARTapResult`, `ARFaceInfo`, `ARBlendShapes` — all exported from the package.

## Platform differences

| | iOS (ARKit) | Android (ARCore) |
|---|---|---|
| Blend shapes | ✅ `onBlendShapesUpdate` (52 coefficients) | Not available |
| Face mesh vertices | ~1220 | ~468 |
| Models | `.usdz` | `.obj` (+ `.png`) |

In a **face** session `onTrackingStateChange` reflects whether a *face* is being tracked (world/motion tracking isn't performed on the front camera).

## Contributing

- [Development workflow](CONTRIBUTING.md#development-workflow)
- [Sending a pull request](CONTRIBUTING.md#sending-a-pull-request)
- [Code of conduct](CODE_OF_CONDUCT.md)

## License

MIT

---

Made with [create-react-native-library](https://github.com/callstack/react-native-builder-bob)
