package com.margelo.nitro.arcore;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

public class CurrentActivityTracker implements Application.ActivityLifecycleCallbacks {
    private static final CurrentActivityTracker INSTANCE = new CurrentActivityTracker();
    private static final WeakHashMap<Activity, Boolean> hosts = new WeakHashMap<>();
    private static WeakReference<Activity> current = new WeakReference<>(null);
    private static boolean registered;

    public static synchronized void register(Application app) {
        if (registered) return;
        app.registerActivityLifecycleCallbacks(INSTANCE);
        registered = true;
    }

    public static Activity getCurrentActivity() { return current.get(); }

    public static void observeHost(Activity activity, long runtimeId) {
        ARCoreLifecycle.assertMainThread();
        hosts.put(activity, true);
        current = new WeakReference<>(activity);
        ARCoreLifecycle.hostResumedNative(runtimeId, activity.getTaskId(), activity);
    }

    public static void pauseHost(Activity activity) {
        ARCoreLifecycle.assertMainThread();
        if (!hosts.containsKey(activity)) return;
        if (current.get() == activity) current.clear();
        ARCoreLifecycle.hostPausedNative(activity.getTaskId(), activity);
    }

    public static void destroyHost(Activity activity) {
        ARCoreLifecycle.assertMainThread();
        if (hosts.remove(activity) == null) return;
        if (current.get() == activity) current.clear();
        ARCoreLifecycle.hostDestroyedNative(activity.getTaskId(), activity,
            activity.isChangingConfigurations(), activity.isFinishing());
    }

    @Override public void onActivityResumed(Activity activity) {
        if (hosts.containsKey(activity)) observeHost(activity, 0);
    }
    @Override public void onActivityPaused(Activity activity) { pauseHost(activity); }
    @Override public void onActivityDestroyed(Activity activity) { destroyHost(activity); }
    @Override public void onActivityCreated(Activity activity, Bundle state) {}
    @Override public void onActivityStarted(Activity activity) {}
    @Override public void onActivityStopped(Activity activity) {}
    @Override public void onActivitySaveInstanceState(Activity activity, Bundle state) {}
}
