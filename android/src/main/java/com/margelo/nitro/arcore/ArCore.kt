package com.margelo.nitro.arcore
  
import com.facebook.proguard.annotations.DoNotStrip

@DoNotStrip
class ArCore : HybridArCoreSpec() {
  override fun multiply(a: Double, b: Double): Double {
    return a * b
  }
}
