import UIKit
import ARKit
import RealityKit
import NitroModules

class HybridARView: HybridARViewHybridSpec {
  // MARK: - RealityKit view

  private lazy var sessionDelegate: ARSessionDelegateHandler = {
    ARSessionDelegateHandler(owner: self)
  }()

  private lazy var tapHandler: TapHandler = {
    TapHandler(owner: self)
  }()

  private lazy var arView: RealityKit.ARView = {
    let view = RealityKit.ARView(frame: .zero)
    view.session.delegate = sessionDelegate
    let tap = UITapGestureRecognizer(target: tapHandler, action: #selector(TapHandler.handleTap(_:)))
    view.addGestureRecognizer(tap)
    return view
  }()

  var view: UIView { arView }

  // MARK: - Anchor / object tracking

  fileprivate var managedAnchors: [String: AnchorEntity] = [:]
  fileprivate var trackedPlanes: Set<UUID> = []
  private var sessionRunning = false

  // MARK: - Props

  var sessionType: ARSessionType?
  var depthMode: ARDepthMode?
  var lightEstimationMode: ARLightEstimationMode?
  var focusMode: ARFocusMode?
  var shaderMode: ARShaderMode?
  var cameraFacing: ARCameraFacing?
  var cameraTargetFps: ARCameraTargetFps?
  var cameraDepthSensorUsage: ARCameraDepthSensorUsage?
  var cloudAnchorMode: ARCloudAnchorMode?
  var instantPlacementMode: ARInstantPlacementMode?

  var planeDetectionMode: ARPlaneDetectionMode? {
    didSet { if sessionRunning { runSession() } }
  }

  var paused: Bool? {
    didSet {
      if paused == true {
        arView.session.pause()
      } else if sessionRunning {
        runSession()
      }
    }
  }

  var debugShowPlanes: Bool? {
    didSet { updateDebugOptions() }
  }
  var debugShowPointCloud: Bool? {
    didSet { updateDebugOptions() }
  }
  var debugShowWorldOrigin: Bool? {
    didSet { updateDebugOptions() }
  }
  var debugShowDepthMap: Bool?

  var objectsJSON: String? {
    didSet { syncObjects() }
  }

  // MARK: - Callbacks

  var onARCoreError: ((_ error: ARError) -> Void)?
  var onTrackingStateChange: ((_ state: ARTrackingStateInfo) -> Void)?
  var onPlaneDetected: ((_ plane: ARPlaneInfo) -> Void)?
  var onPlaneUpdated: ((_ plane: ARPlaneInfo) -> Void)?
  var onAnchorCreated: ((_ anchor: ARAnchorResult) -> Void)?

  // MARK: - Session management

  func cameraPermissionGranted() throws {
    runSession()
  }

  func resetSession() throws {
    managedAnchors.values.forEach { $0.removeFromParent() }
    managedAnchors.removeAll()
    trackedPlanes.removeAll()
    runSession(options: [.removeExistingAnchors, .resetTracking])
  }

  func destroySession() throws {
    arView.session.pause()
    managedAnchors.values.forEach { $0.removeFromParent() }
    managedAnchors.removeAll()
    sessionRunning = false
  }

  // MARK: - Methods

  func hitTest(x: Double, y: Double) throws -> Promise<ARHitTestResult> {
    return Promise.resolved(withResult: performRaycast(x: x, y: y))
  }

  func createAnchor(x: Double, y: Double) throws -> Promise<String> {
    let result = performRaycast(x: x, y: y)
    guard result.hasHit else {
      return Promise.rejected(withError: RuntimeError.error(withMessage: "No surface found at tap point"))
    }
    let anchorId = UUID().uuidString
    let position = SIMD3<Float>(Float(result.x), Float(result.y), Float(result.z))
    let anchor = AnchorEntity(world: position)
    anchor.name = anchorId
    arView.scene.addAnchor(anchor)
    managedAnchors[anchorId] = anchor

    let anchorResult = ARAnchorResult(anchorId: anchorId, x: result.x, y: result.y, z: result.z)
    onAnchorCreated?(anchorResult)

    return Promise.resolved(withResult: anchorId)
  }

  func removeAnchor(anchorId: String) throws {
    guard let anchor = managedAnchors.removeValue(forKey: anchorId) else { return }
    anchor.removeFromParent()
  }

  func takeSnapshot(saveToDisk: Bool) throws -> Promise<String> {
    let promise = Promise<String>()
    arView.snapshot(saveToHDR: false) { image in
      guard let image = image else {
        promise.reject(withError: RuntimeError.error(withMessage: "Snapshot failed"))
        return
      }
      if saveToDisk {
        let path = NSTemporaryDirectory() + "ar_snapshot_\(Int(Date().timeIntervalSince1970)).png"
        if let data = image.pngData() {
          try? data.write(to: URL(fileURLWithPath: path))
          promise.resolve(withResult: path)
        } else {
          promise.reject(withError: RuntimeError.error(withMessage: "Failed to encode snapshot"))
        }
      } else {
        if let data = image.pngData() {
          promise.resolve(withResult: data.base64EncodedString())
        } else {
          promise.reject(withError: RuntimeError.error(withMessage: "Failed to encode snapshot"))
        }
      }
    }
    return promise
  }

  // MARK: - Private helpers

  private func runSession(options: ARSession.RunOptions = []) {
    let config: ARConfiguration

    if sessionType == .face {
      let faceConfig = ARFaceTrackingConfiguration()
      config = faceConfig
    } else {
      let worldConfig = ARWorldTrackingConfiguration()

      switch planeDetectionMode {
      case .horizontal:
        worldConfig.planeDetection = .horizontal
      case .vertical:
        worldConfig.planeDetection = .vertical
      case .both:
        worldConfig.planeDetection = [.horizontal, .vertical]
      case .none:
        worldConfig.planeDetection = []
      default:
        worldConfig.planeDetection = [.horizontal, .vertical]
      }

      if depthMode != nil && depthMode != .disabled {
        if ARWorldTrackingConfiguration.supportsFrameSemantics(.sceneDepth) {
          worldConfig.frameSemantics.insert(.sceneDepth)
        }
      }

      if lightEstimationMode == .environmentalhdr {
        worldConfig.environmentTexturing = .automatic
      }

      worldConfig.isLightEstimationEnabled = lightEstimationMode != .disabled

      config = worldConfig
    }

    arView.session.run(config, options: options)
    sessionRunning = true
  }

  private func updateDebugOptions() {
    var opts: RealityKit.ARView.DebugOptions = []
    if debugShowWorldOrigin == true { opts.insert(.showWorldOrigin) }
    if debugShowPointCloud == true { opts.insert(.showFeaturePoints) }
    if debugShowPlanes == true { opts.insert(.showAnchorGeometry) }
    arView.debugOptions = opts
  }

  fileprivate func performRaycast(x: Double, y: Double) -> ARHitTestResult {
    let point = CGPoint(x: x, y: y)
    let results = arView.raycast(from: point, allowing: .estimatedPlane, alignment: .any)
    guard let first = results.first else {
      return ARHitTestResult(x: 0, y: 0, z: 0, hasHit: false)
    }
    let pos = first.worldTransform.columns.3
    return ARHitTestResult(x: Double(pos.x), y: Double(pos.y), z: Double(pos.z), hasHit: true)
  }

  private static func resolveModelURL(_ model: String) -> URL? {
    var name = model
      .replacingOccurrences(of: ".obj", with: ".usdz")
    if !name.hasSuffix(".usdz") { name += ".usdz" }

    let bundleURL = Bundle(for: HybridARView.self)
      .url(forResource: "ArCoreAssets", withExtension: "bundle")
    if let bundle = bundleURL.flatMap({ Bundle(url: $0) }),
       let url = bundle.url(forResource: name, withExtension: nil) {
      return url
    }
    if let url = Bundle.main.url(forResource: name, withExtension: nil) {
      return url
    }
    let stripped = (name as NSString).deletingPathExtension
    if let url = Bundle.main.url(forResource: stripped, withExtension: "usdz") {
      return url
    }
    return nil
  }

  // MARK: - Object synchronization

  private struct ObjectDesc: Codable {
    let id: String
    let anchorId: String
    let model: String
    let texture: String?
    let scale: Scale?
    let rotation: Rotation?
    let color: String?
    let visible: Bool?

    struct Scale: Codable { let x: Float; let y: Float; let z: Float }
    struct Rotation: Codable { let x: Float; let y: Float; let z: Float }
  }

  private func syncObjects() {
    guard let json = objectsJSON, !json.isEmpty,
          let data = json.data(using: .utf8),
          let descs = try? JSONDecoder().decode([ObjectDesc].self, from: data) else {
      return
    }

    for desc in descs {
      guard let anchor = managedAnchors[desc.anchorId] else { continue }

      let existing = anchor.children.first(where: { $0.name == desc.id })
      if existing != nil { existing?.removeFromParent() }
      if desc.visible == false { continue }

      do {
        let entity: Entity
        if let url = Self.resolveModelURL(desc.model) {
          entity = try Entity.load(contentsOf: url)
        } else {
          let modelName = desc.model
            .replacingOccurrences(of: ".obj", with: "")
            .replacingOccurrences(of: ".usdz", with: "")
          entity = try Entity.load(named: modelName)
        }
        entity.name = desc.id

        if let s = desc.scale {
          entity.scale = SIMD3<Float>(s.x, s.y, s.z)
        }
        if let r = desc.rotation {
          let angles = SIMD3<Float>(r.x, r.y, r.z)
          entity.orientation = simd_quatf(angle: angles.x, axis: [1, 0, 0])
            * simd_quatf(angle: angles.y, axis: [0, 1, 0])
            * simd_quatf(angle: angles.z, axis: [0, 0, 1])
        }

        anchor.addChild(entity)
      } catch {
        onARCoreError?(ARError(code: "MODEL_LOAD_FAILED", message: error.localizedDescription))
      }
    }
  }
}

// MARK: - NSObject-based delegate & tap handler

private class TapHandler: NSObject {
  weak var owner: HybridARView?
  init(owner: HybridARView) { self.owner = owner }

