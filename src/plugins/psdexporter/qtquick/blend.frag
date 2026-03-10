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

// Blend mode constants (match values used in exporter's applyBlendModes)
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
const int LINEAR_BURN = 11;
const int LINEAR_DODGE = 12;
const int VIVID_LIGHT = 13;
const int LINEAR_LIGHT = 14;
const int PIN_LIGHT = 15;
const int HARD_MIX = 16;
const int SUBTRACT = 17;
const int DIVIDE = 18;
const int DARKER_COLOR = 19;
const int LIGHTER_COLOR = 20;
const int HUE = 21;
const int SATURATION = 22;
const int COLOR = 23;
const int LUMINOSITY = 24;

// --- Separable blend modes ---

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

vec3 blendLinearBurn(vec3 bg, vec3 fg) {
    return max(vec3(0.0), bg + fg - 1.0);
}

vec3 blendLinearDodge(vec3 bg, vec3 fg) {
    return min(vec3(1.0), bg + fg);
}

vec3 blendVividLight(vec3 bg, vec3 fg) {
    return vec3(
        fg.r <= 0.5 ? (fg.r <= 0.0 ? 0.0 : max(0.0, 1.0 - (1.0 - bg.r) / (2.0 * fg.r)))
                     : (fg.r >= 1.0 ? 1.0 : min(1.0, bg.r / (2.0 * (1.0 - fg.r)))),
        fg.g <= 0.5 ? (fg.g <= 0.0 ? 0.0 : max(0.0, 1.0 - (1.0 - bg.g) / (2.0 * fg.g)))
                     : (fg.g >= 1.0 ? 1.0 : min(1.0, bg.g / (2.0 * (1.0 - fg.g)))),
        fg.b <= 0.5 ? (fg.b <= 0.0 ? 0.0 : max(0.0, 1.0 - (1.0 - bg.b) / (2.0 * fg.b)))
                     : (fg.b >= 1.0 ? 1.0 : min(1.0, bg.b / (2.0 * (1.0 - fg.b))))
    );
}

vec3 blendLinearLight(vec3 bg, vec3 fg) {
    return clamp(bg + 2.0 * fg - 1.0, 0.0, 1.0);
}

vec3 blendPinLight(vec3 bg, vec3 fg) {
    return vec3(
        fg.r <= 0.5 ? min(bg.r, 2.0 * fg.r) : max(bg.r, 2.0 * fg.r - 1.0),
        fg.g <= 0.5 ? min(bg.g, 2.0 * fg.g) : max(bg.g, 2.0 * fg.g - 1.0),
        fg.b <= 0.5 ? min(bg.b, 2.0 * fg.b) : max(bg.b, 2.0 * fg.b - 1.0)
    );
}

vec3 blendHardMix(vec3 bg, vec3 fg) {
    return vec3(
        bg.r + fg.r >= 1.0 ? 1.0 : 0.0,
        bg.g + fg.g >= 1.0 ? 1.0 : 0.0,
        bg.b + fg.b >= 1.0 ? 1.0 : 0.0
    );
}

vec3 blendSubtract(vec3 bg, vec3 fg) {
    return max(vec3(0.0), bg - fg);
}

vec3 blendDivide(vec3 bg, vec3 fg) {
    return vec3(
        fg.r <= 0.0 ? 1.0 : min(1.0, bg.r / fg.r),
        fg.g <= 0.0 ? 1.0 : min(1.0, bg.g / fg.g),
        fg.b <= 0.0 ? 1.0 : min(1.0, bg.b / fg.b)
    );
}

// --- Non-separable blend modes (HSL) ---

float lum(vec3 c) {
    return 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
}

float sat(vec3 c) {
    return max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));
}

vec3 clipColor(vec3 c) {
    float l = lum(c);
    float n = min(c.r, min(c.g, c.b));
    float x = max(c.r, max(c.g, c.b));
    if (n < 0.0) c = l + (c - l) * l / (l - n);
    if (x > 1.0) c = l + (c - l) * (1.0 - l) / (x - l);
    return c;
}

