/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

precision highp float;

uniform sampler2D u_Texture;

uniform vec4 u_LightingParameters;
uniform vec4 u_MaterialParameters;
uniform vec4 u_ColorCorrectionParameters;

// Environmental HDR lighting (ARCore world space). When u_UseHdrLighting is 1
// the HDR path is used instead of the ambient-intensity color-correction path.
uniform int u_UseHdrLighting;
uniform mat4 u_ViewInverse;
uniform vec3 u_MainLightDirection;
uniform vec3 u_MainLightIntensity;
uniform vec3 u_SphericalHarmonics[9];

#if USE_DEPTH_FOR_OCCLUSION
uniform sampler2D u_DepthTexture;
uniform mat3 u_DepthUvTransform;
uniform float u_DepthAspectRatio;
#endif // USE_DEPTH_FOR_OCCLUSION

varying vec3 v_ViewPosition;
varying vec3 v_ViewNormal;
varying vec2 v_TexCoord;
varying vec3 v_ScreenSpacePosition;
uniform vec4 u_ObjColor;

#if USE_DEPTH_FOR_OCCLUSION

float DepthGetMillimeters(in sampler2D depth_texture, in vec2 depth_uv) {
  // Depth is packed into the red and green components of its texture.
  // The texture is a normalized format, storing millimeters.
  vec3 packedDepthAndVisibility = texture2D(depth_texture, depth_uv).xyz;
  return dot(packedDepthAndVisibility.xy, vec2(255.0, 256.0 * 255.0));
}

// Returns linear interpolation position of value between min and max bounds.
// E.g., DepthInverseLerp(1100, 1000, 2000) returns 0.1.
float DepthInverseLerp(in float value, in float min_bound, in float max_bound) {
  return clamp((value - min_bound) / (max_bound - min_bound), 0.0, 1.0);
}

// Returns a value between 0.0 (not visible) and 1.0 (completely visible)
// Which represents how visible or occluded is the pixel in relation to the
// depth map.
float DepthGetVisibility(in sampler2D depth_texture, in vec2 depth_uv,
                         in float asset_depth_mm) {
  float depth_mm = DepthGetMillimeters(depth_texture, depth_uv);

  // Instead of a hard z-buffer test, allow the asset to fade into the
  // background along a 2 * kDepthTolerancePerMm * asset_depth_mm
  // range centered on the background depth.
  const float kDepthTolerancePerMm = 0.015;
  float visibility_occlusion = clamp(0.5 * (depth_mm - asset_depth_mm) /
    (kDepthTolerancePerMm * asset_depth_mm) + 0.5, 0.0, 1.0);

  // Depth close to zero is most likely invalid, do not use it for occlusions.
  float visibility_depth_near = 1.0 - DepthInverseLerp(
      depth_mm, /*min_depth_mm=*/150.0, /*max_depth_mm=*/200.0);

  // Same for very high depth values.
  float visibility_depth_far = DepthInverseLerp(
      depth_mm, /*min_depth_mm=*/7500.0, /*max_depth_mm=*/8000.0);

  const float kOcclusionAlpha = 0.0;
  float visibility =
      max(max(visibility_occlusion, kOcclusionAlpha),
          max(visibility_depth_near, visibility_depth_far));

  return visibility;
}

