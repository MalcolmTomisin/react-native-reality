import React, {
  forwardRef,
  useCallback,
  useImperativeHandle,
  useMemo,
  useRef,
  useState,
} from 'react';
import type { ViewProps } from 'react-native';
import {
  getHostComponent,
  callback,
  type ViewConfig,
} from 'react-native-nitro-modules';
import type {
  ARViewProps,
  ARViewMethods,
  ARViewHybrid,
  ARObjectDescriptor,
  ARFaceFilterDescriptor,
} from './ARView.nitro';
import { ARSceneContext } from './ARSceneContext';
import { ARFaceSceneContext } from './ARFaceSceneContext';

const noop = (() => {}) as (...args: any[]) => void;

const NativeARView = getHostComponent<ARViewProps, ARViewMethods>(
  'ARViewHybrid',
  (): ViewConfig<ARViewProps> => ({
    uiViewClassName: 'ARViewHybrid',
    bubblingEventTypes: {},
    directEventTypes: {},
    validAttributes: {
      sessionType: true,
      depthMode: true,
      lightEstimationMode: true,
      planeDetectionMode: true,
      focusMode: true,
      shaderMode: true,
      cameraFacing: true,
      cameraTargetFps: true,
      cameraDepthSensorUsage: true,
      cloudAnchorMode: true,
      instantPlacementMode: true,
      paused: true,
      debugShowPlanes: true,
      debugShowPointCloud: true,
      debugShowWorldOrigin: true,
      debugShowDepthMap: true,
      debugShowFaceMesh: true,
      objects: true,
      faceFilters: true,
      faceTextureURI: true,
      onSessionStateChange: true,
      onSessionTypeChange: true,
      onARCoreError: true,
      onTrackingStateChange: true,
      onPlaneDetected: true,
      onPlaneUpdated: true,
      onAnchorCreated: true,
      onTap: true,
      onFaceDetected: true,
      onFaceUpdated: true,
      onFaceLost: true,
      onBlendShapesUpdate: true,
    },
  })
);

export type ARViewHandle = {
  takeSnapshot: (saveToDisk: boolean) => Promise<string>;
  resetSession: () => void;
  destroySession: () => void;
  cameraPermissionGranted: () => void;
  hitTest: (
    x: number,
    y: number
  ) => Promise<{ x: number; y: number; z: number; hasHit: boolean }>;
  createAnchor: (x: number, y: number) => Promise<string>;
  removeAnchor: (anchorId: string) => void;
};

type PublicARViewProps = Omit<ARViewProps, 'objects' | 'faceFilters'> &
  ViewProps & {
    children?: React.ReactNode;
  };

