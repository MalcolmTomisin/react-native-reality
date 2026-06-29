package com.margelo.nitro.arcore;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import java.lang.ref.WeakReference;

public class CurrentActivityTracker implements Application.ActivityLifecycleCallbacks {
    private static WeakReference<Activity> mCurrentActivity = new WeakReference<>(null);

    public static void register(Application app) {
        app.registerActivityLifecycleCallbacks(new CurrentActivityTracker());
    }

    public static Activity getCurrentActivity() {
        return mCurrentActivity.get();
    }

    @Override
    public void onActivityResumed(Activity activity) {
        mCurrentActivity = new WeakReference<>(activity);
    }

    @Override public void onActivityPaused(Activity activity) {}
    @Override public void onActivityCreated(Activity a, Bundle b) {}
    @Override public void onActivityStarted(Activity a) {}
    @Override public void onActivityStopped(Activity a) {}
    @Override public void onActivitySaveInstanceState(Activity a, Bundle b) {}
    @Override public void onActivityDestroyed(Activity a) {}
}
