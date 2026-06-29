project(arcore)
cmake_minimum_required(VERSION 3.9.0)

set(PACKAGE_NAME arcore)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 20)

# Define C++ library and add all sources
add_library(${PACKAGE_NAME} SHARED
        src/main/cpp/cpp-adapter.cpp
        ../cpp/HybridCrossPlatformArCore.cpp
        ../cpp/BackgroundRenderer.cpp
        ../cpp/PlaneRenderer.cpp
        ../cpp/ObjRenderer.cpp
        ../cpp/Texture.cpp
        ../cpp/utils.cpp
        ../cpp/AndroidPlatformServices.cpp
        ../cpp/ARSessionManager.cpp
)

# Add Nitrogen specs
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/arcore+autolinking.cmake)

# Set up local includes
include_directories(
        "src/main/cpp"
        "../cpp"
        "../third_party"
        "../third_party/glm"
        "../libraries/include"
)

find_library(LOG_LIB log)

# Import the ARCore native library
add_library(arcore_sdk SHARED IMPORTED)
set_target_properties(arcore_sdk PROPERTIES
        IMPORTED_LOCATION ${ARCORE_LIBPATH}/${ANDROID_ABI}/libarcore_sdk_c.so
        INTERFACE_INCLUDE_DIRECTORIES ${ARCORE_INCLUDE}
)

# Link all libraries together
target_link_libraries(
        ${PACKAGE_NAME}
        ${LOG_LIB}
        android
        GLESv2
        GLESv3
        EGL
        arcore_sdk
)
