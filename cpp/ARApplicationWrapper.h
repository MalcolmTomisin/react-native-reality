#pragma once

#include <fbjni/fbjni.h>
#include <string>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "ARApplication.h"
#include "AndroidPlatformServices.h"

class ARApplicationWrapper : public facebook::jni::HybridClass<ARApplicationWrapper>
{
public:
    static constexpr auto kJavaDescriptor = "Lcom/margelo/nitro/arcore/ARAppSystem;";

    std::shared_ptr<arcore::ARApplication> arApplication_;

    explicit ARApplicationWrapper(AAssetManager *assetManager)
    {
        auto platformServices = std::make_unique<AndroidPlatformServices>();
        arApplication_ = std::make_shared<arcore::ARApplication>(
            std::move(platformServices),
            assetManager);
    }

    static facebook::jni::local_ref<jhybriddata> initHybrid(
        facebook::jni::alias_ref<jclass>,
        facebook::jni::alias_ref<facebook::jni::JObject> assetManager)
    {
        JNIEnv *env = facebook::jni::Environment::current();
        AAssetManager *nativeAssetManager = AAssetManager_fromJava(env, assetManager.get());
        return makeCxxInstance(nativeAssetManager);
    }

    void onPauseNative()
    {
        if (arApplication_)
            arApplication_->OnPause();
    }

    void onResumeNative(
        facebook::jni::alias_ref<facebook::jni::JObject> context,
        facebook::jni::alias_ref<facebook::jni::JObject> activity,
        jboolean hasCameraPermission)
    {
        if (arApplication_)
        {
            JNIEnv *env = facebook::jni::Environment::current();
            arApplication_->OnResume(env, context.get(), activity.get(), hasCameraPermission);
        }
    }

    void onSurfaceCreated()
    {
        if (arApplication_)
        {
            JNIEnv *env = facebook::jni::Environment::current();
            arApplication_->OnSurfaceCreated(env);
        }
    }

    void onSurfaceChanged(jint width, jint height)
    {
        if (arApplication_)
            arApplication_->OnSurfaceChanged(width, height);
    }

    void onSurfaceDestroyed()
    {
        if (arApplication_)
            arApplication_->OnSurfaceDestroyed();
    }

    void onGlSurfaceCreated()
    {
        if (arApplication_)
            arApplication_->OnGlSurfaceCreated();
    }

    void onGlSurfaceChanged(jint width, jint height)
    {
        if (arApplication_)
            arApplication_->OnGlSurfaceChanged(width, height);
    }

    void onGlDrawFrame()
    {
        if (arApplication_)
            arApplication_->OnGlDrawFrame();
    }

    void onGestureTap(jfloat x, jfloat y)
    {
        if (arApplication_)
            arApplication_->OnGestureTap(x, y);
    }

    void setPaused(jboolean paused)
    {
        if (arApplication_)
            arApplication_->SetPaused(paused);
    }

    void setPlaneDetectionEnabled(jboolean enabled)
    {
        if (arApplication_)
            arApplication_->SetPlaneDetectionEnabled(enabled);
    }

    void setLightEstimationEnabled(jboolean enabled)
    {
        if (arApplication_)
            arApplication_->SetLightEstimationEnabled(enabled);
    }

    void setSessionType(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetSessionType(value->toStdString());
    }

    void setDepthMode(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetDepthMode(value->toStdString());
    }

    void setCloudAnchorMode(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetCloudAnchorMode(value->toStdString());
    }

    void setInstantPlacementMode(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetInstantPlacementMode(value->toStdString());
    }

    void setLightEstimationMode(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetLightEstimationMode(value->toStdString());
    }

    void setPlaneDetectionMode(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetPlaneDetectionMode(value->toStdString());
    }

    void setFocusMode(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetFocusMode(value->toStdString());
    }

    void setDebugShowPlanes(jboolean enabled)
    {
        if (arApplication_)
            arApplication_->SetDebugShowPlanes(enabled);
    }

    void setDebugShowPointCloud(jboolean enabled)
    {
        if (arApplication_)
            arApplication_->SetDebugShowPointCloud(enabled);
    }

