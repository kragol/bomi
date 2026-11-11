#version 440
// --ui composite: one full-window pass that takes the video (PQ, BT.2020) and
// the UI layer (sRGB, premultiplied) and blends them the way an SDR window is
// blended -- on gamma 2.2 values with SDR white at 1.0 -- then encodes PQ.
// Video above SDR white is simply > 1.0 there, so HDR highlights pass through
// where the UI is transparent.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};
layout(binding = 1) uniform sampler2D video;
layout(binding = 2) uniform sampler2D ui;

const float SDR_WHITE = 203.0;
// BT.709 primaries in BT.2020 linear RGB (ITU-R BT.2087), column-major
const mat3 BT709_TO_BT2020 = mat3(0.6274, 0.0691, 0.0164,
                                  0.3293, 0.9195, 0.0880,
                                  0.0433, 0.0114, 0.8956);
const float M1 = 2610.0 / 16384.0, M2 = 2523.0 / 4096.0 * 128.0;
const float C1 = 3424.0 / 4096.0, C2 = 2413.0 / 4096.0 * 32.0, C3 = 2392.0 / 4096.0 * 32.0;

vec3 pqFromNits(vec3 nits)
{
    vec3 y = pow(clamp(nits / 10000.0, 0.0, 1.0), vec3(M1));
    return pow((C1 + C2 * y) / (1.0 + C3 * y), vec3(M2));
}

vec3 nitsFromPq(vec3 e)
{
    vec3 p = pow(clamp(e, 0.0, 1.0), vec3(1.0 / M2));
    return 10000.0 * pow(max(p - C1, 0.0) / (C2 - C3 * p), vec3(1.0 / M1));
}

void main()
{
    vec3 v = texture(video, qt_TexCoord0).rgb;
    vec4 u = texture(ui, qt_TexCoord0); // premultiplied
    if (u.a <= 0.0) {
        fragColor = vec4(v, 1.0) * qt_Opacity; // untouched where there is no UI
        return;
    }
    vec3 uiRgb = u.rgb / u.a;
    // Both in BT.2020 gamma 2.2, relative to SDR white
    vec3 uiG = pow(BT709_TO_BT2020 * pow(max(uiRgb, 0.0), vec3(2.2)), vec3(1.0 / 2.2));
    vec3 videoG = pow(nitsFromPq(v) / SDR_WHITE, vec3(1.0 / 2.2));
    vec3 g = mix(videoG, uiG, u.a);
    fragColor = vec4(pqFromNits(pow(g, vec3(2.2)) * SDR_WHITE), 1.0) * qt_Opacity;
}
