#include <cstdint>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "AndroidPlatformServices.h"
#include "ARCoreAvailability.h"
#include "ARSessionManager.h"

namespace {
facebook::jni::local_ref<facebook::jni::JObject> getApplicationContext()
{
    using namespace facebook::jni;
    static const auto nitro = findClassStatic("com/margelo/nitro/NitroModules");
    static const auto getContext = nitro->getStaticMethod<local_ref<JObject>()>(
        "getApplicationContext", "()Lcom/facebook/react/bridge/ReactApplicationContext;");
    auto context = getContext(nitro);
    if (!context)
        throw std::runtime_error("AR initialization context is unavailable");
    return context;
}
}

bool arcore::checkARCoreAvailability()
{
    using namespace facebook::jni;
    using namespace std::chrono_literals;
    bool available = false;
    ThreadScope::WithClassLoader([&] {
        auto context = getApplicationContext();

        auto* env = Environment::current();
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (true)
        {
            ArAvailability availability;
            ArCoreApk_checkAvailability(env, context.get(), &availability);
            throwPendingJniExceptionAsCppException();
            switch (availability)
            {
            case AR_AVAILABILITY_SUPPORTED_INSTALLED:
                available = true;
                return;
            case AR_AVAILABILITY_UNSUPPORTED_DEVICE_NOT_CAPABLE:
            case AR_AVAILABILITY_SUPPORTED_NOT_INSTALLED:
            case AR_AVAILABILITY_SUPPORTED_APK_TOO_OLD:
                return;
            case AR_AVAILABILITY_UNKNOWN_CHECKING:
                if (std::chrono::steady_clock::now() >= deadline)
                    throw std::runtime_error("Timed out checking ARCore availability");
                std::this_thread::sleep_for(200ms);
                break;
            case AR_AVAILABILITY_UNKNOWN_TIMED_OUT:
                throw std::runtime_error("Timed out checking ARCore availability");
            default:
                throw std::runtime_error("Could not determine ARCore availability");
            }
        }
    });
    return available;
}

bool AndroidPlatformServices::isGooglePlayServicesAvailable()
{
    if (ArSessionManager::Instance().IsInitialized())
    {
        return true;
    }
    using namespace facebook::jni;
    try
    {
        auto context = getApplicationContext();
        static const auto tracker = findClassStatic("com/margelo/nitro/arcore/CurrentActivityTracker");
        static const auto getActivity = tracker->getStaticMethod<local_ref<JObject>()>(
            "getCurrentActivity", "()Landroid/app/Activity;");
        auto activity = getActivity(tracker);
        if (!activity)
            return false;
        bool available = checkARCoreInstallation(Environment::current(), context.get(), activity.get());
        throwPendingJniExceptionAsCppException();
        return available;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AndroidPlatformServices::checkARCoreInstallation(JNIEnv *env, jobject context, jobject activity)
{
    ArInstallStatus install_status;
    bool user_requested_install = !ArSessionManager::Instance().IsInstallRequested();
    ArAvailability availability;

    ArCoreApk_checkAvailability(env, context, &availability);

    switch (availability)
    {
    case AR_AVAILABILITY_SUPPORTED_INSTALLED:
    case AR_AVAILABILITY_SUPPORTED_NOT_INSTALLED:
    case AR_AVAILABILITY_SUPPORTED_APK_TOO_OLD:
        break;
    case AR_AVAILABILITY_UNKNOWN_CHECKING:
        return false;
    default:
        return false;
    }

    ArStatus error = ArCoreApk_requestInstall(env, activity, user_requested_install, &install_status);
    if (error != AR_SUCCESS)
    {
        return false;
    }

    switch (install_status)
    {
    case AR_INSTALL_STATUS_INSTALLED:
        return true;
    case AR_INSTALL_STATUS_INSTALL_REQUESTED:
        ArSessionManager::Instance().SetInstallRequested(true);
        return false;
    }

    return false;
}