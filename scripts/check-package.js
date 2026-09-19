const { execFileSync } = require('node:child_process');
const path = require('node:path');
const pkg = require('../package.json');

function entryPoints(value) {
  if (typeof value === 'string') return [value.replace(/^\.\//, '')];
  return Object.values(value ?? {}).flatMap(entryPoints);
}

const required = new Set([
  ...entryPoints(pkg.main),
  ...entryPoints(pkg.types),
  ...entryPoints(pkg.exports),
  'package.json',
  'nitro.json',
  'ArCore.podspec',
  'android/build.gradle',
  'android/CMakeLists.txt',
  'android/src/main/AndroidManifest.xml',
  'android/src/main/cpp/cpp-adapter.cpp',
  'android/src/main/java/com/margelo/nitro/arcore/HybridARView.kt',
  'ios/HybridARView.swift',
  'cpp/HybridCrossPlatformArCore.hpp',
  'cpp/HybridCrossPlatformArCore.cpp',
  'nitrogen/generated/android/arcore+autolinking.cmake',
  'nitrogen/generated/android/arcore+autolinking.gradle',
  'nitrogen/generated/android/kotlin/com/margelo/nitro/arcore/HybridARViewHybridSpec.kt',
  'nitrogen/generated/ios/ArCore+autolinking.rb',
  'nitrogen/generated/ios/swift/HybridARViewHybridSpec.swift',
  'nitrogen/generated/shared/c++/HybridARViewHybridSpec.hpp',
  'libraries/include/arcore_c_api.h',
  'third_party/glm/glm.hpp',
  'third_party/glm/detail/type_vec3.inl',
  'third_party/glm/LICENSE',
  'ios/Assets/models/andy.usdz',
  'android/src/main/assets/models/andy.obj',
  'android/src/main/assets/models/andy.png',
  'android/src/main/assets/models/depth_color_palette.png',
  'android/src/main/assets/shaders/ar_object.vert',
  'android/src/main/assets/shaders/ar_object.frag',
]);

function checkPackage(files) {
  const paths = new Set(files.map((file) => file.path));
  const missing = [...required].filter((file) => !paths.has(file));
  const unexpected = [...paths].filter(
    (file) =>
      /^(example|docs|scripts)\//.test(file) ||
      /(^|\/)(node_modules|build|DerivedData|coverage|__tests__|__fixtures__|__mocks__|\.[^/]+)(\/|$)/.test(
        file
      ) ||
      /^android\/(gradle\/|gradlew(?:\.bat)?$)/.test(file) ||
      /(^|\/)(local\.properties|Podfile\.lock)$/.test(file) ||
      /\.(tgz|apk|aab|ipa|xcuserstate)$/.test(file)
  );
  return { missing, unexpected };
}

if (require.main === module) {
  const output = execFileSync(
    process.platform === 'win32' ? 'npm.cmd' : 'npm',
    // Older npm versions may still run prepare; keep lifecycle output out of JSON.
    [
      'pack',
      '--dry-run',
      '--json',
      '--ignore-scripts',
      '--foreground-scripts=false',
    ],
    { cwd: path.resolve(__dirname, '..'), encoding: 'utf8' }
  );
  const [manifest] = JSON.parse(output);
  const { missing, unexpected } = checkPackage(manifest.files);
  for (const [label, files] of Object.entries({ missing, unexpected })) {
    if (files.length) {
      console.error(
        `Package ${label} files:\n${files
          .map((file) => `  ${file}`)
          .join('\n')}`
      );
    }
  }
  if (missing.length || unexpected.length) process.exitCode = 1;
  else
    console.log(`Package contents verified (${manifest.files.length} files).`);
}

module.exports = { checkPackage };
