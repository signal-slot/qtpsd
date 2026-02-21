#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    int blendMode;
};

layout(binding = 1) uniform sampler2D source;     // foreground (element content)
layout(binding = 2) uniform sampler2D background;  // accumulated background

// Blend mode constants (match QPsdBlend::Mode values used in exporter)
const int MULTIPLY = 0;
const int SCREEN = 1;
const int OVERLAY = 2;
const int DARKEN = 3;
const int LIGHTEN = 4;
const int COLOR_DODGE = 5;
const int COLOR_BURN = 6;
const int HARD_LIGHT = 7;
const int SOFT_LIGHT = 8;
const int DIFFERENCE = 9;
const int EXCLUSION = 10;

vec3 blendMultiply(vec3 bg, vec3 fg) {
    return bg * fg;
}

vec3 blendScreen(vec3 bg, vec3 fg) {
    return 1.0 - (1.0 - bg) * (1.0 - fg);
}

vec3 blendOverlay(vec3 bg, vec3 fg) {
    return vec3(
        bg.r < 0.5 ? 2.0 * bg.r * fg.r : 1.0 - 2.0 * (1.0 - bg.r) * (1.0 - fg.r),
        bg.g < 0.5 ? 2.0 * bg.g * fg.g : 1.0 - 2.0 * (1.0 - bg.g) * (1.0 - fg.g),
        bg.b < 0.5 ? 2.0 * bg.b * fg.b : 1.0 - 2.0 * (1.0 - bg.b) * (1.0 - fg.b)
    );
}

vec3 blendDarken(vec3 bg, vec3 fg) {
    return min(bg, fg);
}

vec3 blendLighten(vec3 bg, vec3 fg) {
    return max(bg, fg);
}

vec3 blendColorDodge(vec3 bg, vec3 fg) {
    return vec3(
        fg.r >= 1.0 ? 1.0 : min(1.0, bg.r / (1.0 - fg.r)),
        fg.g >= 1.0 ? 1.0 : min(1.0, bg.g / (1.0 - fg.g)),
        fg.b >= 1.0 ? 1.0 : min(1.0, bg.b / (1.0 - fg.b))
    );
}

vec3 blendColorBurn(vec3 bg, vec3 fg) {
    return vec3(
        fg.r <= 0.0 ? 0.0 : max(0.0, 1.0 - (1.0 - bg.r) / fg.r),
        fg.g <= 0.0 ? 0.0 : max(0.0, 1.0 - (1.0 - bg.g) / fg.g),
        fg.b <= 0.0 ? 0.0 : max(0.0, 1.0 - (1.0 - bg.b) / fg.b)
    );
}

vec3 blendHardLight(vec3 bg, vec3 fg) {
    return vec3(
        fg.r < 0.5 ? 2.0 * bg.r * fg.r : 1.0 - 2.0 * (1.0 - bg.r) * (1.0 - fg.r),
        fg.g < 0.5 ? 2.0 * bg.g * fg.g : 1.0 - 2.0 * (1.0 - bg.g) * (1.0 - fg.g),
        fg.b < 0.5 ? 2.0 * bg.b * fg.b : 1.0 - 2.0 * (1.0 - bg.b) * (1.0 - fg.b)
    );
}

float softLightChannel(float bg, float fg) {
    if (fg <= 0.5) {
        return bg - (1.0 - 2.0 * fg) * bg * (1.0 - bg);
    } else {
        float d = (bg <= 0.25)
            ? ((16.0 * bg - 12.0) * bg + 4.0) * bg
            : sqrt(bg);
        return bg + (2.0 * fg - 1.0) * (d - bg);
    }
}

vec3 blendSoftLight(vec3 bg, vec3 fg) {
    return vec3(
        softLightChannel(bg.r, fg.r),
        softLightChannel(bg.g, fg.g),
        softLightChannel(bg.b, fg.b)
    );
}

vec3 blendDifference(vec3 bg, vec3 fg) {
    return abs(bg - fg);
}

vec3 blendExclusion(vec3 bg, vec3 fg) {
    return bg + fg - 2.0 * bg * fg;
}

void main() {
    vec4 fg = texture(source, qt_TexCoord0);
    vec4 bg = texture(background, qt_TexCoord0);

    vec3 result;
    if (blendMode == MULTIPLY)         result = blendMultiply(bg.rgb, fg.rgb);
    else if (blendMode == SCREEN)      result = blendScreen(bg.rgb, fg.rgb);
    else if (blendMode == OVERLAY)     result = blendOverlay(bg.rgb, fg.rgb);
    else if (blendMode == DARKEN)      result = blendDarken(bg.rgb, fg.rgb);
    else if (blendMode == LIGHTEN)     result = blendLighten(bg.rgb, fg.rgb);
    else if (blendMode == COLOR_DODGE) result = blendColorDodge(bg.rgb, fg.rgb);
    else if (blendMode == COLOR_BURN)  result = blendColorBurn(bg.rgb, fg.rgb);
    else if (blendMode == HARD_LIGHT)  result = blendHardLight(bg.rgb, fg.rgb);
    else if (blendMode == SOFT_LIGHT)  result = blendSoftLight(bg.rgb, fg.rgb);
    else if (blendMode == DIFFERENCE)  result = blendDifference(bg.rgb, fg.rgb);
    else if (blendMode == EXCLUSION)   result = blendExclusion(bg.rgb, fg.rgb);
    else                               result = fg.rgb; // fallback: normal

    // Composite blended result over background using foreground alpha
    fragColor = vec4(mix(bg.rgb, result, fg.a), max(bg.a, fg.a)) * qt_Opacity;
}
