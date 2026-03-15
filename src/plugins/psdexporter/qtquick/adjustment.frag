#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    int adjustmentType;
    // Brightness/Contrast (brit)
    float brightness;   // -150..150, normalized to -1..1
    float contrast;     // -50..100, normalized

    // Levels (levl) — per-channel input/output
    float lvl_shadowIn;     // 0..255 normalized to 0..1
    float lvl_highlightIn;
    float lvl_shadowOut;
    float lvl_highlightOut;
    float lvl_midtone;      // gamma (0.01..9.99, default 1.0)
    // Per-channel levels (red)
    float lvlR_shadowIn;
    float lvlR_highlightIn;
    float lvlR_shadowOut;
    float lvlR_highlightOut;
    float lvlR_midtone;
    // Per-channel levels (green)
    float lvlG_shadowIn;
    float lvlG_highlightIn;
    float lvlG_shadowOut;
    float lvlG_highlightOut;
    float lvlG_midtone;
    // Per-channel levels (blue)
    float lvlB_shadowIn;
    float lvlB_highlightIn;
    float lvlB_shadowOut;
    float lvlB_highlightOut;
    float lvlB_midtone;

    // Exposure (expA)
    float exposure;    // EV stops
    float offset;
    float gamma;       // correction

    // Hue/Saturation (hue2) — master only
    float hueShift;        // -180..180 degrees
    float saturationShift; // -100..100
    float lightnessShift;  // -100..100

    // Color Balance (blnc) — shadows/midtones/highlights
    float bal_shadow_cr;   // cyan-red (-100..100)
    float bal_shadow_mg;   // magenta-green
    float bal_shadow_yb;   // yellow-blue
    float bal_mid_cr;
    float bal_mid_mg;
    float bal_mid_yb;
    float bal_hi_cr;
    float bal_hi_mg;
    float bal_hi_yb;
    float bal_preserveLum;

    // Photo Filter (phfl)
    float phfl_r;
    float phfl_g;
    float phfl_b;
    float phfl_density;    // 0..100 normalized to 0..1
    float phfl_preserveLum;

    // Posterize (post)
    float posterizeLevels; // 2..255

    // Threshold (thrs)
    float thresholdLevel;  // 0..255 normalized to 0..1

    // Vibrance (vibA)
    float vibrance;        // -100..100 normalized to -1..1
    float vibranceSat;     // saturation slider -100..100

    // Channel Mixer (mixr)
    float mixr_outR_r;    // red output: red coefficient
    float mixr_outR_g;
    float mixr_outR_b;
    float mixr_outR_c;    // constant
    float mixr_outG_r;
    float mixr_outG_g;
    float mixr_outG_b;
    float mixr_outG_c;
    float mixr_outB_r;
    float mixr_outB_g;
    float mixr_outB_b;
    float mixr_outB_c;
    float mixr_mono;      // monochrome flag (0 or 1)

    // Black & White (blwh) — channel weights
    float bw_red;
    float bw_yellow;
    float bw_green;
    float bw_cyan;
    float bw_blue;
    float bw_magenta;
};

layout(binding = 1) uniform sampler2D source;
// Curves LUT texture (256x1 RGBA): R=red curve, G=green curve, B=blue curve, A=rgb curve
layout(binding = 2) uniform sampler2D curvesLUT;

// Adjustment type constants
const int ADJ_BRIGHTNESS    = 0;
const int ADJ_LEVELS        = 1;
const int ADJ_CURVES        = 2;
const int ADJ_EXPOSURE      = 3;
const int ADJ_HUE_SAT       = 4;
const int ADJ_COLOR_BALANCE = 5;
const int ADJ_PHOTO_FILTER  = 6;
const int ADJ_INVERT        = 7;
const int ADJ_POSTERIZE     = 8;
const int ADJ_THRESHOLD     = 9;
const int ADJ_VIBRANCE      = 10;
const int ADJ_CHANNEL_MIXER = 11;
const int ADJ_BLACK_WHITE   = 12;
const int ADJ_GRADIENT_MAP  = 13;

// --- HSL helpers (reused from blend.frag) ---

float lum(vec3 c) {
    return 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
}

vec3 rgb2hsl(vec3 c) {
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float l = (mx + mn) * 0.5;
    if (mx == mn) return vec3(0.0, 0.0, l);
    float d = mx - mn;
    float s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
    float h;
    if (mx == c.r)      h = (c.g - c.b) / d + (c.g < c.b ? 6.0 : 0.0);
    else if (mx == c.g) h = (c.b - c.r) / d + 2.0;
    else                h = (c.r - c.g) / d + 4.0;
    return vec3(h / 6.0, s, l);
}

