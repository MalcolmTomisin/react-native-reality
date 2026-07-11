#pragma once

#include <memory>
#include <string>
#include <set>
#include <mutex>
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
    class ARApplication
    {
    public:
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
                int32_t is_depth_supported = 0;
                ArSession_isDepthModeSupported(ArSessionManager::Instance().Get(),
                    AR_DEPTH_MODE_AUTOMATIC, &is_depth_supported);
                if (is_depth_supported)
                {
                    ArConfig_setDepthMode(ArSessionManager::Instance().Get(), ar_config,
                        AR_DEPTH_MODE_AUTOMATIC);
                }
                ArSession_configure(ArSessionManager::Instance().Get(), ar_config);
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
            ArSessionManager::Instance().Destroy();
        }

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

            int32_t is_depth_supported = 0;
            ArSession_isDepthModeSupported(session, AR_DEPTH_MODE_AUTOMATIC, &is_depth_supported);
            if (is_depth_supported)
            {
                depth_texture_.UpdateWithDepthImageOnGlThread(*session, *ar_frame_);
            }

            ArCamera *ar_camera;
            ArFrame_acquireCamera(session, ar_frame_, &ar_camera);
            background_renderer_.Draw(session, ar_frame_, debug_show_depth_map_);

            ArTrackingState camera_tracking_state;
            ArCamera_getTrackingState(session, ar_camera, &camera_tracking_state);

            if (camera_tracking_state == AR_TRACKING_STATE_TRACKING)
            {
                glm::mat4 view_mat;
                glm::mat4 projection_mat;
                ArCamera_getViewMatrix(session, ar_camera, glm::value_ptr(view_mat));
                ArCamera_getProjectionMatrix(session, ar_camera, 0.1f, 100.f, glm::value_ptr(projection_mat));

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

            ArCamera_release(ar_camera);
        }

        void OnGestureTap(jfloat /*x*/, jfloat /*y*/) {}

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

        void SetSessionType(const std::string & /*sessionType*/) {}
        void SetDepthMode(const std::string & /*depthMode*/) {}
        void SetCloudAnchorMode(const std::string & /*cloudAnchorMode*/) {}
        void SetInstantPlacementMode(const std::string & /*instantPlacementMode*/) {}
        void SetLightEstimationMode(const std::string & /*lightEstimationMode*/) {}
        void SetPlaneDetectionMode(const std::string & /*planeDetectionMode*/) {}
        void SetFocusMode(const std::string & /*focusMode*/) {}

        void SetDebugShowPlanes(jboolean /*enabled*/) {}
        void SetDebugShowPointCloud(jboolean /*enabled*/) {}
        void SetDebugShowWorldOrigin(jboolean /*enabled*/) {}
        void SetDebugShowDepthMap(jboolean enabled) { debug_show_depth_map_ = enabled; }

        void SetShaderMode(const std::string &shaderMode)
        {
            debug_show_depth_map_ = (shaderMode == "depth");
        }

        void SetARObjects(const std::string &json)
        {
            std::vector<ARObjectDesc> new_descs;
            size_t pos = 0;
            while ((pos = json.find('{', pos)) != std::string::npos)
            {
                ARObjectDesc desc;
                size_t end = json.find('}', pos);
                if (end == std::string::npos)
                    break;
                std::string obj = json.substr(pos, end - pos + 1);

                auto extractString = [&obj](const std::string &key) -> std::string
                {
                    std::string search = "\"" + key + "\":\"";
                    size_t start = obj.find(search);
                    if (start == std::string::npos)
                        return "";
                    start += search.length();
                    size_t finish = obj.find('"', start);
                    if (finish == std::string::npos)
                        return "";
                    return obj.substr(start, finish - start);
                };

                desc.id = extractString("id");
                desc.anchorId = extractString("anchorId");
                desc.model = extractString("model");

                if (!desc.id.empty() && !desc.anchorId.empty() && !desc.model.empty())
                {
                    new_descs.push_back(desc);
                }
                pos = end + 1;
            }

            std::lock_guard<std::mutex> lock(objects_mutex_);
            ar_object_descs_ = std::move(new_descs);
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

        AAssetManager *const asset_manager_;

        struct ARObjectDesc
        {
            std::string id;
            std::string anchorId;
            std::string model;
            float scale[3] = {1.f, 1.f, 1.f};
            float color[4] = {1.f, 1.f, 1.f, 1.f};
            bool visible = true;
        };

        BackgroundRenderer background_renderer_;
        Texture depth_texture_;

        std::unordered_map<std::string, ArAnchor *> managed_anchors_;
        std::vector<ARObjectDesc> ar_object_descs_;
        std::unordered_map<std::string, std::unique_ptr<ObjRenderer>> obj_renderers_;
        std::mutex objects_mutex_;
        int anchor_counter_ = 0;

        int32_t plane_count_ = 0;
    };
}
