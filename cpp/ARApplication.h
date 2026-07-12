#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <set>
#include <mutex>
#include <cstring>
#include <unordered_map>
#include "ARSessionManager.h"
#include "AndroidPlatformServices.h"
#include <android/asset_manager.h>
#include "utils.h"
#include "arcore_c_api.h"

#include "BackgroundRenderer.h"
#include "glm.h"
#include "PlaneRenderer.h"
#include "Texture.h"
#include "ObjRenderer.h"

namespace arcore
{
    // Unified native->JS event. One tagged-union carries every per-frame
    // trackable event (tracking state, planes, taps, anchors, faces) through a
    // single mutex-guarded queue. Adding a new event type = one Category value +
    // one producer + one dispatch arm in Kotlin's processEvents().
    struct AREvent
    {
        enum Category {
            TRACKING_STATE,
            PLANE_DETECTED,
            PLANE_UPDATED,
            PLANE_REMOVED,
            TAP,
            ANCHOR_CREATED,
            FACE_DETECTED,
            FACE_UPDATED,
            FACE_LOST,
        };
        Category category = TRACKING_STATE;
        std::string idA;            // faceId / planeId / anchorId
        std::string strB;           // tracking state or failure reason / plane type
        double transform[16] = {};  // face transform (identity otherwise)
        double scalars[8] = {};     // plane: cx,cy,cz,ex,ez | tap: x,y,hasHit | anchor: x,y,z
    };

    class ARApplication
    {
    public:
        struct ARObjectDesc
        {
            std::string id;
            std::string anchorId;
            std::string model;
            float scale[3] = {1.f, 1.f, 1.f};
            float color[4] = {1.f, 1.f, 1.f, 1.f};
            bool visible = true;
        };

        struct FaceFilterDesc
        {
            std::string id;
            std::string attachmentPoint;
            std::string model;
            float scale[3] = {1.f, 1.f, 1.f};
            float offset[3] = {0.f, 0.f, 0.f};
            float rotation[3] = {0.f, 0.f, 0.f};
            bool visible = true;
        };

        ARApplication(std::unique_ptr<AndroidPlatformServices> platformServices,
                      AAssetManager *assetManager)
            : platformServices_(std::move(platformServices)), asset_manager_(assetManager) {}
        ~ARApplication()
        {
            if (ar_frame_ != nullptr)
            {
                ArFrame_destroy(ar_frame_);
                ar_frame_ = nullptr;
            }
            ArSessionManager::Instance().Destroy();
        }

        std::vector<AREvent> DrainEvents()
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            std::vector<AREvent> result;
            result.swap(pending_events_);
            return result;
        }

