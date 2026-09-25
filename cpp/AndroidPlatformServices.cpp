#include "AndroidPlatformServices.h"
#include "ARCoreAvailability.h"
#include "ARCoreInitialization.h"
#include <fbjni/NativeRunnable.h>
#include <chrono>
#include <map>
#include <stdexcept>

using namespace facebook::jni;

namespace {
struct Lifecycle : JavaClass<Lifecycle> {
    static constexpr auto kJavaDescriptor = "Lcom/margelo/nitro/arcore/ARCoreLifecycle;";
};

class MainScheduler final : public arcore::IInitializationScheduler {
public:
    void assertMainThread() const override {
        static const auto method = Lifecycle::javaClassStatic()->getStaticMethod<void()>("assertMainThread");
        method(Lifecycle::javaClassStatic());
    }
    void dispatch(std::function<void()> action) override {
        ThreadScope::WithClassLoader([&] {
            post(JNativeRunnable::newObjectCxxArgs(std::move(action)), 0);
        });
    }
    Token schedule(int64_t delayMs, std::function<void()> action) override {
        assertMainThread();
        const auto token = ++nextToken_;
        auto runnable = JNativeRunnable::newObjectCxxArgs([this, token, action = std::move(action)] {
            if (!scheduled_.erase(token)) return;
            action();
        });
        scheduled_.emplace(token, make_global(runnable));
        post(runnable, delayMs);
        return token;
    }
    void cancel(Token token) override {
        assertMainThread();
        auto found = scheduled_.find(token);
        if (found == scheduled_.end()) return;
        static const auto method = Lifecycle::javaClassStatic()->getStaticMethod<void(alias_ref<JRunnable>)>("cancel");
        method(Lifecycle::javaClassStatic(), found->second);
        scheduled_.erase(found);
    }
    int64_t now() const override {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
private:
    static void post(alias_ref<JRunnable> runnable, int64_t delayMs) {
        static const auto method = Lifecycle::javaClassStatic()->getStaticMethod<void(alias_ref<JRunnable>, jlong)>("post");
        method(Lifecycle::javaClassStatic(), runnable, delayMs);
    }
    Token nextToken_ = 0;
    std::map<Token, global_ref<JRunnable>> scheduled_;
};

MainScheduler& scheduler() {
    static MainScheduler instance;
    return instance;
}

struct Host {
    weak_ref<JObject> activity;
    bool resumed = false;
};
std::map<int, Host> hosts;
global_ref<JObject> applicationContext;

void notifyViews(int task, bool ready, const std::string& error) {
    static const auto method = Lifecycle::javaClassStatic()->getStaticMethod<void(jint, jboolean, const std::string&)>("notifyViews");
    method(Lifecycle::javaClassStatic(), task, ready, error);
}

arcore::ARCoreInitialization& coordinator() {
    static AndroidPlatformServices platform;
    static arcore::ARCoreInitialization instance(platform, scheduler(), notifyViews);
    return instance;
}

bool isCurrentHost(int task, alias_ref<JObject> activity) {
    const auto found = hosts.find(task);
    if (found == hosts.end()) return false;
    auto current = found->second.activity.lockLocal();
    return current && Environment::current()->IsSameObject(current.get(), activity.get());
}

void registerRuntime(alias_ref<jclass>, jlong runtime, alias_ref<JObject> context) {
    scheduler().assertMainThread();
    applicationContext = make_global(context);
    coordinator().registerRuntime(runtime);
}
void removeRuntime(alias_ref<jclass>, jlong runtime) {
    coordinator().removeRuntime(runtime);
}
void hostResumed(alias_ref<jclass>, jlong runtime, jint task, alias_ref<JObject> activity) {
    scheduler().assertMainThread();
    hosts[task] = {make_weak(activity), true};
    coordinator().hostResumed(runtime, task);
}
void hostPaused(alias_ref<jclass>, jint task, alias_ref<JObject> activity) {
    scheduler().assertMainThread();
    if (!isCurrentHost(task, activity)) return;
    hosts.at(task).resumed = false;
    coordinator().hostPaused(task);
}
void hostDestroyed(alias_ref<jclass>, jint task, alias_ref<JObject> activity,
                   jboolean changing, jboolean finishing) {
    scheduler().assertMainThread();
    if (!isCurrentHost(task, activity)) return;
    hosts.erase(task);
    coordinator().hostDestroyed(task, changing, finishing);
}
jboolean isPending(alias_ref<jclass>) { return coordinator().pending(); }

int taskId(alias_ref<JObject> activity) {
    const auto method = activity->getClass()->getMethod<jint()>("getTaskId");
    return method(activity);
}
}

namespace arcore {
void registerARCoreInitializationNatives() {
    Lifecycle::javaClassStatic()->registerNatives({
        makeNativeMethod("registerRuntimeNative", "(JLandroid/content/Context;)V", registerRuntime),
        makeNativeMethod("removeRuntimeNative", removeRuntime),
        makeNativeMethod("hostResumedNative", "(JILandroid/app/Activity;)V", hostResumed),
        makeNativeMethod("hostPausedNative", "(ILandroid/app/Activity;)V", hostPaused),
        makeNativeMethod("hostDestroyedNative", "(ILandroid/app/Activity;ZZ)V", hostDestroyed),
        makeNativeMethod("isInitializationPending", isPending),
    });
}

std::shared_ptr<margelo::nitro::Promise<bool>> initializeARCore(int64_t runtimeId) {
    auto promise = margelo::nitro::Promise<bool>::create();
    try {
        scheduler().dispatch([runtimeId, promise] {
            try {
                coordinator().initialize(runtimeId, [promise](bool ready, const std::string& error) {
                    if (error.empty()) promise->resolve(ready);
                    else promise->reject(std::make_exception_ptr(std::runtime_error(error)));
                });
            } catch (...) {
                if (promise->isPending()) promise->reject(std::current_exception());
            }
        });
    } catch (...) {
        promise->reject(std::current_exception());
    }
    return promise;
}
}

ArAvailability AndroidPlatformServices::checkAvailability() {
    scheduler().assertMainThread();
    if (!applicationContext) throw std::runtime_error("AR initialization context is unavailable");
    ArAvailability availability;
    ArCoreApk_checkAvailability(Environment::current(), applicationContext.get(), &availability);
    throwPendingJniExceptionAsCppException();
    return availability;
}

ARCoreInstallResult AndroidPlatformServices::requestInstall(int task, bool userRequested) {
    scheduler().assertMainThread();
    if (!hasResumedHost(task)) throw std::runtime_error("AR initialization requires a resumed host Activity");
    auto activity = hosts.at(task).activity.lockLocal();
    ARCoreInstallResult result{AR_ERROR_FATAL, AR_INSTALL_STATUS_INSTALL_REQUESTED};
    result.status = ArCoreApk_requestInstall(Environment::current(), activity.get(), userRequested, &result.installation);
    throwPendingJniExceptionAsCppException();
    return result;
}

bool AndroidPlatformServices::hasResumedHost(int task) {
    scheduler().assertMainThread();
    auto found = hosts.find(task);
    return found != hosts.end() && found->second.resumed && bool(found->second.activity.lockLocal());
}

bool AndroidPlatformServices::isGooglePlayServicesAvailable() {
    scheduler().assertMainThread();
    for (const auto& [task, host] : hosts)
        if (hasResumedHost(task)) return coordinator().ensureForSession(task);
    return false;
}

bool AndroidPlatformServices::checkARCoreInstallation(JNIEnv*, jobject, jobject activity) {
    scheduler().assertMainThread();
    return coordinator().ensureForSession(taskId(wrap_alias(activity)));
}