export const ARView = forwardRef<ARViewHandle, PublicARViewProps>(
  function ARView(
    {
      children,
      onSessionStateChange,
      onSessionTypeChange,
      onARCoreError,
      onTrackingStateChange,
      onPlaneDetected,
      onPlaneUpdated,
      onAnchorCreated,
      onTap,
      onFaceDetected,
      onFaceUpdated,
      onFaceLost,
      onBlendShapesUpdate,
      style,
      sessionType = 'world',
      depthMode = 'disabled',
      lightEstimationMode = 'disabled',
      planeDetectionMode = 'none',
      focusMode = 'auto',
      shaderMode = 'camera',
      cloudAnchorMode = 'disabled',
      instantPlacementMode = 'disabled',
      paused = false,
      debugShowPlanes = false,
      debugShowPointCloud = false,
      debugShowWorldOrigin = false,
      debugShowDepthMap = false,
      debugShowFaceMesh = false,
      faceTextureURI,
      cameraFacing = 'back',
      cameraTargetFps = 'fps30',
      cameraDepthSensorUsage = 'doNotUse',
      ...rest
    },
    ref
  ) {
    const hybridRef = useRef<ARViewHybrid | null>(null);
    const objectsRef = useRef(new Map<string, ARObjectDescriptor>());
    const [objects, setObjects] = useState<ARObjectDescriptor[]>([]);
    const faceFiltersRef = useRef(new Map<string, ARFaceFilterDescriptor>());
    const [faceFilters, setFaceFilters] = useState<ARFaceFilterDescriptor[]>(
      []
    );

    const flush = useCallback(() => {
      setObjects(Array.from(objectsRef.current.values()));
    }, []);

    const flushFaceFilters = useCallback(() => {
      setFaceFilters(Array.from(faceFiltersRef.current.values()));
    }, []);

    const registerObject = useCallback(
      (desc: ARObjectDescriptor) => {
        objectsRef.current.set(desc.id, desc);
        flush();
      },
      [flush]
    );

    const updateObject = useCallback(
      (desc: ARObjectDescriptor) => {
        objectsRef.current.set(desc.id, desc);
        flush();
      },
      [flush]
    );

    const unregisterObject = useCallback(
      (id: string) => {
        objectsRef.current.delete(id);
        flush();
      },
      [flush]
    );

    const registerFilter = useCallback(
      (desc: ARFaceFilterDescriptor) => {
        faceFiltersRef.current.set(desc.id, desc);
        flushFaceFilters();
      },
      [flushFaceFilters]
    );

    const updateFilter = useCallback(
      (desc: ARFaceFilterDescriptor) => {
        faceFiltersRef.current.set(desc.id, desc);
        flushFaceFilters();
      },
      [flushFaceFilters]
    );

    const unregisterFilter = useCallback(
      (id: string) => {
        faceFiltersRef.current.delete(id);
        flushFaceFilters();
      },
      [flushFaceFilters]
    );

    useImperativeHandle(ref, () => ({
      takeSnapshot: (saveToDisk: boolean) =>
        hybridRef.current?.takeSnapshot(saveToDisk) ?? Promise.resolve(''),
      resetSession: () => hybridRef.current?.resetSession(),
      destroySession: () => hybridRef.current?.destroySession(),
      cameraPermissionGranted: () =>
        hybridRef.current?.cameraPermissionGranted(),
      hitTest: (x: number, y: number) =>
        hybridRef.current?.hitTest(x, y) ??
        Promise.resolve({ x, y, z: 0, hasHit: false }),
      createAnchor: (x: number, y: number) =>
        hybridRef.current?.createAnchor(x, y) ?? Promise.resolve(''),
      removeAnchor: (anchorId: string) =>
        hybridRef.current?.removeAnchor(anchorId),
    }));

    const hybridRefCb = useMemo(
      () =>
        callback((r: ARViewHybrid) => {
          hybridRef.current = r;
        }),
      []
    );

    const cbSessionStateChange = useMemo(
      () => callback(onSessionStateChange ?? noop),
      [onSessionStateChange]
    );
    const cbSessionTypeChange = useMemo(
      () => callback(onSessionTypeChange ?? noop),
      [onSessionTypeChange]
    );
    const cbARCoreError = useMemo(
      () => callback(onARCoreError ?? noop),
      [onARCoreError]
    );
    const cbTrackingStateChange = useMemo(
      () => callback(onTrackingStateChange ?? noop),
      [onTrackingStateChange]
    );
    const cbPlaneDetected = useMemo(
      () => callback(onPlaneDetected ?? noop),
      [onPlaneDetected]
    );
    const cbPlaneUpdated = useMemo(
      () => callback(onPlaneUpdated ?? noop),
      [onPlaneUpdated]
    );
    const cbAnchorCreated = useMemo(
      () => callback(onAnchorCreated ?? noop),
      [onAnchorCreated]
    );
    const cbTap = useMemo(() => callback(onTap ?? noop), [onTap]);
    const cbFaceDetected = useMemo(
      () => callback(onFaceDetected ?? noop),
      [onFaceDetected]
    );
    const cbFaceUpdated = useMemo(
      () => callback(onFaceUpdated ?? noop),
      [onFaceUpdated]
    );
    const cbFaceLost = useMemo(
      () => callback(onFaceLost ?? noop),
      [onFaceLost]
    );
    const cbBlendShapesUpdate = useMemo(
      () => callback(onBlendShapesUpdate ?? noop),
      [onBlendShapesUpdate]
    );

    const sceneCtx = useMemo(
      () => ({ registerObject, updateObject, unregisterObject }),
      [registerObject, updateObject, unregisterObject]
    );
    const faceSceneCtx = useMemo(
      () => ({ registerFilter, updateFilter, unregisterFilter }),
      [registerFilter, updateFilter, unregisterFilter]
    );

    return (
      <ARSceneContext.Provider value={sceneCtx}>
        <ARFaceSceneContext.Provider value={faceSceneCtx}>
          <NativeARView
            style={style}
            hybridRef={hybridRefCb}
            sessionType={sessionType}
            depthMode={depthMode}
            lightEstimationMode={lightEstimationMode}
            planeDetectionMode={planeDetectionMode}
            focusMode={focusMode}
            shaderMode={shaderMode}
            cloudAnchorMode={cloudAnchorMode}
            instantPlacementMode={instantPlacementMode}
            paused={paused}
            debugShowPlanes={debugShowPlanes}
            debugShowPointCloud={debugShowPointCloud}
            debugShowWorldOrigin={debugShowWorldOrigin}
            debugShowDepthMap={debugShowDepthMap}
            debugShowFaceMesh={debugShowFaceMesh}
            faceTextureURI={faceTextureURI}
            cameraFacing={cameraFacing}
            cameraTargetFps={cameraTargetFps}
            cameraDepthSensorUsage={cameraDepthSensorUsage}
            {...rest}
            objects={objects}
            faceFilters={faceFilters}
            onSessionStateChange={cbSessionStateChange}
            onSessionTypeChange={cbSessionTypeChange}
            onARCoreError={cbARCoreError}
            onTrackingStateChange={cbTrackingStateChange}
            onPlaneDetected={cbPlaneDetected}
            onPlaneUpdated={cbPlaneUpdated}
            onAnchorCreated={cbAnchorCreated}
            onTap={cbTap}
            onFaceDetected={cbFaceDetected}
            onFaceUpdated={cbFaceUpdated}
            onFaceLost={cbFaceLost}
            onBlendShapesUpdate={cbBlendShapesUpdate}
          />
          {children}
        </ARFaceSceneContext.Provider>
      </ARSceneContext.Provider>
    );
  }
);
