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

  fileprivate lazy var arView: RealityKit.ARView = {
    let view = RealityKit.ARView(frame: .zero)
    view.session.delegate = sessionDelegate
    let tap = UITapGestureRecognizer(target: tapHandler, action: #selector(TapHandler.handleTap(_:)))
    view.addGestureRecognizer(tap)
    registerLifecycleObservers()
    return view
  }()

  var view: UIView { arView }

  deinit {
    lifecycleObservers.forEach { NotificationCenter.default.removeObserver($0) }
  }

  // MARK: - Anchor / object tracking

  fileprivate var managedAnchors: [String: AnchorEntity] = [:]
  fileprivate var trackedPlanes: Set<UUID> = []
  fileprivate var trackedFaces: Set<UUID> = []
  fileprivate var faceAnchorEntity: AnchorEntity?
  fileprivate var faceFilterEntities: [String: Entity] = [:]
  private var sessionRunning = false
  private var wasInterrupted = false
  private var lifecycleObservers: [NSObjectProtocol] = []

  // MARK: - Props

  var sessionType: ARSessionType? {
    didSet { if oldValue != sessionType, sessionRunning { runSession() } }
  }
  var depthMode: ARDepthMode?
  var lightEstimationMode: ARLightEstimationMode?
  var focusMode: ARFocusMode?
  var shaderMode: ARShaderMode?
  var cameraFacing: ARCameraFacing? {
    didSet { if sessionRunning { runSession() } }
  }
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
  var debugShowFaceMesh: Bool?

  var objects: [ARObjectDescriptor]? {
    didSet { syncObjects() }
  }

  var faceFilters: [ARFaceFilterDescriptor]? {
    didSet { syncFaceFilters() }
  }

  var faceTextureURI: String?

  // MARK: - Callbacks

  var onSessionStateChange: ((_ state: String) -> Void)?
  var onSessionTypeChange: ((_ type: String) -> Void)?
  var onARCoreError: ((_ error: ARError) -> Void)?
  var onTrackingStateChange: ((_ state: ARTrackingStateInfo) -> Void)?
  var onPlaneDetected: ((_ plane: ARPlaneInfo) -> Void)?
  var onPlaneUpdated: ((_ plane: ARPlaneInfo) -> Void)?
  var onAnchorCreated: ((_ anchor: ARAnchorResult) -> Void)?
  var onTap: ((_ result: ARTapResult) -> Void)?
  var onFaceDetected: ((_ face: ARFaceInfo) -> Void)?
  var onFaceUpdated: ((_ face: ARFaceInfo) -> Void)?
  var onFaceLost: ((_ faceId: String) -> Void)?
  var onBlendShapesUpdate: ((_ shapes: ARBlendShapes) -> Void)?

  // MARK: - Session management

  func cameraPermissionGranted() throws {
    runSession()
  }

  func resetSession() throws {
    managedAnchors.values.forEach { $0.removeFromParent() }
    managedAnchors.removeAll()
    trackedPlanes.removeAll()
    trackedFaces.removeAll()
    faceAnchorEntity?.removeFromParent()
    faceAnchorEntity = nil
    faceFilterEntities.removeAll()
    runSession(options: [.removeExistingAnchors, .resetTracking])
  }

  func destroySession() throws {
    onSessionStateChange?("destroying")
    arView.session.pause()
    managedAnchors.values.forEach { $0.removeFromParent() }
    managedAnchors.removeAll()
    trackedPlanes.removeAll()
    trackedFaces.removeAll()
    faceAnchorEntity?.removeFromParent()
    faceAnchorEntity = nil
    faceFilterEntities.removeAll()
    sessionRunning = false
    onSessionStateChange?("destroyed")
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
    return Promise.resolved(withResult: makeAnchor(at: result))
  }

  /// Creates a world anchor at a raycast hit, emits `onAnchorCreated`, returns its id.
  private func makeAnchor(at result: ARHitTestResult) -> String {
    let anchorId = UUID().uuidString
    let position = SIMD3<Float>(Float(result.x), Float(result.y), Float(result.z))
    let anchor = AnchorEntity(world: position)
    anchor.name = anchorId
    arView.scene.addAnchor(anchor)
    managedAnchors[anchorId] = anchor
    onAnchorCreated?(ARAnchorResult(anchorId: anchorId, x: result.x, y: result.y, z: result.z))
    return anchorId
  }

  /// Observes app foreground transitions. ARKit's `sessionInterruptionEnded` is not
  /// reliably delivered when returning from the background, so we also resume on
  /// `didBecomeActive` — whichever fires first wins (guarded by `wasInterrupted`).
  private func registerLifecycleObservers() {
    let observer = NotificationCenter.default.addObserver(
      forName: UIApplication.didBecomeActiveNotification,
      object: nil,
      queue: .main
    ) { [weak self] _ in
      self?.resumeFromInterruption()
    }
    lifecycleObservers.append(observer)
  }

  /// Marks the session interrupted (app backgrounded / camera taken over) and
  /// notifies JS. Called from `sessionWasInterrupted`.
  fileprivate func markInterrupted() {
    wasInterrupted = true
    onSessionStateChange?("paused")
  }

  /// Resumes after an interruption ends. RealityKit's ARView auto-resumes the
  /// session when the app returns to the foreground, so we must NOT call
  /// `session.run()` here — doing so during the foreground transition makes ARKit
  /// fire `sessionWasInterrupted` again and the state bounces back to "paused". We
  /// only notify JS that the session is live. Idempotent via `wasInterrupted` so the
  /// delegate callback and the foreground notification don't double-fire.
  fileprivate func resumeFromInterruption() {
    guard wasInterrupted else { return }
    wasInterrupted = false
    onSessionStateChange?("ready")
  }

  /// Handles a screen tap: always emits `onTap` (so JS sees every tap, even misses);
  /// additionally creates an anchor + emits `onAnchorCreated` on a surface hit.
  /// Hit-testing is world-only, so the AR view ignores taps in face sessions
  /// (overlay buttons are separate views and keep receiving touches).
  fileprivate func handleScreenTap(x: Double, y: Double) {
    if sessionType == .face { return }
    let result = performRaycast(x: x, y: y)
    var anchorId: String? = nil
    if result.hasHit {
      anchorId = makeAnchor(at: result)
    }
    onTap?(ARTapResult(x: x, y: y, hasHit: result.hasHit, anchorId: anchorId))
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
    if sessionRunning {
      onSessionStateChange?("destroying")
      arView.session.pause()
    }
    onSessionStateChange?("initializing")

    let config: ARConfiguration
    let usedFaceConfig: Bool

    let useFrontCamera = sessionType == .face || cameraFacing == .front
    if useFrontCamera && ARFaceTrackingConfiguration.isSupported {
      let faceConfig = ARFaceTrackingConfiguration()
      faceConfig.isLightEstimationEnabled = true
      if #available(iOS 16.0, *) {
        faceConfig.maximumNumberOfTrackedFaces = 1
      }
      config = faceConfig
      usedFaceConfig = true
    } else {
      let worldConfig = ARWorldTrackingConfiguration()

      if let mode = planeDetectionMode {
        switch mode {
        case .horizontal:
          worldConfig.planeDetection = .horizontal
        case .vertical:
          worldConfig.planeDetection = .vertical
        case .both:
          worldConfig.planeDetection = [.horizontal, .vertical]
        case .none:
          worldConfig.planeDetection = []
        @unknown default:
          worldConfig.planeDetection = [.horizontal, .vertical]
        }
      } else {
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
      usedFaceConfig = false
    }

    arView.session.run(config, options: options)
    sessionRunning = true
    onSessionStateChange?("ready")
    // Report the type the session actually came up with (captures a face->world
    // fallback on devices without face tracking).
    onSessionTypeChange?(usedFaceConfig ? "face" : "world")
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

  static func resolveModelURL(_ model: String) -> URL? {
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

  private func syncObjects() {
    guard let descs = objects, !descs.isEmpty else { return }

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
          entity.scale = SIMD3<Float>(Float(s.x), Float(s.y), Float(s.z))
        }
        if let r = desc.rotation {
          entity.orientation = simd_quatf(ix: Float(r.x), iy: Float(r.y), iz: Float(r.z), r: Float(r.w))
        }

        anchor.addChild(entity)
      } catch {
        onARCoreError?(ARError(code: "MODEL_LOAD_FAILED", message: error.localizedDescription))
      }
    }
  }

  // MARK: - Face filter synchronization

  fileprivate func syncFaceFilters() {
    guard let descs = faceFilters, !descs.isEmpty else {
      for (_, entity) in faceFilterEntities { entity.removeFromParent() }
      faceFilterEntities.removeAll()
      return
    }

    guard let faceAnchor = faceAnchorEntity else { return }

    let currentIds = Set(descs.map { $0.id })
    for (id, entity) in faceFilterEntities where !currentIds.contains(id) {
      entity.removeFromParent()
      faceFilterEntities.removeValue(forKey: id)
    }

    for desc in descs {
      if let existing = faceFilterEntities[desc.id] {
        existing.removeFromParent()
        faceFilterEntities.removeValue(forKey: desc.id)
      }
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
          entity.scale = SIMD3<Float>(Float(s.x), Float(s.y), Float(s.z))
        }

        var position = SIMD3<Float>(0, 0, 0)
        if let o = desc.offset {
          position = SIMD3<Float>(Float(o.x), Float(o.y), Float(o.z))
        }
        position = position + Self.offsetForAttachmentPoint(desc.attachmentPoint)
        entity.position = position

        if let r = desc.rotation {
          let angles = SIMD3<Float>(Float(r.x), Float(r.y), Float(r.z))
          entity.orientation = simd_quatf(angle: angles.x, axis: [1, 0, 0])
            * simd_quatf(angle: angles.y, axis: [0, 1, 0])
            * simd_quatf(angle: angles.z, axis: [0, 0, 1])
        }

        faceAnchor.addChild(entity)
        faceFilterEntities[desc.id] = entity
      } catch {
        onARCoreError?(ARError(code: "FACE_MODEL_LOAD_FAILED", message: error.localizedDescription))
      }
    }
  }

  private static func offsetForAttachmentPoint(_ point: String) -> SIMD3<Float> {
    switch point {
    case "forehead": return SIMD3<Float>(0, 0.08, 0.02)
    case "foreheadLeft": return SIMD3<Float>(-0.05, 0.06, 0.02)
    case "foreheadRight": return SIMD3<Float>(0.05, 0.06, 0.02)
    case "noseTip": return SIMD3<Float>(0, -0.02, 0.06)
    case "noseBridge": return SIMD3<Float>(0, 0.02, 0.05)
    case "leftEye": return SIMD3<Float>(-0.03, 0.02, 0.04)
    case "rightEye": return SIMD3<Float>(0.03, 0.02, 0.04)
    case "leftEar": return SIMD3<Float>(-0.08, 0.01, -0.01)
    case "rightEar": return SIMD3<Float>(0.08, 0.01, -0.01)
    case "chin": return SIMD3<Float>(0, -0.07, 0.03)
    case "mouthCenter": return SIMD3<Float>(0, -0.04, 0.04)
    default: return SIMD3<Float>(0, 0, 0)
    }
  }

  fileprivate func updateFaceFilterTransforms(_ faceAnchor: ARFaceAnchor) {
    guard let anchorEntity = faceAnchorEntity else { return }
    anchorEntity.transform = Transform(matrix: faceAnchor.transform)
  }
}

