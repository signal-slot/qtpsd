#version 460 core

// Photoshop adjustment-layer shader for Flutter (FragmentProgram).
// Port of the QtQuick adjustment.frag; the uniform order here must match
// the _uniformNames list in the generated PsdAdjustment widget.

#include <flutter/runtime_effect.glsl>

precision highp float;

uniform vec2 uSize;

uniform float uType;

// Brightness/Contrast (brit)
uniform float brightness;   // raw value / 255
uniform float contrast;     // raw value / 100
uniform float brit_pivot;   // modern contrast pivot (means / 255)
uniform float brit_modern;  // 1.0 = modern (contrast first), 0.0 = legacy

// Levels (levl) — master + per-channel
uniform float lvl_shadowIn;
uniform float lvl_highlightIn;
uniform float lvl_shadowOut;
uniform float lvl_highlightOut;
uniform float lvl_midtone;
uniform float lvlR_shadowIn;
uniform float lvlR_highlightIn;
uniform float lvlR_shadowOut;
uniform float lvlR_highlightOut;
uniform float lvlR_midtone;
uniform float lvlG_shadowIn;
uniform float lvlG_highlightIn;
uniform float lvlG_shadowOut;
uniform float lvlG_highlightOut;
uniform float lvlG_midtone;
uniform float lvlB_shadowIn;
uniform float lvlB_highlightIn;
uniform float lvlB_shadowOut;
uniform float lvlB_highlightOut;
uniform float lvlB_midtone;

// Exposure (expA)
uniform float exposure;
uniform float offset;
uniform float gamma;

// Hue/Saturation (hue2)
uniform float hueShift;
uniform float saturationShift;
uniform float lightnessShift;

// Color balance (blnc)
uniform float bal_shadow_cr;
uniform float bal_shadow_mg;
uniform float bal_shadow_yb;
uniform float bal_mid_cr;
uniform float bal_mid_mg;
uniform float bal_mid_yb;
uniform float bal_hi_cr;
uniform float bal_hi_mg;
uniform float bal_hi_yb;
uniform float bal_preserveLum;

// Photo filter (phfl)
uniform float phfl_r;
uniform float phfl_g;
uniform float phfl_b;
uniform float phfl_density;
uniform float phfl_preserveLum;

// Posterize / Threshold
uniform float post_levels;
uniform float threshold;

// Vibrance (vibA)
uniform float vibrance;
uniform float vibranceSat;

// Channel mixer (mixr)
uniform float mixr_outR_r;
uniform float mixr_outR_g;
uniform float mixr_outR_b;
uniform float mixr_outR_c;
uniform float mixr_outG_r;
uniform float mixr_outG_g;
uniform float mixr_outG_b;
uniform float mixr_outG_c;
uniform float mixr_outB_r;
uniform float mixr_outB_g;
uniform float mixr_outB_b;
uniform float mixr_outB_c;
uniform float mixr_mono;

// Black & White (blwh)
uniform float bw_red;
uniform float bw_yellow;
uniform float bw_green;
uniform float bw_cyan;
uniform float bw_blue;
uniform float bw_magenta;

// Per-pixel weighting
uniform float adjWeight;
uniform float useWeightMask;

uniform sampler2D uSrc;     // rasterized layers below the adjustment
uniform sampler2D uLut;     // 256x1 curves / gradient-map LUT
uniform sampler2D uWeight;  // grayscale weight mask

out vec4 fragColor;

const float ADJ_BRIGHTNESS    = 0.0;
const float ADJ_LEVELS        = 1.0;
const float ADJ_CURVES        = 2.0;
const float ADJ_EXPOSURE      = 3.0;
const float ADJ_HUE_SAT       = 4.0;
const float ADJ_COLOR_BALANCE = 5.0;
const float ADJ_PHOTO_FILTER  = 6.0;
const float ADJ_INVERT        = 7.0;
const float ADJ_POSTERIZE     = 8.0;
const float ADJ_THRESHOLD     = 9.0;
const float ADJ_VIBRANCE      = 10.0;
const float ADJ_CHANNEL_MIXER = 11.0;
const float ADJ_BLACK_WHITE   = 12.0;
const float ADJ_GRADIENT_MAP  = 13.0;

bool isType(float t) {
    return abs(uType - t) < 0.5;
}

float lum(vec3 c) {
    return 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
}

vec3 rgb2hsl(vec3 c) {
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float l = (mx + mn) * 0.5;
    if (mx == mn)
        return vec3(0.0, 0.0, l);
    float d = mx - mn;
    float s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
    float h;
    if (mx == c.r)
        h = (c.g - c.b) / d + (c.g < c.b ? 6.0 : 0.0);
    else if (mx == c.g)
        h = (c.b - c.r) / d + 2.0;
    else
        h = (c.r - c.g) / d + 4.0;
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
    if (hsl.y <= 0.0)
        return vec3(hsl.z);
    float q = hsl.z < 0.5 ? hsl.z * (1.0 + hsl.y) : hsl.z + hsl.y - hsl.z * hsl.y;
    float p = 2.0 * hsl.z - q;
    return vec3(
        hue2rgb(p, q, hsl.x + 1.0/3.0),
        hue2rgb(p, q, hsl.x),
        hue2rgb(p, q, hsl.x - 1.0/3.0)
    );
}