float hue2rgb(float p, float q, float t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

vec3 hsl2rgb(vec3 hsl) {
    if (hsl.y == 0.0) return vec3(hsl.z);
    float q = hsl.z < 0.5 ? hsl.z * (1.0 + hsl.y) : hsl.z + hsl.y - hsl.z * hsl.y;
    float p = 2.0 * hsl.z - q;
    return vec3(
        hue2rgb(p, q, hsl.x + 1.0/3.0),
        hue2rgb(p, q, hsl.x),
        hue2rgb(p, q, hsl.x - 1.0/3.0)
    );
}

// --- Levels helper ---
float applyLevels(float v, float sIn, float hIn, float sOut, float hOut, float mid) {
    // Input levels: remap sIn..hIn to 0..1
    float range = hIn - sIn;
    if (range <= 0.0) range = 1.0/255.0;
    v = clamp((v - sIn) / range, 0.0, 1.0);
    // Midtone (gamma)
    if (mid != 1.0 && mid > 0.0)
        v = pow(v, 1.0 / mid);
    // Output levels: remap 0..1 to sOut..hOut
    return mix(sOut, hOut, v);
}

void main() {
    vec4 texel = texture(source, qt_TexCoord0);
    // Unpremultiply
    vec3 color = texel.a > 0.0 ? texel.rgb / texel.a : vec3(0.0);
    float alpha = texel.a;

    if (adjustmentType == ADJ_BRIGHTNESS) {
        // Photoshop Brightness/Contrast (modern, non-legacy)
        // brightness: -150..150 mapped to uniform as raw value / 150
        // contrast: -50..100 mapped to uniform as raw value / 100
        color = clamp(color + brightness, 0.0, 1.0);
        if (contrast > 0.0) {
            color = clamp((color - 0.5) * (1.0 / (1.0 - contrast)) + 0.5, 0.0, 1.0);
        } else if (contrast < 0.0) {
            color = clamp((color - 0.5) * (1.0 + contrast) + 0.5, 0.0, 1.0);
        }
    }
    else if (adjustmentType == ADJ_LEVELS) {
        color.r = applyLevels(color.r, lvlR_shadowIn, lvlR_highlightIn, lvlR_shadowOut, lvlR_highlightOut, lvlR_midtone);
        color.g = applyLevels(color.g, lvlG_shadowIn, lvlG_highlightIn, lvlG_shadowOut, lvlG_highlightOut, lvlG_midtone);
        color.b = applyLevels(color.b, lvlB_shadowIn, lvlB_highlightIn, lvlB_shadowOut, lvlB_highlightOut, lvlB_midtone);
        // Apply RGB master levels on top
        color.r = applyLevels(color.r, lvl_shadowIn, lvl_highlightIn, lvl_shadowOut, lvl_highlightOut, lvl_midtone);
        color.g = applyLevels(color.g, lvl_shadowIn, lvl_highlightIn, lvl_shadowOut, lvl_highlightOut, lvl_midtone);
        color.b = applyLevels(color.b, lvl_shadowIn, lvl_highlightIn, lvl_shadowOut, lvl_highlightOut, lvl_midtone);
    }
    else if (adjustmentType == ADJ_CURVES) {
        // Look up from 256x1 LUT texture
        // curvesLUT: R channel = red curve, G = green curve, B = blue curve, A = rgb master
        float master_r = texture(curvesLUT, vec2(color.r, 0.5)).a;
        float master_g = texture(curvesLUT, vec2(color.g, 0.5)).a;
        float master_b = texture(curvesLUT, vec2(color.b, 0.5)).a;
        color.r = texture(curvesLUT, vec2(master_r, 0.5)).r;
        color.g = texture(curvesLUT, vec2(master_g, 0.5)).g;
        color.b = texture(curvesLUT, vec2(master_b, 0.5)).b;
    }
    else if (adjustmentType == ADJ_EXPOSURE) {
        // Exposure: linear light space
        color = clamp(pow(max(vec3(0.0), color * pow(2.0, exposure) + offset), vec3(1.0 / gamma)), 0.0, 1.0);
    }
    else if (adjustmentType == ADJ_HUE_SAT) {
        vec3 hsl = rgb2hsl(color);
        hsl.x = fract(hsl.x + hueShift / 360.0);
        hsl.y = clamp(hsl.y + saturationShift / 100.0, 0.0, 1.0);
        hsl.z = clamp(hsl.z + lightnessShift / 100.0, 0.0, 1.0);
        color = hsl2rgb(hsl);
    }
    else if (adjustmentType == ADJ_COLOR_BALANCE) {
        float l = lum(color);
        // Tonal range weights
        float shadowW = clamp(1.0 - l / 0.5, 0.0, 1.0);     // shadows: dark pixels
        float highW = clamp((l - 0.5) / 0.5, 0.0, 1.0);      // highlights: bright pixels
        float midW = 1.0 - shadowW - highW;                    // midtones: middle range

        vec3 shift = vec3(0.0);
        shift += shadowW * vec3(bal_shadow_cr, bal_shadow_mg, bal_shadow_yb) / 100.0;
        shift += midW * vec3(bal_mid_cr, bal_mid_mg, bal_mid_yb) / 100.0;
        shift += highW * vec3(bal_hi_cr, bal_hi_mg, bal_hi_yb) / 100.0;
        color = clamp(color + shift, 0.0, 1.0);

        if (bal_preserveLum > 0.5) {
            float newL = lum(color);
            if (newL > 0.0)
                color *= l / newL;
        }
    }
    else if (adjustmentType == ADJ_PHOTO_FILTER) {
        vec3 filterColor = vec3(phfl_r, phfl_g, phfl_b);
        float d = phfl_density;
        if (phfl_preserveLum > 0.5) {
            float origLum = lum(color);
            color = mix(color, color * filterColor, d);
            float newLum = lum(color);
            if (newLum > 0.0)
                color *= origLum / newLum;
        } else {
            color = mix(color, color * filterColor, d);
        }
        color = clamp(color, 0.0, 1.0);
    }
    else if (adjustmentType == ADJ_INVERT) {
        color = 1.0 - color;
    }
    else if (adjustmentType == ADJ_POSTERIZE) {
        float levels = max(2.0, posterizeLevels);
        color = floor(color * levels) / (levels - 1.0);
        color = clamp(color, 0.0, 1.0);
    }
    else if (adjustmentType == ADJ_THRESHOLD) {
        float gray = lum(color);
        color = vec3(gray >= thresholdLevel ? 1.0 : 0.0);
    }
    else if (adjustmentType == ADJ_VIBRANCE) {
        // Vibrance: boost saturation of less-saturated pixels more
        vec3 hsl = rgb2hsl(color);
        float satBoost = vibrance * (1.0 - hsl.y); // more boost for less saturated
        hsl.y = clamp(hsl.y + satBoost, 0.0, 1.0);
        // Also apply flat saturation shift
        hsl.y = clamp(hsl.y + vibranceSat / 100.0, 0.0, 1.0);
        color = hsl2rgb(hsl);
    }
    else if (adjustmentType == ADJ_CHANNEL_MIXER) {
        vec3 result;
        if (mixr_mono > 0.5) {
            // Monochrome: single output channel
            float gray = mixr_outR_r / 100.0 * color.r + mixr_outR_g / 100.0 * color.g + mixr_outR_b / 100.0 * color.b + mixr_outR_c / 100.0;
            result = vec3(clamp(gray, 0.0, 1.0));
        } else {
            result.r = clamp(mixr_outR_r / 100.0 * color.r + mixr_outR_g / 100.0 * color.g + mixr_outR_b / 100.0 * color.b + mixr_outR_c / 100.0, 0.0, 1.0);
            result.g = clamp(mixr_outG_r / 100.0 * color.r + mixr_outG_g / 100.0 * color.g + mixr_outG_b / 100.0 * color.b + mixr_outG_c / 100.0, 0.0, 1.0);
            result.b = clamp(mixr_outB_r / 100.0 * color.r + mixr_outB_g / 100.0 * color.g + mixr_outB_b / 100.0 * color.b + mixr_outB_c / 100.0, 0.0, 1.0);
        }
        color = result;
    }
    else if (adjustmentType == ADJ_BLACK_WHITE) {
        // Photoshop Black & White uses 6 color channels
        // Convert RGB to approximate channel contributions
        vec3 hsl = rgb2hsl(color);
        float h = hsl.x * 360.0; // 0..360

        // Determine weight for each color range based on hue
        // Red: around 0/360, Yellow: 60, Green: 120, Cyan: 180, Blue: 240, Magenta: 300
        float rW = max(0.0, 1.0 - min(abs(h), abs(h - 360.0)) / 60.0);
        float yW = max(0.0, 1.0 - abs(h - 60.0) / 60.0);
        float gW = max(0.0, 1.0 - abs(h - 120.0) / 60.0);
        float cW = max(0.0, 1.0 - abs(h - 180.0) / 60.0);
        float bW = max(0.0, 1.0 - abs(h - 240.0) / 60.0);
        float mW = max(0.0, 1.0 - abs(h - 300.0) / 60.0);

        float totalW = rW + yW + gW + cW + bW + mW;
        if (totalW > 0.0) {
            float mixFactor = (rW * bw_red + yW * bw_yellow + gW * bw_green +
                             cW * bw_cyan + bW * bw_blue + mW * bw_magenta) / (totalW * 100.0);
            float gray = lum(color) + (mixFactor - 0.5) * hsl.y;
            color = vec3(clamp(gray, 0.0, 1.0));
        } else {
            color = vec3(lum(color));
        }
    }
    else if (adjustmentType == ADJ_GRADIENT_MAP) {
        // Gradient map: luminance -> gradient color lookup via curvesLUT texture
        float gray = lum(color);
        color = texture(curvesLUT, vec2(gray, 0.5)).rgb;
    }

    // Re-premultiply
    fragColor = vec4(color * alpha, alpha) * qt_Opacity;
}