vec3 setLum(vec3 c, float l) {
    return clipColor(c + (l - lum(c)));
}

vec3 setSat(vec3 c, float s) {
    float cmin = min(c.r, min(c.g, c.b));
    float cmax = max(c.r, max(c.g, c.b));
    if (cmax > cmin) {
        return (c - cmin) * s / (cmax - cmin);
    }
    return vec3(0.0);
}

void main() {
    vec4 fg = texture(source, qt_TexCoord0);
    vec4 bg = texture(background, qt_TexCoord0);

    // Qt Quick layer textures use premultiplied alpha.
    // Unpremultiply to get straight-alpha colors for blend math.
    vec3 Cs = fg.a > 0.0 ? fg.rgb / fg.a : vec3(0.0);
    vec3 Cb = bg.a > 0.0 ? bg.rgb / bg.a : vec3(0.0);

    vec3 B;
    if (blendMode == MULTIPLY)              B = blendMultiply(Cb, Cs);
    else if (blendMode == SCREEN)           B = blendScreen(Cb, Cs);
    else if (blendMode == OVERLAY)          B = blendOverlay(Cb, Cs);
    else if (blendMode == DARKEN)           B = blendDarken(Cb, Cs);
    else if (blendMode == LIGHTEN)          B = blendLighten(Cb, Cs);
    else if (blendMode == COLOR_DODGE)      B = blendColorDodge(Cb, Cs);
    else if (blendMode == COLOR_BURN)       B = blendColorBurn(Cb, Cs);
    else if (blendMode == HARD_LIGHT)       B = blendHardLight(Cb, Cs);
    else if (blendMode == SOFT_LIGHT)       B = blendSoftLight(Cb, Cs);
    else if (blendMode == DIFFERENCE)       B = blendDifference(Cb, Cs);
    else if (blendMode == EXCLUSION)        B = blendExclusion(Cb, Cs);
    else if (blendMode == LINEAR_BURN)      B = blendLinearBurn(Cb, Cs);
    else if (blendMode == LINEAR_DODGE)     B = blendLinearDodge(Cb, Cs);
    else if (blendMode == VIVID_LIGHT)      B = blendVividLight(Cb, Cs);
    else if (blendMode == LINEAR_LIGHT)     B = blendLinearLight(Cb, Cs);
    else if (blendMode == PIN_LIGHT)        B = blendPinLight(Cb, Cs);
    else if (blendMode == HARD_MIX)         B = blendHardMix(Cb, Cs);
    else if (blendMode == SUBTRACT)         B = blendSubtract(Cb, Cs);
    else if (blendMode == DIVIDE)           B = blendDivide(Cb, Cs);
    else if (blendMode == DARKER_COLOR)     B = lum(Cb) <= lum(Cs) ? Cb : Cs;
    else if (blendMode == LIGHTER_COLOR)    B = lum(Cb) >= lum(Cs) ? Cb : Cs;
    else if (blendMode == HUE)             B = setLum(setSat(Cs, sat(Cb)), lum(Cb));
    else if (blendMode == SATURATION)      B = setLum(setSat(Cb, sat(Cs)), lum(Cb));
    else if (blendMode == COLOR)           B = setLum(Cs, lum(Cb));
    else if (blendMode == LUMINOSITY)      B = setLum(Cb, lum(Cs));
    else                                    B = Cs; // fallback: normal

    // W3C Compositing: source-over with blend mode (premultiplied output).
    // Co = (1 - αb) * Cs·αs + (1 - αs) * Cb·αb + αs·αb · B(Cb, Cs)
    float Ao = fg.a + bg.a * (1.0 - fg.a);
    vec3 Co = (1.0 - bg.a) * fg.rgb
            + (1.0 - fg.a) * bg.rgb
            + fg.a * bg.a * B;

    fragColor = vec4(Co, Ao) * qt_Opacity;
}
