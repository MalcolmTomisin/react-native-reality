import { useCallback, useEffect, useRef, useState } from 'react';
import {
  StyleSheet,
  Text,
  View,
  TouchableOpacity,
  PermissionsAndroid,
  Platform,
} from 'react-native';
import {
  ARView,
  ARObject,
  ARFaceFilter,
  initialize,
  isDepthModeSupported,
  type ARViewHandle,
  type ARError,
  type ARTrackingStateInfo,
  type ARPlaneInfo,
  type ARAnchorResult,
  type ARTapResult,
  type ARFaceInfo,
  type ARBlendShapes,
} from 'react-native-reality';

type SessionMode = 'world' | 'face';

export default function App() {
  const arViewRef = useRef<ARViewHandle>(null);
  const [initialized, setInitialized] = useState(false);
  const [hasPermission, setHasPermission] = useState(false);
  const [depthSupported, setDepthSupported] = useState(true);
  const [mode, setMode] = useState<SessionMode>('world');
  const [trackingState, setTrackingState] = useState('');
  const [planeCount, setPlaneCount] = useState(0);
  const [anchors, setAnchors] = useState<ARAnchorResult[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [tapCount, setTapCount] = useState(0);
  const [lastTapResult, setLastTapResult] = useState('');
  const [faceCount, setFaceCount] = useState(0);
  const [faceStatus, setFaceStatus] = useState('');
  const [blendShapeInfo, setBlendShapeInfo] = useState('');
  const [sessionState, setSessionState] = useState('');
  const [activeType, setActiveType] = useState('');

  useEffect(() => {
    initialize().then(setInitialized);
  }, []);

  useEffect(() => {
    if (Platform.OS === 'android') {
      PermissionsAndroid.request(PermissionsAndroid.PERMISSIONS.CAMERA).then(
        (status) => {
          if (status === PermissionsAndroid.RESULTS.GRANTED) {
            setHasPermission(true);
            arViewRef.current?.cameraPermissionGranted();
          }
        }
      );
    } else {
      setHasPermission(true);
      arViewRef.current?.cameraPermissionGranted();
    }
  }, []);

  const onSessionStateChange = useCallback((state: string) => {
    console.log('Session state changed to', state);
    setSessionState(state);
  }, []);

  const onSessionTypeChange = useCallback((type: string) => {
    console.log('Session type changed to', type);
    setActiveType(type);
  }, []);

  const onARCoreError = useCallback((err: ARError) => {
    console.error('ARCore error:', err.code, err.message);
    setError(`${err.code}: ${err.message}`);
  }, []);

  const onTrackingStateChange = useCallback(
    (state: ARTrackingStateInfo) => {
      console.log('Tracking state changed to', state.state, state.reason);
      setTrackingState(state.state);
      if (state.state === 'tracking' && !depthSupported) {
        setDepthSupported(isDepthModeSupported());
      }
    },
    [depthSupported]
  );

  const onPlaneDetected = useCallback((_plane: ARPlaneInfo) => {
    setPlaneCount((c) => c + 1);
  }, []);

  const onAnchorCreated = useCallback((anchor: ARAnchorResult) => {
    setAnchors((prev) => [...prev, anchor]);
  }, []);

  const onTap = useCallback((result: ARTapResult) => {
    setTapCount((c) => c + 1);
    setLastTapResult(
      result.hasHit
        ? `Hit at screen (${result.x.toFixed(0)}, ${result.y.toFixed(0)})`
        : `Miss at screen (${result.x.toFixed(0)}, ${result.y.toFixed(
            0
          )}) — no plane`
    );
  }, []);

  const onFaceDetected = useCallback((face: ARFaceInfo) => {
    setFaceCount((c) => c + 1);
    setFaceStatus(`Detected face ${face.faceId}`);
  }, []);

  const onFaceUpdated = useCallback((face: ARFaceInfo) => {
    setFaceStatus(`Tracking face ${face.faceId}`);
  }, []);

  const onFaceLost = useCallback((faceId: string) => {
    setFaceCount((c) => Math.max(0, c - 1));
    setFaceStatus(`Lost face ${faceId}`);
  }, []);

  const onBlendShapesUpdate = useCallback((shapes: ARBlendShapes) => {
    if (shapes.shapeKeys.length > 0) {
      const top = shapes.shapeKeys
        .map((key, i) => ({ key, value: shapes.shapeValues[i]! }))
        .sort((a, b) => b.value - a.value)
        .slice(0, 3)
        .map((s) => `${s.key}: ${s.value.toFixed(2)}`)
        .join(', ');
      setBlendShapeInfo(top);
    }
  }, []);

  const resetUiState = useCallback(() => {
    setAnchors([]);
    setPlaneCount(0);
    setTapCount(0);
    setLastTapResult('');
    setFaceCount(0);
    setFaceStatus('');
    setBlendShapeInfo('');
    setTrackingState('');
    setError(null);
  }, []);

  const handleReset = useCallback(() => {
    arViewRef.current?.resetSession();
    resetUiState();
  }, [resetUiState]);

  // Toggling only changes the sessionType prop; the native layer reconfigures the
  // session (and camera) in place. No session destroy here — that would leave
  // Android with no session to recreate.
  const handleToggleMode = useCallback(() => {
    resetUiState();
    setMode((m) => (m === 'world' ? 'face' : 'world'));
  }, [resetUiState]);

  if (!initialized) {
    return (
      <View style={styles.centered}>
        <Text style={styles.text}>Initializing ARCore...</Text>
      </View>
    );
  }

  if (!hasPermission) {
    return (
      <View style={styles.centered}>
        <Text style={styles.text}>Waiting for camera permission...</Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      {mode === 'world' ? (
        <ARView
          ref={arViewRef}
          style={styles.arView}
          sessionType="world"
          planeDetectionMode="both"
          depthMode={depthSupported ? 'automatic' : 'disabled'}
          lightEstimationMode="environmentalHDR"
          focusMode="auto"
          debugShowPlanes={true}
          onSessionStateChange={onSessionStateChange}
          onSessionTypeChange={onSessionTypeChange}
          onARCoreError={onARCoreError}
          onTrackingStateChange={onTrackingStateChange}
          onPlaneDetected={onPlaneDetected}
          onAnchorCreated={onAnchorCreated}
          onTap={onTap}
        >
          {anchors.map((anchor) => (
            <ARObject
              key={anchor.anchorId}
              anchorId={anchor.anchorId}
              model="andy"
              scale={{ x: 0.1, y: 0.1, z: 0.1 }}
              // texture accepts idiomatic RN sources:
              //   require('./tex.png') | { uri: 'file://…' | 'data:…' } | 'bundledName'
              // Here: a texture fetched over the network.
              texture={{
                uri: 'https://threejs.org/examples/textures/uv_grid_opengl.jpg',
              }}
            />
          ))}
        </ARView>
      ) : (
        <ARView
          ref={arViewRef}
          style={styles.arView}
          sessionType="face"
          cameraFacing="front"
          lightEstimationMode="environmentalHDR"
          onSessionStateChange={onSessionStateChange}
          onSessionTypeChange={onSessionTypeChange}
          onARCoreError={onARCoreError}
          onTrackingStateChange={onTrackingStateChange}
          onFaceDetected={onFaceDetected}
          onFaceUpdated={onFaceUpdated}
          onFaceLost={onFaceLost}
          onBlendShapesUpdate={onBlendShapesUpdate}
        >
          <ARFaceFilter
            attachmentPoint="noseTip"
            model="nose"
            scale={{ x: 1, y: 1, z: 1 }}
          />
          <ARFaceFilter
            attachmentPoint="foreheadLeft"
            model="forehead_left"
            scale={{ x: 1, y: 1, z: 1 }}
          />
          <ARFaceFilter
            attachmentPoint="foreheadRight"
            model="forehead_right"
            scale={{ x: 1, y: 1, z: 1 }}
          />
        </ARView>
      )}

      <View style={styles.overlay}>
        <Text style={styles.modeText}>
          Mode: {mode === 'world' ? 'World Tracking' : 'Face Tracking'}
        </Text>
        {error && <Text style={styles.errorText}>{error}</Text>}
        <Text style={styles.text}>Session: {sessionState || 'none'}</Text>
        <Text style={styles.text}>Active: {activeType || 'none'}</Text>
        <Text style={styles.text}>Tracking: {trackingState || 'none'}</Text>

        {mode === 'world' && (
          <>
            <Text style={styles.text}>Planes: {planeCount}</Text>
            <Text style={styles.text}>Anchors: {anchors.length}</Text>
            <Text style={styles.text}>
              Depth: {depthSupported ? 'supported' : 'not supported'}
            </Text>
            <Text style={styles.text}>Taps: {tapCount}</Text>
            {lastTapResult !== '' && (
              <Text style={styles.text}>{lastTapResult}</Text>
            )}
          </>
        )}

        {mode === 'face' && (
          <>
            <Text style={styles.text}>Faces: {faceCount}</Text>
            {faceStatus !== '' && <Text style={styles.text}>{faceStatus}</Text>}
            {blendShapeInfo !== '' && (
              <Text style={styles.text}>Blend: {blendShapeInfo}</Text>
            )}
          </>
        )}

        <View style={styles.buttonRow}>
          <TouchableOpacity style={styles.button} onPress={handleToggleMode}>
            <Text style={styles.buttonText}>
              {mode === 'world' ? 'Face Mode' : 'World Mode'}
            </Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.button} onPress={handleReset}>
            <Text style={styles.buttonText}>Reset</Text>
          </TouchableOpacity>
        </View>
      </View>

      {(sessionState === 'initializing' ||
        sessionState === 'destroying' ||
        sessionState === 'paused') && (
        <View style={styles.loadingOverlay}>
          <Text style={styles.loadingText}>
            {sessionState === 'initializing'
              ? 'Starting AR session...'
              : sessionState === 'paused'
              ? 'Session paused'
              : 'Stopping AR session...'}
          </Text>
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  centered: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: '#000',
  },
  arView: {
    flex: 1,
  },
  overlay: {
    position: 'absolute',
    top: 50,
    left: 16,
    right: 16,
    backgroundColor: 'rgba(0,0,0,0.6)',
    borderRadius: 12,
    padding: 12,
  },
  modeText: {
    color: '#4FC3F7',
    fontSize: 16,
    fontWeight: '700',
    marginBottom: 6,
  },
  text: {
    color: '#fff',
    fontSize: 14,
    marginBottom: 4,
  },
  errorText: {
    color: '#ff4444',
    fontSize: 14,
    marginBottom: 4,
  },
  buttonRow: {
    flexDirection: 'row',
    gap: 8,
    marginTop: 8,
  },
  button: {
    flex: 1,
    backgroundColor: '#2196F3',
    borderRadius: 8,
    paddingVertical: 10,
    paddingHorizontal: 16,
    alignItems: 'center',
  },
  buttonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
  loadingOverlay: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: 'rgba(0,0,0,0.7)',
    alignItems: 'center',
    justifyContent: 'center',
  },
  loadingText: {
    color: '#fff',
    fontSize: 18,
    fontWeight: '600',
  },
});