        void PushEvent(AREvent e)
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            pending_events_.push_back(std::move(e));
        }

        void OnPause()
        {
            activity_resumed_ = false;
            ArSessionManager::Instance().Pause();
        }
        void OnResume(JNIEnv *env, jobject context, jobject activity, jboolean hasCameraPermission)
        {
            activity_resumed_ = true;

            if (active_view_count_ > 0 && !hasCameraPermission)
            {
                LOGI("AR Session not started: missing camera permission.");
                return;
            }
            if (!ArSessionManager::Instance().IsInitialized() && active_view_count_ > 0)
            {
                auto canProceed = platformServices_->checkARCoreInstallation(env, context, activity);
                if (!canProceed)
                {
                    return;
                }
                LOGI("Creating AR Session");
                ArSessionManager::Instance().Create(env, context);
                if (ar_frame_ == nullptr)
                {
                    ArFrame_create(ArSessionManager::Instance().Get(), &ar_frame_);
                }
                ArConfig *ar_config = nullptr;
                ArConfig_create(ArSessionManager::Instance().Get(), &ar_config);

                // Select camera facing direction
                {
                    ArCameraConfigFacingDirection facing =
                        (session_type_ == "face" || camera_facing_ == "front")
                            ? AR_CAMERA_CONFIG_FACING_DIRECTION_FRONT
                            : AR_CAMERA_CONFIG_FACING_DIRECTION_BACK;

                    ArCameraConfigFilter *filter = nullptr;
                    ArCameraConfigFilter_create(ArSessionManager::Instance().Get(), &filter);
                    ArCameraConfigFilter_setFacingDirection(
                        ArSessionManager::Instance().Get(), filter, facing);

                    ArCameraConfigList *config_list = nullptr;
                    ArCameraConfigList_create(ArSessionManager::Instance().Get(), &config_list);
                    ArSession_getSupportedCameraConfigsWithFilter(
                        ArSessionManager::Instance().Get(), filter, config_list);

                    int32_t config_count = 0;
                    ArCameraConfigList_getSize(
                        ArSessionManager::Instance().Get(), config_list, &config_count);

                    if (config_count > 0)
                    {
                        ArCameraConfig *camera_config = nullptr;
                        ArCameraConfig_create(ArSessionManager::Instance().Get(), &camera_config);
                        ArCameraConfigList_getItem(
                            ArSessionManager::Instance().Get(), config_list, 0, camera_config);
                        ArSession_setCameraConfig(
                            ArSessionManager::Instance().Get(), camera_config);
                        ArCameraConfig_destroy(camera_config);
                        LOGI("Camera facing: %s",
                             facing == AR_CAMERA_CONFIG_FACING_DIRECTION_FRONT ? "FRONT" : "BACK");
                    }
                    else
                    {
                        LOGE("No camera configs found for requested facing direction");
                    }

                    ArCameraConfigList_destroy(config_list);
                    ArCameraConfigFilter_destroy(filter);
                }

                if (session_type_ == "face")
                {
                    ArConfig_setAugmentedFaceMode(ArSessionManager::Instance().Get(),
                        ar_config, AR_AUGMENTED_FACE_MODE_MESH3D);
                    // Front-camera augmented-face sessions do not support plane
                    // finding or depth; leaving them enabled makes ArSession_configure
                    // fail (unsupported config), which silently disables face mode.
                    ArConfig_setPlaneFindingMode(ArSessionManager::Instance().Get(),
                        ar_config, AR_PLANE_FINDING_MODE_DISABLED);
                    ArConfig_setDepthMode(ArSessionManager::Instance().Get(),
                        ar_config, AR_DEPTH_MODE_DISABLED);
                    LOGI("Configured face tracking mode");
                }
                else
                {
                    int32_t is_depth_supported = 0;
                    ArSession_isDepthModeSupported(ArSessionManager::Instance().Get(),
                        AR_DEPTH_MODE_AUTOMATIC, &is_depth_supported);
                    if (is_depth_supported)
                    {
                        ArConfig_setDepthMode(ArSessionManager::Instance().Get(), ar_config,
                            AR_DEPTH_MODE_AUTOMATIC);
                    }

                    ArPlaneFindingMode plane_mode = AR_PLANE_FINDING_MODE_HORIZONTAL_AND_VERTICAL;
                    if (plane_detection_mode_ == "none")
                        plane_mode = AR_PLANE_FINDING_MODE_DISABLED;
                    else if (plane_detection_mode_ == "horizontal")
                        plane_mode = AR_PLANE_FINDING_MODE_HORIZONTAL;
                    else if (plane_detection_mode_ == "vertical")
                        plane_mode = AR_PLANE_FINDING_MODE_VERTICAL;
                    ArConfig_setPlaneFindingMode(ArSessionManager::Instance().Get(),
                        ar_config, plane_mode);
                }

                if (ArSession_configure(ArSessionManager::Instance().Get(), ar_config) != AR_SUCCESS)
                {
                    LOGE("ArSession_configure failed for session_type=%s", session_type_.c_str());
                }
                ArConfig_destroy(ar_config);

                ArSession_setDisplayGeometry(ArSessionManager::Instance().Get(), display_rotation_, width_, height_);
                ArSessionManager::Instance().Resume();
                return;
            }

            if (active_view_count_ > 0 && ArSessionManager::Instance().IsInitialized())
            {
                ArSessionManager::Instance().Resume();
                return;
            }

            LOGI("AR Session not started: no active AR views.");
        }

        void OnARViewMounted()
        {
            active_view_count_++;
            if (activity_resumed_ && ArSessionManager::Instance().IsInitialized())
            {
                ArSessionManager::Instance().Resume();
            }
        }

        void OnARViewUnmounted()
        {
            if (active_view_count_ > 0)
            {
                active_view_count_--;
            }

            if (active_view_count_ == 0)
            {
                ArSessionManager::Instance().Pause();
            }
        }

        void DestroySession()
        {
            ArSessionManager::Instance().Pause();
            if (ar_frame_ != nullptr)
            {
                ArFrame_destroy(ar_frame_);
                ar_frame_ = nullptr;
            }
            ArSessionManager::Instance().Destroy();
            {
                std::lock_guard<std::mutex> lock(events_mutex_);
                pending_events_.clear();
            }
            {
                std::lock_guard<std::mutex> lock(face_mutex_);
                tracked_faces_.clear();
            }
            tracked_planes_.clear();
            tracking_state_reported_ = false;
            last_tracking_state_ = AR_TRACKING_STATE_STOPPED;
            last_failure_reason_ = AR_TRACKING_FAILURE_REASON_NONE;
            {
                std::lock_guard<std::mutex> lock(face_filters_mutex_);
                face_filter_descs_.clear();
            }
        }

        bool IsSessionInitialized()
        {
            return ArSessionManager::Instance().IsInitialized();
        }

        // The session type the live session was configured with ("world"/"face").
        std::string GetSessionType() { return session_type_; }

        void OnSurfaceCreated(JNIEnv * /*env*/) {}
        void OnSurfaceChanged(jint /*width*/, jint /*height*/) {}
        void OnSurfaceDestroyed() {}

        void OnGlSurfaceCreated()
        {
            LOGI("OnGlSurfaceCreated()");
            depth_texture_.CreateOnGlThread();
            background_renderer_.InitializeGlContent(asset_manager_, depth_texture_.GetTextureId());

            auto andy = std::make_unique<ObjRenderer>();
            andy->InitializeGlContent(asset_manager_, "models/andy.obj", "models/andy.png");
            andy->SetMaterialProperty(0.0f, 2.0f, 0.5f, 6.0f);
            obj_renderers_["models/andy.obj"] = std::move(andy);
        }

        void OnGlSurfaceChanged(jint width, jint height)
        {
            LOGI("OnGlSurfaceChanged(%d, %d)", width, height);
            glViewport(0, 0, width, height);
            width_ = width;
            height_ = height;
            ArSession *session = ArSessionManager::Instance().Get();
            if (session != nullptr)
            {
                ArSession_setDisplayGeometry(session, display_rotation_, width, height);
            }
        }

        void OnGlDrawFrame()
        {
            glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            ArSession *session = ArSessionManager::Instance().Get();
            if (session == nullptr || ar_frame_ == nullptr)
                return;

            ArSession_setCameraTextureName(session, background_renderer_.GetTextureId());

            if (ArSession_update(session, ar_frame_) != AR_SUCCESS)
            {
                LOGE("OnGlDrawFrame ArSession_update error");
                return;
            }

            // Depth is world-only; the front-camera face session has no depth image.
            if (session_type_ != "face")
            {
                int32_t is_depth_supported = 0;
                ArSession_isDepthModeSupported(session, AR_DEPTH_MODE_AUTOMATIC, &is_depth_supported);
                if (is_depth_supported)
                {
                    depth_texture_.UpdateWithDepthImageOnGlThread(*session, *ar_frame_);
                }
            }

            ArCamera *ar_camera;
            ArFrame_acquireCamera(session, ar_frame_, &ar_camera);
            background_renderer_.Draw(session, ar_frame_, debug_show_depth_map_);

            ArTrackingState camera_tracking_state;
            ArCamera_getTrackingState(session, ar_camera, &camera_tracking_state);

            ArTrackingFailureReason failure_reason = AR_TRACKING_FAILURE_REASON_NONE;
            if (camera_tracking_state != AR_TRACKING_STATE_TRACKING)
            {
                ArCamera_getTrackingFailureReason(session, ar_camera, &failure_reason);
            }
            EmitTrackingStateIfChanged(camera_tracking_state, failure_reason);

            if (session_type_ == "face")
            {
                // Augmented Faces performs no world/motion tracking, so the ArCamera
                // tracking state stays PAUSED ("limited"). Process faces every frame
                // regardless — ProcessFaces checks each face's own tracking state.
                glm::mat4 view_mat;
                glm::mat4 projection_mat;
                ArCamera_getViewMatrix(session, ar_camera, glm::value_ptr(view_mat));
                ArCamera_getProjectionMatrix(session, ar_camera, 0.1f, 100.f, glm::value_ptr(projection_mat));
                ProcessFaces(session, view_mat, projection_mat);
            }
            else if (camera_tracking_state == AR_TRACKING_STATE_TRACKING)
            {
                {
                    glm::mat4 view_mat;
                    glm::mat4 projection_mat;
                    ArCamera_getViewMatrix(session, ar_camera, glm::value_ptr(view_mat));
                    ArCamera_getProjectionMatrix(session, ar_camera, 0.1f, 100.f, glm::value_ptr(projection_mat));

                    if (plane_detection_mode_ != "none")
                    {
                        ProcessPlanes(session);
                    }

                    float color_correction[4] = {1.f, 1.f, 1.f, 1.f};

                    std::lock_guard<std::mutex> lock(objects_mutex_);
                    for (const auto &desc : ar_object_descs_)
                    {
                        if (!desc.visible)
                            continue;
                        auto anchor_it = managed_anchors_.find(desc.anchorId);
                        if (anchor_it == managed_anchors_.end())
                            continue;

                        ArTrackingState anchor_state;
                        ArAnchor_getTrackingState(session, anchor_it->second, &anchor_state);
                        if (anchor_state != AR_TRACKING_STATE_TRACKING)
                            continue;

                        glm::mat4 model_mat;
                        utils::GetTransformMatrixFromAnchor(*anchor_it->second, session, &model_mat);
                        model_mat = glm::scale(model_mat, glm::vec3(desc.scale[0], desc.scale[1], desc.scale[2]));

                        auto renderer_it = obj_renderers_.find(desc.model);
                        if (renderer_it == obj_renderers_.end())
                            renderer_it = obj_renderers_.find("models/" + desc.model + ".obj");
                        if (renderer_it == obj_renderers_.end())
                            renderer_it = obj_renderers_.find(desc.model + ".obj");
                        if (renderer_it != obj_renderers_.end())
                        {
                            renderer_it->second->Draw(projection_mat, view_mat, model_mat,
                                                      color_correction, desc.color);
                        }
                    }
                }
            }

            ArCamera_release(ar_camera);
        }

        // Handles a screen tap on the GL thread. Always emits a TAP event (so the
        // JS layer can observe every tap, even misses); additionally emits
        // ANCHOR_CREATED when the tap lands inside a tracked plane.
        void OnGestureTap(jfloat x, jfloat y)
        {
            ArSession *session = ArSessionManager::Instance().Get();
            if (session == nullptr || ar_frame_ == nullptr)
            {
                AREvent tap;
                tap.category = AREvent::TAP;
                tap.scalars[0] = x;
                tap.scalars[1] = y;
                tap.scalars[2] = 0.0; // hasHit = false
                PushEvent(std::move(tap));
                return;
            }

            ArHitResultList *hit_result_list = nullptr;
            ArHitResultList_create(session, &hit_result_list);
            ArFrame_hitTest(session, ar_frame_, x, y, hit_result_list);

            int32_t hit_result_count = 0;
            ArHitResultList_getSize(session, hit_result_list, &hit_result_count);

            std::string anchor_id;
            float world[3] = {0.f, 0.f, 0.f};

            for (int32_t i = 0; i < hit_result_count; ++i)
            {
                ArHitResult *hit_result = nullptr;
                ArHitResult_create(session, &hit_result);
                ArHitResultList_getItem(session, hit_result_list, i, hit_result);

                ArTrackable *trackable = nullptr;
                ArHitResult_acquireTrackable(session, hit_result, &trackable);
                ArTrackableType trackable_type;
                ArTrackable_getType(session, trackable, &trackable_type);

                if (trackable_type == AR_TRACKABLE_PLANE)
                {
                    ArPose *hit_pose = nullptr;
                    ArPose_create(session, nullptr, &hit_pose);
                    ArHitResult_getHitPose(session, hit_result, hit_pose);

                    int32_t in_polygon = 0;
                    ArPlane *plane = ArAsPlane(trackable);
                    ArPlane_isPoseInPolygon(session, plane, hit_pose, &in_polygon);

                    if (in_polygon)
                    {
                        float pose_raw[7] = {};
                        ArPose_getPoseRaw(session, hit_pose, pose_raw);
                        world[0] = pose_raw[4];
                        world[1] = pose_raw[5];
                        world[2] = pose_raw[6];

                        ArAnchor *anchor = nullptr;
                        if (ArHitResult_acquireNewAnchor(session, hit_result, &anchor) == AR_SUCCESS)
                        {
                            anchor_id = "anchor_" + std::to_string(anchor_counter_++);
                            managed_anchors_[anchor_id] = anchor;
                        }
                    }
                    ArPose_destroy(hit_pose);
                }

                ArTrackable_release(trackable);
                ArHitResult_destroy(hit_result);

                if (!anchor_id.empty())
                    break;
            }

            ArHitResultList_destroy(hit_result_list);

            bool hasHit = !anchor_id.empty();

            AREvent tap;
            tap.category = AREvent::TAP;
            tap.idA = anchor_id; // empty when no anchor
            tap.scalars[0] = x;
            tap.scalars[1] = y;
            tap.scalars[2] = hasHit ? 1.0 : 0.0;
            PushEvent(std::move(tap));

            if (hasHit)
            {
                AREvent anchor_event;
                anchor_event.category = AREvent::ANCHOR_CREATED;
                anchor_event.idA = anchor_id;
                anchor_event.scalars[0] = world[0];
                anchor_event.scalars[1] = world[1];
                anchor_event.scalars[2] = world[2];
                PushEvent(std::move(anchor_event));
            }
        }

        void SetDisplayRotation(jint rotation)
        {
            display_rotation_ = rotation;
            ArSession *session = ArSessionManager::Instance().Get();
            if (session != nullptr)
            {
                ArSession_setDisplayGeometry(session, display_rotation_, width_, height_);
            }
        }

        void SetPaused(jboolean /*paused*/) {}
        void SetPlaneDetectionEnabled(jboolean /*enabled*/) {}
        void SetLightEstimationEnabled(jboolean /*enabled*/) {}

        void SetSessionType(const std::string &sessionType) {
            session_type_ = sessionType;
            std::transform(session_type_.begin(), session_type_.end(), session_type_.begin(), ::tolower);
        }
        void SetCameraFacing(const std::string &facing) {
            camera_facing_ = facing;
            std::transform(camera_facing_.begin(), camera_facing_.end(), camera_facing_.begin(), ::tolower);
        }
        void SetDepthMode(const std::string & /*depthMode*/) {}
        void SetCloudAnchorMode(const std::string & /*cloudAnchorMode*/) {}
        void SetInstantPlacementMode(const std::string & /*instantPlacementMode*/) {}
        void SetLightEstimationMode(const std::string & /*lightEstimationMode*/) {}
        void SetPlaneDetectionMode(const std::string &planeDetectionMode) {
            plane_detection_mode_ = planeDetectionMode;
            std::transform(plane_detection_mode_.begin(), plane_detection_mode_.end(),
                           plane_detection_mode_.begin(), ::tolower);
        }
        void SetFocusMode(const std::string & /*focusMode*/) {}

        void SetDebugShowPlanes(jboolean /*enabled*/) {}
        void SetDebugShowPointCloud(jboolean /*enabled*/) {}
        void SetDebugShowWorldOrigin(jboolean /*enabled*/) {}
        void SetDebugShowDepthMap(jboolean enabled) { debug_show_depth_map_ = enabled; }

        void SetShaderMode(const std::string &shaderMode)
        {
            debug_show_depth_map_ = (shaderMode == "depth");
        }

        void SetARObjects(std::vector<ARObjectDesc> new_descs)
        {
            std::lock_guard<std::mutex> lock(objects_mutex_);
            ar_object_descs_ = std::move(new_descs);
        }

        void SetFaceFilters(std::vector<FaceFilterDesc> new_descs)
        {
            std::lock_guard<std::mutex> lock(face_filters_mutex_);
            face_filter_descs_ = std::move(new_descs);
        }

        std::string CreateAnchor(float screen_x, float screen_y)
        {
            ArSession *session = ArSessionManager::Instance().Get();
            if (session == nullptr || ar_frame_ == nullptr)
                return "";

            ArHitResultList *hit_result_list = nullptr;
            ArHitResultList_create(session, &hit_result_list);
            ArFrame_hitTest(session, ar_frame_, screen_x, screen_y, hit_result_list);

            int32_t hit_result_count = 0;
            ArHitResultList_getSize(session, hit_result_list, &hit_result_count);

            std::string anchor_id;
            for (int32_t i = 0; i < hit_result_count; ++i)
            {
                ArHitResult *hit_result = nullptr;
                ArHitResult_create(session, &hit_result);
                ArHitResultList_getItem(session, hit_result_list, i, hit_result);

                ArTrackable *trackable = nullptr;
                ArHitResult_acquireTrackable(session, hit_result, &trackable);
                ArTrackableType trackable_type;
                ArTrackable_getType(session, trackable, &trackable_type);

                if (trackable_type == AR_TRACKABLE_PLANE)
                {
                    ArPose *hit_pose = nullptr;
                    ArPose_create(session, nullptr, &hit_pose);
                    ArHitResult_getHitPose(session, hit_result, hit_pose);

                    int32_t in_polygon = 0;
                    ArPlane *plane = ArAsPlane(trackable);
                    ArPlane_isPoseInPolygon(session, plane, hit_pose, &in_polygon);
                    ArPose_destroy(hit_pose);

                    if (in_polygon)
                    {
                        ArAnchor *anchor = nullptr;
                        if (ArHitResult_acquireNewAnchor(session, hit_result, &anchor) == AR_SUCCESS)
                        {
                            anchor_id = "anchor_" + std::to_string(anchor_counter_++);
                            managed_anchors_[anchor_id] = anchor;
                        }
                    }
                }

                ArTrackable_release(trackable);
                ArHitResult_destroy(hit_result);

                if (!anchor_id.empty())
                    break;
            }

            ArHitResultList_destroy(hit_result_list);
            return anchor_id;
        }

        void RemoveAnchor(const std::string &anchorId)
        {
            auto it = managed_anchors_.find(anchorId);
            if (it != managed_anchors_.end())
            {
                ArAnchor_detach(ArSessionManager::Instance().Get(), it->second);
                ArAnchor_release(it->second);
                managed_anchors_.erase(it);
            }
        }

        bool IsDepthModeSupported()
        {
            ArSession *session = ArSessionManager::Instance().Get();
            if (session == nullptr)
                return false;
            int32_t is_supported = 0;
            ArSession_isDepthModeSupported(session, AR_DEPTH_MODE_AUTOMATIC, &is_supported);
            return is_supported != 0;
        }

    private:
        // Emits a TRACKING_STATE event only when the camera tracking state or its
        // failure reason changes, so JS isn't spammed every frame.
        void EmitTrackingStateIfChanged(ArTrackingState state, ArTrackingFailureReason reason)
        {
            if (tracking_state_reported_ && state == last_tracking_state_ &&
                reason == last_failure_reason_)
            {
                return;
            }
            last_tracking_state_ = state;
            last_failure_reason_ = reason;
            tracking_state_reported_ = true;

            AREvent event;
            event.category = AREvent::TRACKING_STATE;
            switch (state)
            {
            case AR_TRACKING_STATE_TRACKING: event.strB = "tracking";    break;
            case AR_TRACKING_STATE_PAUSED:   event.strB = "limited";     break;
            default:                          event.strB = "unavailable"; break;
            }
            switch (reason)
            {
            case AR_TRACKING_FAILURE_REASON_NONE:                  event.idA = ""; break;
            case AR_TRACKING_FAILURE_REASON_BAD_STATE:             event.idA = "badState"; break;
            case AR_TRACKING_FAILURE_REASON_INSUFFICIENT_LIGHT:   event.idA = "insufficientLight"; break;
            case AR_TRACKING_FAILURE_REASON_EXCESSIVE_MOTION:     event.idA = "excessiveMotion"; break;
            case AR_TRACKING_FAILURE_REASON_INSUFFICIENT_FEATURES:event.idA = "insufficientFeatures"; break;
            case AR_TRACKING_FAILURE_REASON_CAMERA_UNAVAILABLE:   event.idA = "cameraUnavailable"; break;
            default:                                               event.idA = ""; break;
            }
            PushEvent(std::move(event));
        }

        // Surfaces detected planes to JS. Mirrors ProcessFaces: diffs a tracked set
        // to emit PLANE_DETECTED / PLANE_UPDATED / PLANE_REMOVED. Data only — no
        // rendering (debugShowPlanes visual rendering is a follow-up).
        void ProcessPlanes(ArSession *session)
        {
            ArTrackableList *plane_list = nullptr;
            ArTrackableList_create(session, &plane_list);
            ArSession_getAllTrackables(session, AR_TRACKABLE_PLANE, plane_list);

            int32_t plane_count = 0;
            ArTrackableList_getSize(session, plane_list, &plane_count);

            std::set<std::string> current_plane_ids;

            for (int32_t i = 0; i < plane_count; ++i)
            {
                ArTrackable *trackable = nullptr;
                ArTrackableList_acquireItem(session, plane_list, i, &trackable);

                ArTrackingState tracking_state;
                ArTrackable_getTrackingState(session, trackable, &tracking_state);

                ArPlane *plane = ArAsPlane(trackable);

                // Skip planes that have been subsumed by a larger plane.
                ArPlane *subsumed_by = nullptr;
                ArPlane_acquireSubsumedBy(session, plane, &subsumed_by);
                bool is_subsumed = (subsumed_by != nullptr);
                if (subsumed_by != nullptr)
                {
                    ArTrackable_release(ArAsTrackable(subsumed_by));
                }

                if (tracking_state == AR_TRACKING_STATE_TRACKING && !is_subsumed)
                {
                    char id_buf[32];
                    snprintf(id_buf, sizeof(id_buf), "plane_%p", (void *)trackable);
                    std::string planeId(id_buf);
                    current_plane_ids.insert(planeId);

                    ArPose *center_pose = nullptr;
                    ArPose_create(session, nullptr, &center_pose);
                    ArPlane_getCenterPose(session, plane, center_pose);
                    float raw[7] = {};
                    ArPose_getPoseRaw(session, center_pose, raw);
                    ArPose_destroy(center_pose);

                    float extent_x = 0.f, extent_z = 0.f;
                    ArPlane_getExtentX(session, plane, &extent_x);
                    ArPlane_getExtentZ(session, plane, &extent_z);

                    ArPlaneType plane_type;
                    ArPlane_getType(session, plane, &plane_type);
                    std::string type_str;
                    switch (plane_type)
                    {
                    case AR_PLANE_HORIZONTAL_UPWARD_FACING:   type_str = "horizontal"; break;
                    case AR_PLANE_HORIZONTAL_DOWNWARD_FACING: type_str = "horizontal"; break;
                    case AR_PLANE_VERTICAL:                   type_str = "vertical"; break;
                    default:                                   type_str = "unknown"; break;
                    }

                    bool is_new = tracked_planes_.find(planeId) == tracked_planes_.end();
                    tracked_planes_.insert(planeId);

                    AREvent event;
                    event.category = is_new ? AREvent::PLANE_DETECTED : AREvent::PLANE_UPDATED;
                    event.idA = planeId;
                    event.strB = type_str;
                    event.scalars[0] = raw[4]; // centerX
                    event.scalars[1] = raw[5]; // centerY
                    event.scalars[2] = raw[6]; // centerZ
                    event.scalars[3] = extent_x;
                    event.scalars[4] = extent_z;
                    PushEvent(std::move(event));
                }

                ArTrackable_release(trackable);
            }

            // Emit PLANE_REMOVED for planes that are no longer tracked.
            for (auto it = tracked_planes_.begin(); it != tracked_planes_.end();)
            {
                if (current_plane_ids.find(*it) == current_plane_ids.end())
                {
                    AREvent removed;
                    removed.category = AREvent::PLANE_REMOVED;
                    removed.idA = *it;
                    PushEvent(std::move(removed));
                    it = tracked_planes_.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            ArTrackableList_destroy(plane_list);
        }

        static glm::mat4 PoseToMatrix(ArSession *session, ArPose *pose)
        {
            float raw[7];
            ArPose_getPoseRaw(session, pose, raw);
            float qx = raw[0], qy = raw[1], qz = raw[2], qw = raw[3];
            float tx = raw[4], ty = raw[5], tz = raw[6];

            glm::mat4 m;
            float *t = glm::value_ptr(m);
            t[0]  = 1.f - 2.f*(qy*qy + qz*qz);
            t[1]  = 2.f*(qx*qy + qw*qz);
            t[2]  = 2.f*(qx*qz - qw*qy);
            t[3]  = 0.f;
            t[4]  = 2.f*(qx*qy - qw*qz);
            t[5]  = 1.f - 2.f*(qx*qx + qz*qz);
            t[6]  = 2.f*(qy*qz + qw*qx);
            t[7]  = 0.f;
            t[8]  = 2.f*(qx*qz + qw*qy);
            t[9]  = 2.f*(qy*qz - qw*qx);
            t[10] = 1.f - 2.f*(qx*qx + qy*qy);
            t[11] = 0.f;
            t[12] = tx;
            t[13] = ty;
            t[14] = tz;
            t[15] = 1.f;
            return m;
        }

        static glm::vec3 AttachmentPointOffset(const std::string &point)
        {
            if (point == "forehead")   return {0.f, 0.08f, 0.02f};
            if (point == "noseTip")    return {0.f, -0.02f, 0.06f};
            if (point == "noseBridge") return {0.f, 0.02f, 0.05f};
            if (point == "leftEye")    return {-0.03f, 0.02f, 0.04f};
            if (point == "rightEye")   return {0.03f, 0.02f, 0.04f};
            if (point == "leftEar")    return {-0.08f, 0.01f, -0.01f};
            if (point == "rightEar")   return {0.08f, 0.01f, -0.01f};
            if (point == "chin")       return {0.f, -0.07f, 0.03f};
            if (point == "mouthCenter") return {0.f, -0.04f, 0.04f};
            if (point == "foreheadLeft")  return {0.f, 0.f, 0.f};
            if (point == "foreheadRight") return {0.f, 0.f, 0.f};
            return {0.f, 0.f, 0.f};
        }

        static ArAugmentedFaceRegionType AttachmentPointToRegion(const std::string &point, bool &use_region)
        {
            if (point == "noseTip" || point == "noseBridge" || point == "mouthCenter" || point == "chin")
            {
                use_region = true;
                return AR_AUGMENTED_FACE_REGION_NOSE_TIP;
            }
            if (point == "leftEye" || point == "leftEar" || point == "foreheadLeft")
            {
                use_region = true;
                return AR_AUGMENTED_FACE_REGION_FOREHEAD_LEFT;
            }
            if (point == "rightEye" || point == "rightEar" || point == "foreheadRight")
            {
                use_region = true;
                return AR_AUGMENTED_FACE_REGION_FOREHEAD_RIGHT;
            }
            if (point == "forehead")
            {
                use_region = false;
                return AR_AUGMENTED_FACE_REGION_NOSE_TIP;
            }
            use_region = false;
            return AR_AUGMENTED_FACE_REGION_NOSE_TIP;
        }

        ObjRenderer *EnsureRenderer(const std::string &model)
        {
            auto it = obj_renderers_.find(model);
            if (it != obj_renderers_.end())
                return it->second.get();

            std::string obj_path = model;
            std::string tex_path;

            if (obj_path.find(".obj") == std::string::npos)
                obj_path = "models/" + model + ".obj";

            size_t dot = obj_path.rfind('.');
            if (dot != std::string::npos)
                tex_path = obj_path.substr(0, dot) + ".png";

            auto renderer = std::make_unique<ObjRenderer>();
            renderer->InitializeGlContent(asset_manager_, obj_path, tex_path);
            renderer->SetMaterialProperty(0.0f, 2.0f, 0.5f, 6.0f);
            auto *ptr = renderer.get();
            obj_renderers_[model] = std::move(renderer);
            return ptr;
        }

        void ProcessFaces(ArSession *session, const glm::mat4 &view_mat, const glm::mat4 &projection_mat)
        {
            ArTrackableList *face_list = nullptr;
            ArTrackableList_create(session, &face_list);
            ArSession_getAllTrackables(session, AR_TRACKABLE_FACE, face_list);

            int32_t face_count = 0;
            ArTrackableList_getSize(session, face_list, &face_count);

            std::set<std::string> current_face_ids;

            for (int32_t i = 0; i < face_count; ++i)
            {
                ArTrackable *trackable = nullptr;
                ArTrackableList_acquireItem(session, face_list, i, &trackable);

                ArTrackingState tracking_state;
                ArTrackable_getTrackingState(session, trackable, &tracking_state);

                if (tracking_state == AR_TRACKING_STATE_TRACKING)
                {
                    ArAugmentedFace *face = ArAsFace(trackable);

                    ArPose *center_pose = nullptr;
                    ArPose_create(session, nullptr, &center_pose);
                    ArAugmentedFace_getCenterPose(session, face, center_pose);

                    glm::mat4 center_mat = PoseToMatrix(session, center_pose);

                    char id_buf[32];
                    snprintf(id_buf, sizeof(id_buf), "face_%p", (void *)trackable);
                    std::string faceId(id_buf);
                    current_face_ids.insert(faceId);

                    {
                        std::lock_guard<std::mutex> lock(face_mutex_);
                        bool is_new = tracked_faces_.find(faceId) == tracked_faces_.end();
                        tracked_faces_.insert(faceId);

                        AREvent event;
                        event.category = is_new ? AREvent::FACE_DETECTED : AREvent::FACE_UPDATED;
                        event.idA = faceId;
                        const float *src = glm::value_ptr(center_mat);
                        for (int k = 0; k < 16; ++k)
                            event.transform[k] = static_cast<double>(src[k]);
                        PushEvent(std::move(event));
                    }

                    // Render face filters
                    {
                        std::lock_guard<std::mutex> lock(face_filters_mutex_);
                        for (const auto &desc : face_filter_descs_)
                        {
                            if (!desc.visible)
                                continue;

                            bool use_region = false;
                            ArAugmentedFaceRegionType region = AttachmentPointToRegion(desc.attachmentPoint, use_region);

                            glm::mat4 base_mat;
                            if (use_region)
                            {
                                ArPose *region_pose = nullptr;
                                ArPose_create(session, nullptr, &region_pose);
                                ArAugmentedFace_getRegionPose(session, face, region, region_pose);
                                base_mat = PoseToMatrix(session, region_pose);
                                ArPose_destroy(region_pose);
                            }
                            else
                            {
                                base_mat = center_mat;
                            }

                            glm::vec3 offset = AttachmentPointOffset(desc.attachmentPoint)
                                              + glm::vec3(desc.offset[0], desc.offset[1], desc.offset[2]);
                            glm::mat4 model_mat = glm::translate(base_mat, offset);
                            model_mat = glm::scale(model_mat, glm::vec3(desc.scale[0], desc.scale[1], desc.scale[2]));

                            if (desc.rotation[0] != 0.f)
                                model_mat = glm::rotate(model_mat, desc.rotation[0], glm::vec3(1.f, 0.f, 0.f));
                            if (desc.rotation[1] != 0.f)
                                model_mat = glm::rotate(model_mat, desc.rotation[1], glm::vec3(0.f, 1.f, 0.f));
                            if (desc.rotation[2] != 0.f)
                                model_mat = glm::rotate(model_mat, desc.rotation[2], glm::vec3(0.f, 0.f, 1.f));

                            ObjRenderer *renderer = EnsureRenderer(desc.model);
                            if (renderer)
                            {
                                float color[] = {1.f, 1.f, 1.f, 1.f};
                                renderer->Draw(projection_mat, view_mat, model_mat, color, color);
                            }
                        }
                    }

                    ArPose_destroy(center_pose);
                }

                ArTrackable_release(trackable);
            }

            // Check for lost faces
            {
                std::lock_guard<std::mutex> lock(face_mutex_);
                for (auto it = tracked_faces_.begin(); it != tracked_faces_.end();)
                {
                    if (current_face_ids.find(*it) == current_face_ids.end())
                    {
                        AREvent lost_event;
                        lost_event.category = AREvent::FACE_LOST;
                        lost_event.idA = *it;
                        PushEvent(std::move(lost_event));
                        it = tracked_faces_.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            ArTrackableList_destroy(face_list);
        }

        std::unique_ptr<AndroidPlatformServices> platformServices_;
        int active_view_count_ = 0;
        bool activity_resumed_ = false;

        glm::mat3 GetTextureTransformMatrix(const ArSession *session,
                                            const ArFrame *frame);
        ArFrame *ar_frame_ = nullptr;

        bool install_requested_ = false;
        bool calculate_uv_transform_ = false;
        int width_ = 1;
        int height_ = 1;
        int display_rotation_ = 0;
        bool is_instant_placement_enabled_ = true;
        bool debug_show_depth_map_ = false;
        std::string session_type_;
        std::string camera_facing_ = "back";

        AAssetManager *const asset_manager_;

        BackgroundRenderer background_renderer_;
        Texture depth_texture_;

        std::unordered_map<std::string, ArAnchor *> managed_anchors_;
        std::vector<ARObjectDesc> ar_object_descs_;
        std::unordered_map<std::string, std::unique_ptr<ObjRenderer>> obj_renderers_;
        std::mutex objects_mutex_;
        int anchor_counter_ = 0;

        // Unified per-frame event queue (Tier 2). All producers push here.
        std::vector<AREvent> pending_events_;
        std::mutex events_mutex_;

        // Face tracking bookkeeping (events themselves go through pending_events_).
        std::set<std::string> tracked_faces_;
        std::mutex face_mutex_;

        // Plane tracking bookkeeping.
        std::set<std::string> tracked_planes_;
        std::string plane_detection_mode_ = "both";

        // Last-reported camera tracking state (debounce TRACKING_STATE events).
        ArTrackingState last_tracking_state_ = AR_TRACKING_STATE_STOPPED;
        ArTrackingFailureReason last_failure_reason_ = AR_TRACKING_FAILURE_REASON_NONE;
        bool tracking_state_reported_ = false;

        std::vector<FaceFilterDesc> face_filter_descs_;
        std::mutex face_filters_mutex_;
    };
}