    void setDebugShowWorldOrigin(jboolean enabled)
    {
        if (arApplication_)
            arApplication_->SetDebugShowWorldOrigin(enabled);
    }

    void setDebugShowDepthMap(jboolean enabled)
    {
        if (arApplication_)
            arApplication_->SetDebugShowDepthMap(enabled);
    }

    void setShaderMode(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetShaderMode(value->toStdString());
    }

    void setDisplayRotation(jint rotation)
    {
        if (arApplication_)
            arApplication_->SetDisplayRotation(rotation);
    }

    void setARObjects(
        facebook::jni::alias_ref<facebook::jni::JArrayClass<facebook::jni::JObject>> jDescs)
    {
        if (!arApplication_ || !jDescs)
            return;

        std::vector<arcore::ARApplication::ARObjectDesc> descs;
        auto size = jDescs->size();
        descs.reserve(size);

        auto cls = facebook::jni::findClassStatic("com/margelo/nitro/arcore/ARObjectDescriptor");
        auto getId = cls->getField<facebook::jni::JString>("id");
        auto getAnchorId = cls->getField<facebook::jni::JString>("anchorId");
        auto getModel = cls->getField<facebook::jni::JString>("model");

        for (size_t i = 0; i < size; ++i)
        {
            auto obj = jDescs->getElement(i);
            arcore::ARApplication::ARObjectDesc desc;
            desc.id = obj->getFieldValue(getId)->toStdString();
            desc.anchorId = obj->getFieldValue(getAnchorId)->toStdString();
            desc.model = obj->getFieldValue(getModel)->toStdString();
            descs.push_back(std::move(desc));
        }

        arApplication_->SetARObjects(std::move(descs));
    }

    facebook::jni::local_ref<facebook::jni::JString> createAnchor(jfloat x, jfloat y)
    {
        if (arApplication_)
        {
            std::string id = arApplication_->CreateAnchor(x, y);
            return facebook::jni::make_jstring(id);
        }
        return facebook::jni::make_jstring("");
    }

    void removeAnchor(facebook::jni::alias_ref<facebook::jni::JString> anchorId)
    {
        if (arApplication_)
            arApplication_->RemoveAnchor(anchorId->toStdString());
    }

    void onARViewMounted()
    {
        if (arApplication_)
            arApplication_->OnARViewMounted();
    }

    void onARViewUnmounted()
    {
        if (arApplication_)
            arApplication_->OnARViewUnmounted();
    }

    void destroySession()
    {
        if (arApplication_)
            arApplication_->DestroySession();
    }

