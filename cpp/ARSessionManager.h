#pragma once

#include <fbjni/fbjni.h>
#include "arcore_c_api.h"

// Simple singleton-style manager for maintaining a single ArSession instance.
// Provides thin wrappers around the ARCore C API: ArSession_create,
// ArSession_pause, and ArSession_destroy.
class ArSessionManager {
 public:
  // Returns the global ArSessionManager instance.
  static ArSessionManager& Instance() {
    static ArSessionManager instance;
    return instance;
  }

  // Disallow copy and move.
  ArSessionManager(const ArSessionManager&) = delete;
  ArSessionManager& operator=(const ArSessionManager&) = delete;

  // Create the ArSession if not already created. Returns AR status.
  // Compatible with ArSession_create.
  ArStatus Create(JNIEnv* env, jobject context) {
    if (ar_session_ != nullptr) {
      return AR_SUCCESS;  // Already created
    }
    return ArSession_create(env, context, &ar_session_);
  }

  // Pause the session if present. Compatible with ArSession_pause.
  ArStatus Pause() {
    if (ar_session_ != nullptr) {
      return ArSession_pause(ar_session_);
    }
    return AR_SUCCESS;
  }

  // Resume the session if present. Compatible with ArSession_resume.
  ArStatus Resume() {
    if (ar_session_ != nullptr) {
      return ArSession_resume(ar_session_);
    }
    return AR_SUCCESS;
  }

  // Destroy the session if present. Compatible with ArSession_destroy.
  void Destroy() {
    if (ar_session_ != nullptr) {
      ArSession_destroy(ar_session_);
      ar_session_ = nullptr;
    }
  }

  // Accessor for the underlying ArSession pointer.
  ArSession* Get() const { return ar_session_; }

  // Whether a session has been created.
  bool IsInitialized() const { return ar_session_ != nullptr; }

  void SetInstallRequested(bool requested) {
    install_requested_ = requested;
  }

  bool IsInstallRequested() const {
    return install_requested_;
  }

 private:
  ArSessionManager() = default;
  ~ArSessionManager() = default;

  ArSession* ar_session_ = nullptr;
  bool install_requested_ = false;
};