// MARK: - NSObject-based delegate & tap handler

private class TapHandler: NSObject {
  weak var owner: HybridARView?
  init(owner: HybridARView) { self.owner = owner }

  @objc func handleTap(_ gesture: UITapGestureRecognizer) {
    guard let owner = owner, let arView = gesture.view else { return }
    let location = gesture.location(in: arView)
    owner.handleScreenTap(x: Double(location.x), y: Double(location.y))
  }
}

private class ARSessionDelegateHandler: NSObject, ARSessionDelegate {
  weak var owner: HybridARView?
  init(owner: HybridARView) { self.owner = owner; super.init() }

  func session(_ session: ARSession, didFailWithError error: Error) {
    owner?.onARCoreError?(ARError(code: "SESSION_ERROR", message: error.localizedDescription))
    owner?.onSessionStateChange?("failed")
  }

  func sessionWasInterrupted(_ session: ARSession) {
    owner?.markInterrupted()
  }

  func sessionInterruptionEnded(_ session: ARSession) {
    // ARKit can't reliably relocalize world tracking after an interruption, so
    // reset tracking and drop stale anchors — matching Android's resume behavior.
    // (didBecomeActive is the backup trigger when this isn't delivered.)
    owner?.resumeFromInterruption()
  }

  func session(_ session: ARSession, cameraDidChangeTrackingState camera: ARCamera) {
    let state: String
    var reason: String?

    switch camera.trackingState {
    case .notAvailable:
      state = "unavailable"
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
      if let plane = anchor as? ARPlaneAnchor {
        if owner.trackedPlanes.contains(plane.identifier) { continue }
        owner.trackedPlanes.insert(plane.identifier)
        owner.onPlaneDetected?(planeInfoFrom(plane))
      } else if let face = anchor as? ARFaceAnchor {
        if owner.trackedFaces.contains(face.identifier) { continue }
        owner.trackedFaces.insert(face.identifier)

        let faceAnchorEntity = AnchorEntity()
        faceAnchorEntity.transform = Transform(matrix: face.transform)
        owner.arView.scene.addAnchor(faceAnchorEntity)
        owner.faceAnchorEntity = faceAnchorEntity

        owner.onFaceDetected?(faceInfoFrom(face))
        emitBlendShapes(face, owner: owner)

        owner.syncFaceFilters()
      }
    }
  }