float DepthGetBlurredVisibilityAroundUV(in sampler2D depth_texture, in vec2 uv,
                                        in float asset_depth_mm) {
  // Kernel used:
  // 0   4   7   4   0
  // 4   16  26  16  4
  // 7   26  41  26  7
  // 4   16  26  16  4
  // 0   4   7   4   0
  const float kKernelTotalWeights = 269.0;
  float sum = 0.0;

  const float kOcclusionBlurAmount = 0.01;
  vec2 blurriness = vec2(kOcclusionBlurAmount,
                         kOcclusionBlurAmount * u_DepthAspectRatio);

  float current = 0.0;

  current += DepthGetVisibility(depth_texture, uv + vec2(-1.0, -2.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+1.0, -2.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(-1.0, +2.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+1.0, +2.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(-2.0, +1.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+2.0, +1.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(-2.0, -1.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+2.0, -1.0) * blurriness, asset_depth_mm);
  sum += current * 4.0;

  current = 0.0;
  current += DepthGetVisibility(depth_texture, uv + vec2(-2.0, -0.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+2.0, +0.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+0.0, +2.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(-0.0, -2.0) * blurriness, asset_depth_mm);
  sum += current * 7.0;

  current = 0.0;
  current += DepthGetVisibility(depth_texture, uv + vec2(-1.0, -1.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+1.0, -1.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(-1.0, +1.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+1.0, +1.0) * blurriness, asset_depth_mm);
  sum += current * 16.0;

  current = 0.0;
  current += DepthGetVisibility(depth_texture, uv + vec2(+0.0, +1.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(-0.0, -1.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(-1.0, -0.0) * blurriness, asset_depth_mm);
  current += DepthGetVisibility(depth_texture, uv + vec2(+1.0, +0.0) * blurriness, asset_depth_mm);
  sum += current * 26.0;

  sum += DepthGetVisibility(depth_texture, uv , asset_depth_mm) * 41.0;

  return sum / kKernelTotalWeights;
}

#endif // USE_DEPTH_FOR_OCCLUSION

void main() {
    // We support approximate sRGB gamma.
    const float kGamma = 0.4545454;
    const float kInverseGamma = 2.2;
    const float kMiddleGrayGamma = 0.466;

    // Unpack lighting and material parameters for better naming.
    vec3 viewLightDirection = u_LightingParameters.xyz;
    vec3 colorShift = u_ColorCorrectionParameters.rgb;
    float averagePixelIntensity = u_ColorCorrectionParameters.a;

    float materialAmbient = u_MaterialParameters.x;
    float materialDiffuse = u_MaterialParameters.y;
    float materialSpecular = u_MaterialParameters.z;
    float materialSpecularPower = u_MaterialParameters.w;

    // Normalize varying parameters, because they are linearly interpolated in the vertex shader.
    vec3 viewFragmentDirection = normalize(v_ViewPosition);
    vec3 viewNormal = normalize(v_ViewNormal);

    // Flip the y-texture coordinate to address the texture from top-left.
    vec4 objectColor = texture2D(u_Texture, vec2(v_TexCoord.x, 1.0 - v_TexCoord.y));

    // Apply color to grayscale image only if the alpha of u_ObjColor is
    // greater and equal to 255.0.
    objectColor.rgb *= mix(vec3(1.0), u_ObjColor.rgb / 255.0,
                           step(255.0, u_ObjColor.a));

    // Apply inverse SRGB gamma to the texture before making lighting calculations.
    objectColor.rgb = pow(objectColor.rgb, vec3(kInverseGamma));

    if (u_UseHdrLighting == 1) {
        // Environmental HDR: physically-based diffuse from the estimated main
        // directional light plus ambient spherical-harmonics irradiance, both
        // evaluated in ARCore world space.
        vec3 worldNormal = normalize((u_ViewInverse * vec4(viewNormal, 0.0)).xyz);

        // Ambient irradiance reconstructed from the 9 SH coefficients. These are
        // already scaled on the CPU to fold in the SH basis constants, the
        // cosine-lobe convolution, and the 1/pi Lambertian term.
        vec3 ambientIrradiance =
              u_SphericalHarmonics[0]
            + u_SphericalHarmonics[1] * worldNormal.y
            + u_SphericalHarmonics[2] * worldNormal.z
            + u_SphericalHarmonics[3] * worldNormal.x
            + u_SphericalHarmonics[4] * worldNormal.x * worldNormal.y
            + u_SphericalHarmonics[5] * worldNormal.y * worldNormal.z
            + u_SphericalHarmonics[6] * (3.0 * worldNormal.z * worldNormal.z - 1.0)
            + u_SphericalHarmonics[7] * worldNormal.z * worldNormal.x
            + u_SphericalHarmonics[8] * (worldNormal.x * worldNormal.x - worldNormal.y * worldNormal.y);
        ambientIrradiance = max(ambientIrradiance, vec3(0.0));

        // Main directional light (Lambertian, matched to the SH 1/pi scaling).
        const float kPi = 3.14159265359;
        float nDotL = max(dot(worldNormal, u_MainLightDirection), 0.0);
        vec3 mainLight = u_MainLightIntensity * (nDotL / kPi);

        // ARCore returns absolute (and typically small) radiance, so apply an
        // exposure before tone mapping to keep indoor scenes legible. Reinhard
        // then rolls off any highlights so brighter scenes don't blow out.
        const float kHdrExposure = 20.0;
        vec3 linearColor =
            objectColor.rgb * (ambientIrradiance + mainLight) * kHdrExposure;
        vec3 toneMapped = linearColor / (linearColor + vec3(1.0));
        gl_FragColor.rgb = pow(toneMapped, vec3(kGamma));
        gl_FragColor.a = objectColor.a;
    } else {
        // Ambient light is unaffected by the light intensity.
        float ambient = materialAmbient;

        // Approximate a hemisphere light (not a harsh directional light).
        float diffuse = materialDiffuse *
                0.5 * (dot(viewNormal, viewLightDirection) + 1.0);

        // Compute specular light. Textures are loaded with with premultiplied alpha
        // (https://developer.android.com/reference/android/graphics/BitmapFactory.Options#inPremultiplied),
        // so premultiply the specular color by alpha as well.
        vec3 reflectedLightDirection = reflect(viewLightDirection, viewNormal);
        float specularStrength = max(0.0, dot(viewFragmentDirection, reflectedLightDirection));
        float specular = objectColor.a * materialSpecular *
                pow(specularStrength, materialSpecularPower);

        vec3 color = objectColor.rgb * (ambient + diffuse) + specular;
        // Apply SRGB gamma before writing the fragment color.
        color.rgb = pow(color, vec3(kGamma));
        // Apply average pixel intensity and color shift
        color *= colorShift * (averagePixelIntensity / kMiddleGrayGamma);
        gl_FragColor.rgb = color;
        gl_FragColor.a = objectColor.a;
    }

#if USE_DEPTH_FOR_OCCLUSION
    const float kMetersToMillimeters = 1000.0;
    float asset_depth_mm = v_ViewPosition.z * kMetersToMillimeters * -1.;
    // Computes the texture coordinates to sample from the depth image.
    vec2 depth_uvs = (u_DepthUvTransform * vec3(v_ScreenSpacePosition.xy, 1)).xy;

    // The following step is very costly. Replace the last line with the
    // commented line if it's too expensive.
    // gl_FragColor *= DepthGetVisibility(u_DepthTexture, depth_uvs, asset_depth_mm);
    gl_FragColor *= DepthGetBlurredVisibilityAroundUV(u_DepthTexture, depth_uvs, asset_depth_mm);
#endif // USE_DEPTH_FOR_OCCLUSION
}
