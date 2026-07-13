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
  initialize,
  isDepthModeSupported,
  type ARViewHandle,
  type ARError,
  type ARTrackingStateInfo,
  type ARPlaneInfo,
  type ARAnchorResult,
} from 'react-native-reality';

export default function App() {
  const arViewRef = useRef<ARViewHandle>(null);
  const [initialized, setInitialized] = useState(false);
  const [hasPermission, setHasPermission] = useState(false);
  const [depthSupported, setDepthSupported] = useState(true);
  const [trackingState, setTrackingState] = useState('');
  const [planeCount, setPlaneCount] = useState(0);
  const [anchors, setAnchors] = useState<ARAnchorResult[]>([]);
  const [error, setError] = useState<string | null>(null);

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

  const onARCoreError = useCallback((err: ARError) => {
    setError(`${err.code}: ${err.message}`);
  }, []);

  const onTrackingStateChange = useCallback(
    (state: ARTrackingStateInfo) => {
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

  const handleReset = useCallback(() => {
    arViewRef.current?.resetSession();
    setAnchors([]);
    setPlaneCount(0);
  }, []);

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
      <ARView
        ref={arViewRef}
        style={styles.arView}
        sessionType="world"
        planeDetectionMode="both"
        depthMode={depthSupported ? 'automatic' : 'disabled'}
        lightEstimationMode="environmentalHDR"
        focusMode="auto"
        debugShowPlanes={true}
        onARCoreError={onARCoreError}
        onTrackingStateChange={onTrackingStateChange}
        onPlaneDetected={onPlaneDetected}
        onAnchorCreated={onAnchorCreated}
      >
        {anchors.map((anchor) => (
          <ARObject
            key={anchor.anchorId}
            anchorId={anchor.anchorId}
            model="andy"
            scale={{ x: 0.1, y: 0.1, z: 0.1 }}
          />
        ))}
      </ARView>

      <View style={styles.overlay}>
        {error && <Text style={styles.errorText}>{error}</Text>}
        <Text style={styles.text}>Tracking: {trackingState}</Text>
        <Text style={styles.text}>Planes: {planeCount}</Text>
        <Text style={styles.text}>Anchors: {anchors.length}</Text>
        <Text style={styles.text}>
          Depth: {depthSupported ? 'supported' : 'not supported'}
        </Text>
        <TouchableOpacity style={styles.button} onPress={handleReset}>
          <Text style={styles.buttonText}>Reset Session</Text>
        </TouchableOpacity>
      </View>
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
  button: {
    backgroundColor: '#2196F3',
    borderRadius: 8,
    paddingVertical: 10,
    paddingHorizontal: 16,
    marginTop: 8,
    alignItems: 'center',
  },
  buttonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
});