  @objc func handleTap(_ gesture: UITapGestureRecognizer) {
    guard let owner = owner, let arView = gesture.view else { return }
    let location = gesture.location(in: arView)
    _ = try? owner.createAnchor(x: Double(location.x), y: Double(location.y))
  }
}

private class ARSessionDelegateHandler: NSObject, ARSessionDelegate {
  weak var owner: HybridARView?
  init(owner: HybridARView) { self.owner = owner; super.init() }

  func session(_ session: ARSession, didFailWithError error: Error) {
    owner?.onARCoreError?(ARError(code: "SESSION_ERROR", message: error.localizedDescription))
  }

  func session(_ session: ARSession, cameraDidChangeTrackingState camera: ARCamera) {
    let state: String
    var reason: String?

    switch camera.trackingState {
    case .notAvailable:
      state = "notAvailable"
    case .limited(let r):
      state = "limited"
      switch r {
      case .initializing: reason = "initializing"
      case .excessiveMotion: reason = "excessiveMotion"
      case .insufficientFeatures: reason = "insufficientFeatures"
      case .relocalizing: reason = "relocalizing"
      @unknown default: reason = "unknown"
      }
    case .normal:
      state = "tracking"
    }

    owner?.onTrackingStateChange?(ARTrackingStateInfo(state: state, reason: reason))
  }

  func session(_ session: ARSession, didAdd anchors: [ARAnchor]) {
    guard let owner = owner else { return }
    for anchor in anchors {
      guard let plane = anchor as? ARPlaneAnchor else { continue }
      if owner.trackedPlanes.contains(plane.identifier) { continue }
      owner.trackedPlanes.insert(plane.identifier)
      owner.onPlaneDetected?(planeInfoFrom(plane))
    }
  }

  func session(_ session: ARSession, didUpdate anchors: [ARAnchor]) {
    for anchor in anchors {
      guard let plane = anchor as? ARPlaneAnchor else { continue }
      owner?.onPlaneUpdated?(planeInfoFrom(plane))
    }
  }

  private func planeInfoFrom(_ plane: ARPlaneAnchor) -> ARPlaneInfo {
    let type: String
    switch plane.alignment {
    case .horizontal: type = "horizontal"
    case .vertical: type = "vertical"
    @unknown default: type = "unknown"
    }
    return ARPlaneInfo(
      id: plane.identifier.uuidString,
      type: type,
      centerX: Double(plane.center.x),
      centerY: Double(plane.center.y),
      centerZ: Double(plane.center.z),
      extentX: Double(plane.extent.x),
      extentZ: Double(plane.extent.z)
    )
  }
}
