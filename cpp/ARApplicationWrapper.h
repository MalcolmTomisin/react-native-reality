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

    void setCameraFacing(facebook::jni::alias_ref<facebook::jni::JString> value)
    {
        if (arApplication_)
            arApplication_->SetCameraFacing(value->toStdString());
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

        JNIEnv *env = facebook::jni::Environment::current();
        jobjectArray rawArray = (jobjectArray)jDescs.get();

        jclass cls = env->FindClass("com/margelo/nitro/arcore/ARObjectDescriptor");
        jfieldID fId = env->GetFieldID(cls, "id", "Ljava/lang/String;");
        jfieldID fAnchorId = env->GetFieldID(cls, "anchorId", "Ljava/lang/String;");
        jfieldID fModel = env->GetFieldID(cls, "model", "Ljava/lang/String;");

        jsize size = env->GetArrayLength(rawArray);
        std::vector<arcore::ARApplication::ARObjectDesc> descs;
        descs.reserve(size);

        for (jsize i = 0; i < size; ++i)
        {
            env->PushLocalFrame(8);

            jobject obj = env->GetObjectArrayElement(rawArray, i);
            arcore::ARApplication::ARObjectDesc desc;

            auto jId = (jstring)env->GetObjectField(obj, fId);
            const char *c = env->GetStringUTFChars(jId, nullptr);
            desc.id = c;
            env->ReleaseStringUTFChars(jId, c);

            auto jAnchor = (jstring)env->GetObjectField(obj, fAnchorId);
            c = env->GetStringUTFChars(jAnchor, nullptr);
            desc.anchorId = c;
            env->ReleaseStringUTFChars(jAnchor, c);

            auto jModel = (jstring)env->GetObjectField(obj, fModel);
            c = env->GetStringUTFChars(jModel, nullptr);
            desc.model = c;
            env->ReleaseStringUTFChars(jModel, c);

            descs.push_back(std::move(desc));
            env->PopLocalFrame(nullptr);
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

    void setFaceFilters(
        facebook::jni::alias_ref<facebook::jni::JArrayClass<facebook::jni::JObject>> jDescs)
    {
        if (!arApplication_ || !jDescs)
            return;

        JNIEnv *env = facebook::jni::Environment::current();
        jobjectArray rawArray = (jobjectArray)jDescs.get();

        jclass descCls = env->FindClass("com/margelo/nitro/arcore/ARFaceFilterDescriptor");
        jfieldID fId = env->GetFieldID(descCls, "id", "Ljava/lang/String;");
        jfieldID fAttach = env->GetFieldID(descCls, "attachmentPoint", "Ljava/lang/String;");
        jfieldID fModel = env->GetFieldID(descCls, "model", "Ljava/lang/String;");
        jfieldID fScale = env->GetFieldID(descCls, "scale", "Lcom/margelo/nitro/arcore/ARVector3;");
        jfieldID fOffset = env->GetFieldID(descCls, "offset", "Lcom/margelo/nitro/arcore/ARVector3;");
        jfieldID fRotation = env->GetFieldID(descCls, "rotation", "Lcom/margelo/nitro/arcore/ARVector3;");
        jfieldID fVisible = env->GetFieldID(descCls, "visible", "Ljava/lang/Boolean;");

        jclass vec3Cls = env->FindClass("com/margelo/nitro/arcore/ARVector3");
        jfieldID fX = env->GetFieldID(vec3Cls, "x", "D");
        jfieldID fY = env->GetFieldID(vec3Cls, "y", "D");
        jfieldID fZ = env->GetFieldID(vec3Cls, "z", "D");

        jclass boolCls = env->FindClass("java/lang/Boolean");
        jmethodID boolValue = env->GetMethodID(boolCls, "booleanValue", "()Z");

        jsize size = env->GetArrayLength(rawArray);
        std::vector<arcore::ARApplication::FaceFilterDesc> descs;
        descs.reserve(size);

        for (jsize i = 0; i < size; ++i)
        {
            env->PushLocalFrame(16);

            jobject obj = env->GetObjectArrayElement(rawArray, i);
            arcore::ARApplication::FaceFilterDesc desc;

            auto jId = (jstring)env->GetObjectField(obj, fId);
            const char *idChars = env->GetStringUTFChars(jId, nullptr);
            desc.id = idChars;
            env->ReleaseStringUTFChars(jId, idChars);

            auto jAttach = (jstring)env->GetObjectField(obj, fAttach);
            const char *attachChars = env->GetStringUTFChars(jAttach, nullptr);
            desc.attachmentPoint = attachChars;
            env->ReleaseStringUTFChars(jAttach, attachChars);

            auto jModel = (jstring)env->GetObjectField(obj, fModel);
            const char *modelChars = env->GetStringUTFChars(jModel, nullptr);
            desc.model = modelChars;
            env->ReleaseStringUTFChars(jModel, modelChars);

            jobject visibleObj = env->GetObjectField(obj, fVisible);
            if (visibleObj)
                desc.visible = env->CallBooleanMethod(visibleObj, boolValue);

            jobject scaleObj = env->GetObjectField(obj, fScale);
            if (scaleObj)
            {
                desc.scale[0] = static_cast<float>(env->GetDoubleField(scaleObj, fX));
                desc.scale[1] = static_cast<float>(env->GetDoubleField(scaleObj, fY));
                desc.scale[2] = static_cast<float>(env->GetDoubleField(scaleObj, fZ));
            }

            jobject offsetObj = env->GetObjectField(obj, fOffset);
            if (offsetObj)
            {
                desc.offset[0] = static_cast<float>(env->GetDoubleField(offsetObj, fX));
                desc.offset[1] = static_cast<float>(env->GetDoubleField(offsetObj, fY));
                desc.offset[2] = static_cast<float>(env->GetDoubleField(offsetObj, fZ));
            }

            jobject rotObj = env->GetObjectField(obj, fRotation);
            if (rotObj)
            {
                desc.rotation[0] = static_cast<float>(env->GetDoubleField(rotObj, fX));
                desc.rotation[1] = static_cast<float>(env->GetDoubleField(rotObj, fY));
                desc.rotation[2] = static_cast<float>(env->GetDoubleField(rotObj, fZ));
            }

            descs.push_back(std::move(desc));
            env->PopLocalFrame(nullptr);
        }

        arApplication_->SetFaceFilters(std::move(descs));
    }

    void destroySession()
    {
        if (arApplication_)
            arApplication_->DestroySession();
    }

    jobjectArray drainEvents()
    {
        using namespace facebook::jni;
        JNIEnv *env = Environment::current();

        jclass cls = env->FindClass("com/margelo/nitro/arcore/AREvent");

        if (!arApplication_)
            return env->NewObjectArray(0, cls, nullptr);

        auto events = arApplication_->DrainEvents();
        if (events.empty())
            return env->NewObjectArray(0, cls, nullptr);

        jmethodID fromCpp = env->GetStaticMethodID(cls, "fromCpp",
            "(ILjava/lang/String;Ljava/lang/String;[D[D)Lcom/margelo/nitro/arcore/AREvent;");

        jobjectArray result = env->NewObjectArray(
            static_cast<jsize>(events.size()), cls, nullptr);

        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto &e = events[i];
            jstring idA = env->NewStringUTF(e.idA.c_str());
            jstring strB = env->NewStringUTF(e.strB.c_str());

            jdoubleArray transform = env->NewDoubleArray(16);
            env->SetDoubleArrayRegion(transform, 0, 16, e.transform);

            jdoubleArray scalars = env->NewDoubleArray(8);
            env->SetDoubleArrayRegion(scalars, 0, 8, e.scalars);

            jobject obj = env->CallStaticObjectMethod(cls, fromCpp,
                static_cast<jint>(e.category), idA, strB, transform, scalars);
            env->SetObjectArrayElement(result, static_cast<jsize>(i), obj);

            env->DeleteLocalRef(idA);
            env->DeleteLocalRef(strB);
            env->DeleteLocalRef(transform);
            env->DeleteLocalRef(scalars);
            env->DeleteLocalRef(obj);
        }

        return result;
    }

    jboolean isSessionInitialized()
    {
        return arApplication_ && arApplication_->IsSessionInitialized() ? JNI_TRUE : JNI_FALSE;
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
            makeNativeMethod("setCameraFacing", "(Ljava/lang/String;)V", ARApplicationWrapper::setCameraFacing),
            makeNativeMethod("setShaderMode", "(Ljava/lang/String;)V", ARApplicationWrapper::setShaderMode),
            makeNativeMethod("setDisplayRotation", "(I)V", ARApplicationWrapper::setDisplayRotation),
            makeNativeMethod("onARViewMounted", "()V", ARApplicationWrapper::onARViewMounted),
            makeNativeMethod("onARViewUnmounted", "()V", ARApplicationWrapper::onARViewUnmounted),
            makeNativeMethod("destroySession", "()V", ARApplicationWrapper::destroySession),
            makeNativeMethod("setARObjects", "([Lcom/margelo/nitro/arcore/ARObjectDescriptor;)V", ARApplicationWrapper::setARObjects),
            makeNativeMethod("createAnchor", "(FF)Ljava/lang/String;", ARApplicationWrapper::createAnchor),
            makeNativeMethod("removeAnchor", "(Ljava/lang/String;)V", ARApplicationWrapper::removeAnchor),
            makeNativeMethod("drainEvents", "()[Lcom/margelo/nitro/arcore/AREvent;", ARApplicationWrapper::drainEvents),
            makeNativeMethod("isSessionInitialized", "()Z", ARApplicationWrapper::isSessionInitialized),
            makeNativeMethod("setFaceFilters", "([Lcom/margelo/nitro/arcore/ARFaceFilterDescriptor;)V", ARApplicationWrapper::setFaceFilters),
        });
    }
};
