package com.margelo.nitro.arcore

import android.view.View
import com.facebook.react.ReactPackage
import com.facebook.react.bridge.NativeModule
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.uimanager.ViewManager
import com.margelo.nitro.arcore.views.HybridARViewHybridManager

class ArCorePackage : ReactPackage {
    override fun createNativeModules(reactContext: ReactApplicationContext): List<NativeModule> {
        return emptyList()
    }

    override fun createViewManagers(reactContext: ReactApplicationContext): List<ViewManager<out View, *>> {
        return listOf(HybridARViewHybridManager())
    }

    companion object {
        init {
            System.loadLibrary("arcore")
        }
    }
}
