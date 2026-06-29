package com.margelo.nitro.arcore

import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.os.Handler
import android.os.Looper
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip
import com.margelo.nitro.core.Promise

@Keep
@DoNotStrip
class HybridARView(private val reactContext: com.facebook.react.uimanager.ThemedReactContext) : HybridARViewHybridSpec(), GLSurfaceView.Renderer {

    private val mainHandler = Handler(Looper.getMainLooper())
    private val app = ARAppSystem.instance

    init {
        CurrentActivityTracker.register(reactContext.applicationContext as android.app.Application)
        app.ensureInitialized(reactContext.applicationContext)
    }

    private val container: FrameLayout by lazy {
        FrameLayout(reactContext).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            )
        }
    }

    private val glSurfaceView: GLSurfaceView by lazy {
        GLSurfaceView(container.context).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            )
            setEGLContextClientVersion(3)
            setEGLConfigChooser(8, 8, 8, 8, 16, 0)
            preserveEGLContextOnPause = true
            setRenderer(this@HybridARView)
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
            setWillNotDraw(false)
        }
    }

    private val gestureDetector: GestureDetector by lazy {
        GestureDetector(
            container.context,
            object : GestureDetector.SimpleOnGestureListener() {
                override fun onDown(e: MotionEvent): Boolean = true

                override fun onSingleTapUp(e: MotionEvent): Boolean {
                    val x = e.x
                    val y = e.y
                    glSurfaceView.queueEvent {
                        val anchorId = app.createAnchor(x, y)
                        if (anchorId.isNotEmpty()) {
                            mainHandler.post {
                                onAnchorCreated?.invoke(
                                    ARAnchorResult(anchorId, x.toDouble(), y.toDouble(), 0.0)
                                )
                            }
                        }
                    }
                    return true
                }
            },
        )
    }

    override val view: View
        get() {
            if (glSurfaceView.parent == null) {
                container.addView(glSurfaceView)
                glSurfaceView.setOnTouchListener { _, event ->
                    gestureDetector.onTouchEvent(event)
                    true
                }
                container.addOnAttachStateChangeListener(object : View.OnAttachStateChangeListener {
                    override fun onViewAttachedToWindow(v: View) {
                        ARViewRegistry.register(this@HybridARView)
                        app.onARViewMounted()

                        val activity = reactContext.currentActivity
                            ?: CurrentActivityTracker.getCurrentActivity()
                        if (activity != null) {
                            app.onResume(v.context.applicationContext, activity)
                        }
                    }

                    override fun onViewDetachedFromWindow(v: View) {
                        app.onARViewUnmounted()
                        ARViewRegistry.unregister(this@HybridARView)
                    }
                })
            }
            return container
        }

    // --- Props ---

    override var sessionType: ARSessionType? = null
        set(value) {
            field = value
            value?.let { app.setSessionType(it.name) }
        }

    override var depthMode: ARDepthMode? = null
        set(value) {
            field = value
            value?.let { app.setDepthMode(it.name) }
        }

    override var lightEstimationMode: ARLightEstimationMode? = null
        set(value) {
            field = value
            value?.let { app.setLightEstimationMode(it.name) }
        }

    override var planeDetectionMode: ARPlaneDetectionMode? = null
        set(value) {
            field = value
            value?.let { app.setPlaneDetectionMode(it.name) }
        }

    override var focusMode: ARFocusMode? = null
        set(value) {
            field = value
            value?.let { app.setFocusMode(it.name) }
        }

    override var shaderMode: ARShaderMode? = null
        set(value) {
            field = value
            value?.let { app.setShaderMode(it.name) }
        }

    override var cameraFacing: ARCameraFacing? = null
        set(value) { field = value }

    override var cameraTargetFps: ARCameraTargetFps? = null
        set(value) { field = value }

    override var cameraDepthSensorUsage: ARCameraDepthSensorUsage? = null
        set(value) { field = value }

    override var cloudAnchorMode: ARCloudAnchorMode? = null
        set(value) {
            field = value
            value?.let { app.setCloudAnchorMode(it.name) }
        }

    override var instantPlacementMode: ARInstantPlacementMode? = null
        set(value) {
            field = value
            value?.let { app.setInstantPlacementMode(it.name) }
        }

    override var paused: Boolean? = null
        set(value) {
            field = value
            value?.let { app.setPaused(it) }
        }

    override var debugShowPlanes: Boolean? = null
        set(value) {
            field = value
            value?.let { app.setDebugShowPlanes(it) }
        }

    override var debugShowPointCloud: Boolean? = null
        set(value) {
            field = value
            value?.let { app.setDebugShowPointCloud(it) }
        }

    override var debugShowWorldOrigin: Boolean? = null
        set(value) {
            field = value
            value?.let { app.setDebugShowWorldOrigin(it) }
        }

    override var debugShowDepthMap: Boolean? = null
        set(value) {
            field = value
            value?.let { app.setDebugShowDepthMap(it) }
        }

    override var objectsJSON: String? = null
        set(value) {
            field = value
            value?.let { json ->
                glSurfaceView.queueEvent {
                    app.setARObjects(json)
                }
            }
        }

    override var onARCoreError: ((error: ARError) -> Unit)? = null
    override var onTrackingStateChange: ((state: ARTrackingStateInfo) -> Unit)? = null
    override var onPlaneDetected: ((plane: ARPlaneInfo) -> Unit)? = null
    override var onPlaneUpdated: ((plane: ARPlaneInfo) -> Unit)? = null
    override var onAnchorCreated: ((anchor: ARAnchorResult) -> Unit)? = null

    // --- Methods ---

    override fun resetSession() {
        app.destroySession()
    }

    override fun destroySession() {
        app.destroySession()
    }

    override fun cameraPermissionGranted() {
        val activity = reactContext.currentActivity
            ?: CurrentActivityTracker.getCurrentActivity() ?: return
        app.onResume(activity.applicationContext, activity)
    }

    override fun hitTest(x: Double, y: Double): Promise<ARHitTestResult> {
        return Promise.async {
            val anchorId = app.createAnchor(x.toFloat(), y.toFloat())
            if (anchorId.isNotEmpty()) {
                app.removeAnchor(anchorId)
                ARHitTestResult(x, y, 0.0, true)
            } else {
                ARHitTestResult(x, y, 0.0, false)
            }
        }
    }

    override fun createAnchor(x: Double, y: Double): Promise<String> {
        return Promise.async {
            app.createAnchor(x.toFloat(), y.toFloat())
        }
    }

    override fun removeAnchor(anchorId: String) {
        glSurfaceView.queueEvent {
            app.removeAnchor(anchorId)
        }
    }

    override fun takeSnapshot(saveToDisk: Boolean): Promise<String> {
        return Promise.async {
            ""
        }
    }

    // --- GLSurfaceView.Renderer ---

    override fun onSurfaceCreated(
        gl: javax.microedition.khronos.opengles.GL10?,
        config: javax.microedition.khronos.egl.EGLConfig?,
    ) {
        GLES20.glClearColor(0.1f, 0.1f, 0.1f, 1.0f)
        app.onSurfaceCreated()
        app.onGlSurfaceCreated()
    }

    override fun onSurfaceChanged(
        gl: javax.microedition.khronos.opengles.GL10?,
        width: Int,
        height: Int,
    ) {
        val activity = CurrentActivityTracker.getCurrentActivity()
        @Suppress("DEPRECATION")
        val rotation = activity?.windowManager?.defaultDisplay?.rotation ?: 0
        app.onSurfaceChanged(width, height)
        app.onGlSurfaceChanged(width, height)
        app.setDisplayRotation(rotation)
    }

    override fun onDrawFrame(gl: javax.microedition.khronos.opengles.GL10?) {
        app.onGlDrawFrame()
    }
}