float applyLevels(float v, float sIn, float hIn, float sOut, float hOut, float mid) {
    float range = hIn - sIn;
    if (range <= 0.0) range = 1.0/255.0;
    v = clamp((v - sIn) / range, 0.0, 1.0);
    if (mid != 1.0 && mid > 0.0)
        v = pow(v, 1.0 / mid);
    return mix(sOut, hOut, v);
}

vec3 srgb2lin(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

vec3 lin2srgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

void main() {
    vec2 uv = FlutterFragCoord().xy / uSize;
    vec4 texel = texture(uSrc, uv);
    vec3 color = texel.a > 0.0 ? texel.rgb / texel.a : vec3(0.0);
    float alpha = texel.a;
    vec3 origColor = color;

    if (isType(ADJ_BRIGHTNESS)) {
        // Legacy applies brightness then contrast around 0.5;
        // modern applies contrast around brit_pivot first, then brightness
        if (brit_modern > 0.5) {
            if (contrast > 0.0) {
                color = clamp((color - brit_pivot) * (1.0 / (1.0 - min(contrast, 0.999))) + brit_pivot, 0.0, 1.0);
            } else if (contrast < 0.0) {
                color = clamp((color - brit_pivot) * (1.0 + contrast) + brit_pivot, 0.0, 1.0);
            }
            color = clamp(color + brightness, 0.0, 1.0);
        } else {
            color = clamp(color + brightness, 0.0, 1.0);
            if (contrast > 0.0) {
                color = clamp((color - 0.5) * (1.0 / (1.0 - min(contrast, 0.999))) + 0.5, 0.0, 1.0);
            } else if (contrast < 0.0) {
                color = clamp((color - 0.5) * (1.0 + contrast) + 0.5, 0.0, 1.0);
            }
        }
    }
    if (isType(ADJ_LEVELS)) {
        color.r = applyLevels(color.r, lvlR_shadowIn, lvlR_highlightIn, lvlR_shadowOut, lvlR_highlightOut, lvlR_midtone);
        color.g = applyLevels(color.g, lvlG_shadowIn, lvlG_highlightIn, lvlG_shadowOut, lvlG_highlightOut, lvlG_midtone);
        color.b = applyLevels(color.b, lvlB_shadowIn, lvlB_highlightIn, lvlB_shadowOut, lvlB_highlightOut, lvlB_midtone);
        color.r = applyLevels(color.r, lvl_shadowIn, lvl_highlightIn, lvl_shadowOut, lvl_highlightOut, lvl_midtone);
        color.g = applyLevels(color.g, lvl_shadowIn, lvl_highlightIn, lvl_shadowOut, lvl_highlightOut, lvl_midtone);
        color.b = applyLevels(color.b, lvl_shadowIn, lvl_highlightIn, lvl_shadowOut, lvl_highlightOut, lvl_midtone);
    }
    if (isType(ADJ_CURVES)) {
        // The master (rgb) curve is composed into each channel at export time
        color.r = texture(uLut, vec2(color.r, 0.5)).r;
        color.g = texture(uLut, vec2(color.g, 0.5)).g;
        color.b = texture(uLut, vec2(color.b, 0.5)).b;
    }
    if (isType(ADJ_EXPOSURE)) {
        // Exposure operates in linear light
        vec3 lin = srgb2lin(color);
        lin = pow(max(vec3(0.0), lin * pow(2.0, exposure) + offset), vec3(1.0 / gamma));
        color = clamp(lin2srgb(lin), 0.0, 1.0);
    }
    if (isType(ADJ_HUE_SAT)) {
        vec3 hsl = rgb2hsl(color);
        hsl.x = fract(hsl.x + hueShift / 360.0);
        hsl.y = clamp(hsl.y + saturationShift / 100.0, 0.0, 1.0);
        hsl.z = clamp(hsl.z + lightnessShift / 100.0, 0.0, 1.0);
        color = hsl2rgb(hsl);
    }
    if (isType(ADJ_COLOR_BALANCE)) {
        float l = lum(color);
        float shadowW = clamp(1.0 - l / 0.5, 0.0, 1.0);
        float highW = clamp((l - 0.5) / 0.5, 0.0, 1.0);
        float midW = 1.0 - shadowW - highW;
        vec3 delta = shadowW * vec3(bal_shadow_cr, bal_shadow_mg, bal_shadow_yb);
        delta += midW * vec3(bal_mid_cr, bal_mid_mg, bal_mid_yb);
        delta += highW * vec3(bal_hi_cr, bal_hi_mg, bal_hi_yb);
        color = clamp(color + delta / 100.0, 0.0, 1.0);
        if (bal_preserveLum > 0.5) {
            float newL = lum(color);
            if (newL > 0.0)
                color = clamp(color * (l / newL), 0.0, 1.0);
        }
    }
    if (isType(ADJ_PHOTO_FILTER)) {
        // Multiply with the filter color in linear light, mix at density,
        // then restore the original luminosity
        vec3 filterColor = vec3(phfl_r, phfl_g, phfl_b);
        vec3 lin = srgb2lin(color);
        vec3 flin = srgb2lin(filterColor);
        lin = mix(lin, lin * flin, phfl_density);
        float origLum = lum(color);
        color = clamp(lin2srgb(lin), 0.0, 1.0);
        if (phfl_preserveLum > 0.5) {
            float newLum = lum(color);
            if (newLum > 0.0)
                color = clamp(color * (origLum / newLum), 0.0, 1.0);
        }
    }
    if (isType(ADJ_INVERT)) {
        color = vec3(1.0) - color;
    }
    if (isType(ADJ_POSTERIZE)) {
        float levels = max(2.0, post_levels);
        color = clamp(floor(color * levels) / (levels - 1.0), 0.0, 1.0);
    }
    if (isType(ADJ_THRESHOLD)) {
        float v = lum(color) >= threshold - 0.5 / 255.0 ? 1.0 : 0.0;
        color = vec3(v);
    }
    if (isType(ADJ_VIBRANCE)) {
        // Push each channel away from the max channel by the squared
        // distance, then apply the saturation slider as a gentle spread
        // around the channel average
        float mx = max(color.r, max(color.g, color.b));
        vec3 d = vec3(mx) - color;
        color = color - d * d * 1.5 * vibrance;
        float avg = (color.r + color.g + color.b) / 3.0;
        color = clamp(vec3(avg) + (color - vec3(avg)) * (1.0 + 0.5 * vibranceSat), 0.0, 1.0);
    }
    if (isType(ADJ_CHANNEL_MIXER)) {
        // dot() keeps the expressions shallow; SkSL has a max parse depth
        vec3 kR = vec3(mixr_outR_r, mixr_outR_g, mixr_outR_b) / 100.0;
        vec3 kG = vec3(mixr_outG_r, mixr_outG_g, mixr_outG_b) / 100.0;
        vec3 kB = vec3(mixr_outB_r, mixr_outB_g, mixr_outB_b) / 100.0;
        float outR = dot(kR, color) + mixr_outR_c / 100.0;
        float outG = dot(kG, color) + mixr_outG_c / 100.0;
        float outB = dot(kB, color) + mixr_outB_c / 100.0;
        vec3 result = clamp(vec3(outR, outG, outB), 0.0, 1.0);
        if (mixr_mono > 0.5) {
            result = vec3(result.r);
        }
        color = result;
    }
    if (isType(ADJ_BLACK_WHITE)) {
        vec3 hsl = rgb2hsl(color);
        float h = hsl.x * 360.0;
        float rW = max(0.0, 1.0 - min(abs(h), abs(h - 360.0)) / 60.0);
        float yW = max(0.0, 1.0 - abs(h - 60.0) / 60.0);
        float gW = max(0.0, 1.0 - abs(h - 120.0) / 60.0);
        float cW = max(0.0, 1.0 - abs(h - 180.0) / 60.0);
        float bW = max(0.0, 1.0 - abs(h - 240.0) / 60.0);
        float mW = max(0.0, 1.0 - abs(h - 300.0) / 60.0);
        float totalW = rW + yW + gW + cW + bW + mW;
        // The neutral point is the channel average, not Rec.601 luma
        float avg = (color.r + color.g + color.b) / 3.0;
        if (totalW > 0.0) {
            float weighted = dot(vec3(rW, yW, gW), vec3(bw_red, bw_yellow, bw_green));
            weighted += dot(vec3(cW, bW, mW), vec3(bw_cyan, bw_blue, bw_magenta));
            float mixFactor = weighted / (totalW * 100.0);
            float gray = avg + (mixFactor - 0.5) * hsl.y;
            color = vec3(clamp(gray, 0.0, 1.0));
        } else {
            color = vec3(avg);
        }
    }
    if (isType(ADJ_GRADIENT_MAP)) {
        float gray = lum(color);
        color = texture(uLut, vec2(gray, 0.5)).rgb;
    }

    // Blend between original and adjusted by opacity and weight mask
    float w = adjWeight;
    if (useWeightMask > 0.5)
        w *= texture(uWeight, uv).r;
    color = mix(origColor, color, clamp(w, 0.0, 1.0));

    fragColor = vec4(color * alpha, alpha);
}