  func session(_ session: ARSession, didUpdate anchors: [ARAnchor]) {
    guard let owner = owner else { return }
    for anchor in anchors {
      if let plane = anchor as? ARPlaneAnchor {
        owner.onPlaneUpdated?(planeInfoFrom(plane))
      } else if let face = anchor as? ARFaceAnchor {
        owner.updateFaceFilterTransforms(face)
        owner.onFaceUpdated?(faceInfoFrom(face))
        emitBlendShapes(face, owner: owner)
      }
    }
  }

  func session(_ session: ARSession, didRemove anchors: [ARAnchor]) {
    guard let owner = owner else { return }
    for anchor in anchors {
      if let plane = anchor as? ARPlaneAnchor {
        owner.trackedPlanes.remove(plane.identifier)
      } else if let face = anchor as? ARFaceAnchor {
        owner.trackedFaces.remove(face.identifier)
        owner.faceAnchorEntity?.removeFromParent()
        owner.faceAnchorEntity = nil
        owner.faceFilterEntities.removeAll()
        owner.onFaceLost?(face.identifier.uuidString)
      }
    }
  }

  private func faceInfoFrom(_ face: ARFaceAnchor) -> ARFaceInfo {
    let t = face.transform
    let transform: [Double] = [
      Double(t.columns.0.x), Double(t.columns.0.y), Double(t.columns.0.z), Double(t.columns.0.w),
      Double(t.columns.1.x), Double(t.columns.1.y), Double(t.columns.1.z), Double(t.columns.1.w),
      Double(t.columns.2.x), Double(t.columns.2.y), Double(t.columns.2.z), Double(t.columns.2.w),
      Double(t.columns.3.x), Double(t.columns.3.y), Double(t.columns.3.z), Double(t.columns.3.w),
    ]
    return ARFaceInfo(faceId: face.identifier.uuidString, transform: transform)
  }

  private func emitBlendShapes(_ face: ARFaceAnchor, owner: HybridARView) {
    guard owner.onBlendShapesUpdate != nil else { return }
    var keys: [String] = []
    var values: [Double] = []
    keys.reserveCapacity(face.blendShapes.count)
    values.reserveCapacity(face.blendShapes.count)
    for (key, value) in face.blendShapes {
      keys.append(key.rawValue)
      values.append(value.doubleValue)
    }
    owner.onBlendShapesUpdate?(ARBlendShapes(faceId: face.identifier.uuidString, shapeKeys: keys, shapeValues: values))
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