    facebook::jni::local_ref<facebook::jni::JArrayClass<facebook::jni::JObject>> drainFaceEvents()
    {
        using namespace facebook::jni;

        if (!arApplication_)
        {
            auto cls = findClassStatic("com/margelo/nitro/arcore/FaceEvent");
            return JArrayClass<JObject>::newArray(0, cls);
        }

        auto events = arApplication_->DrainFaceEvents();
        auto cls = findClassStatic("com/margelo/nitro/arcore/FaceEvent");
        auto result = JArrayClass<JObject>::newArray(events.size(), cls);

        auto fromCpp = cls->getStaticMethod<local_ref<JObject>(jint, local_ref<JString>, local_ref<JArrayDouble>)>("fromCpp");

        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto &e = events[i];
            auto faceId = make_jstring(e.faceId);
            auto transform = JArrayDouble::newArray(16);
            JNIEnv *env = Environment::current();
            jdouble buf[16];
            for (int j = 0; j < 16; ++j)
                buf[j] = static_cast<jdouble>(e.transform[j]);
            env->SetDoubleArrayRegion(transform.get(), 0, 16, buf);

            auto obj = fromCpp(cls, static_cast<jint>(e.type), faceId, transform);
            (*result)[i] = obj;
        }
        return result;
    }

    static void registerNatives()
    {
        registerHybrid({
            makeNativeMethod(
                "initHybrid",
                "(Landroid/content/res/AssetManager;)Lcom/facebook/jni/HybridData;",
                ARApplicationWrapper::initHybrid),
            makeNativeMethod("onPauseNative", ARApplicationWrapper::onPauseNative),
            makeNativeMethod(
                "onResumeNative",
                "(Landroid/content/Context;Landroid/app/Activity;Z)V",
                ARApplicationWrapper::onResumeNative),
            makeNativeMethod("onSurfaceCreated", "()V", ARApplicationWrapper::onSurfaceCreated),
            makeNativeMethod("onSurfaceChanged", "(II)V", ARApplicationWrapper::onSurfaceChanged),
            makeNativeMethod("onSurfaceDestroyed", "()V", ARApplicationWrapper::onSurfaceDestroyed),
            makeNativeMethod("onGlSurfaceCreated", "()V", ARApplicationWrapper::onGlSurfaceCreated),
            makeNativeMethod("onGlSurfaceChanged", "(II)V", ARApplicationWrapper::onGlSurfaceChanged),
            makeNativeMethod("onGlDrawFrame", "()V", ARApplicationWrapper::onGlDrawFrame),
            makeNativeMethod("onGestureTap", "(FF)V", ARApplicationWrapper::onGestureTap),
            makeNativeMethod("setPaused", "(Z)V", ARApplicationWrapper::setPaused),
            makeNativeMethod("setPlaneDetectionEnabled", "(Z)V", ARApplicationWrapper::setPlaneDetectionEnabled),
            makeNativeMethod("setLightEstimationEnabled", "(Z)V", ARApplicationWrapper::setLightEstimationEnabled),
            makeNativeMethod("setSessionType", "(Ljava/lang/String;)V", ARApplicationWrapper::setSessionType),
            makeNativeMethod("setDepthMode", "(Ljava/lang/String;)V", ARApplicationWrapper::setDepthMode),
            makeNativeMethod("setCloudAnchorMode", "(Ljava/lang/String;)V", ARApplicationWrapper::setCloudAnchorMode),
            makeNativeMethod("setInstantPlacementMode", "(Ljava/lang/String;)V", ARApplicationWrapper::setInstantPlacementMode),
            makeNativeMethod("setLightEstimationMode", "(Ljava/lang/String;)V", ARApplicationWrapper::setLightEstimationMode),
            makeNativeMethod("setPlaneDetectionMode", "(Ljava/lang/String;)V", ARApplicationWrapper::setPlaneDetectionMode),
            makeNativeMethod("setFocusMode", "(Ljava/lang/String;)V", ARApplicationWrapper::setFocusMode),
            makeNativeMethod("setDebugShowPlanes", "(Z)V", ARApplicationWrapper::setDebugShowPlanes),
            makeNativeMethod("setDebugShowPointCloud", "(Z)V", ARApplicationWrapper::setDebugShowPointCloud),
            makeNativeMethod("setDebugShowWorldOrigin", "(Z)V", ARApplicationWrapper::setDebugShowWorldOrigin),
            makeNativeMethod("setDebugShowDepthMap", "(Z)V", ARApplicationWrapper::setDebugShowDepthMap),
            makeNativeMethod("setShaderMode", "(Ljava/lang/String;)V", ARApplicationWrapper::setShaderMode),
            makeNativeMethod("setDisplayRotation", "(I)V", ARApplicationWrapper::setDisplayRotation),
            makeNativeMethod("onARViewMounted", "()V", ARApplicationWrapper::onARViewMounted),
            makeNativeMethod("onARViewUnmounted", "()V", ARApplicationWrapper::onARViewUnmounted),
            makeNativeMethod("destroySession", "()V", ARApplicationWrapper::destroySession),
            makeNativeMethod("setARObjects", "([Lcom/margelo/nitro/arcore/ARObjectDescriptor;)V", ARApplicationWrapper::setARObjects),
            makeNativeMethod("createAnchor", "(FF)Ljava/lang/String;", ARApplicationWrapper::createAnchor),
            makeNativeMethod("removeAnchor", "(Ljava/lang/String;)V", ARApplicationWrapper::removeAnchor),
            makeNativeMethod("drainFaceEvents", "()[Lcom/margelo/nitro/arcore/FaceEvent;", ARApplicationWrapper::drainFaceEvents),
        });
    }
};
