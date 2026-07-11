import React, {
  forwardRef,
  useCallback,
  useImperativeHandle,
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
      onARCoreError: true,
      onTrackingStateChange: true,
      onPlaneDetected: true,
      onPlaneUpdated: true,
      onAnchorCreated: true,
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
      onARCoreError,
      onTrackingStateChange,
      onPlaneDetected,
      onPlaneUpdated,
      onAnchorCreated,
      onFaceDetected,
      onFaceUpdated,
      onFaceLost,
      onBlendShapesUpdate,
      style,
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
        hybridRef.current!.takeSnapshot(saveToDisk),
      resetSession: () => hybridRef.current!.resetSession(),
      destroySession: () => hybridRef.current!.destroySession(),
      cameraPermissionGranted: () =>
        hybridRef.current!.cameraPermissionGranted(),
      hitTest: (x: number, y: number) => hybridRef.current!.hitTest(x, y),
      createAnchor: (x: number, y: number) =>
        hybridRef.current!.createAnchor(x, y),
      removeAnchor: (anchorId: string) =>
        hybridRef.current!.removeAnchor(anchorId),
    }));

    return (
      <ARSceneContext.Provider
        value={{ registerObject, updateObject, unregisterObject }}
      >
        <ARFaceSceneContext.Provider
          value={{ registerFilter, updateFilter, unregisterFilter }}
        >
          <NativeARView
            style={style}
            hybridRef={callback((r: ARViewHybrid) => {
              hybridRef.current = r;
            })}
            {...rest}
            objects={objects}
            faceFilters={faceFilters}
            onARCoreError={onARCoreError ? callback(onARCoreError) : undefined}
            onTrackingStateChange={
              onTrackingStateChange
                ? callback(onTrackingStateChange)
                : undefined
            }
            onPlaneDetected={
              onPlaneDetected ? callback(onPlaneDetected) : undefined
            }
            onPlaneUpdated={
              onPlaneUpdated ? callback(onPlaneUpdated) : undefined
            }
            onAnchorCreated={
              onAnchorCreated ? callback(onAnchorCreated) : undefined
            }
            onFaceDetected={
              onFaceDetected ? callback(onFaceDetected) : undefined
            }
            onFaceUpdated={onFaceUpdated ? callback(onFaceUpdated) : undefined}
            onFaceLost={onFaceLost ? callback(onFaceLost) : undefined}
            onBlendShapesUpdate={
              onBlendShapesUpdate ? callback(onBlendShapesUpdate) : undefined
            }
          />
          {children}
        </ARFaceSceneContext.Provider>
      </ARSceneContext.Provider>
    );
  }
);
