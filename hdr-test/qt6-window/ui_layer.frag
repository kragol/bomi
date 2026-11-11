#version 440
// --ui layer: the UI root is rendered into a layer and this converts it to
// PQ before Qt blends it over the video. The blend then happens on PQ values.
//
// Qt Quick draws the UI in sRGB as for an SDR window; KWin shows SDR as gamma
// 2.2 with white at its reference luminance, which it maps PQ's 203 nits to.
// So UI white goes to 203 nits.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};
layout(binding = 1) uniform sampler2D source;

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

void main()
{
    vec4 c = texture(source, qt_TexCoord0); // premultiplied
    vec3 rgb = c.a > 0.0 ? c.rgb / c.a : vec3(0.0);
    vec3 pq = pqFromNits(BT709_TO_BT2020 * pow(max(rgb, 0.0), vec3(2.2)) * SDR_WHITE);
    fragColor = vec4(pq * c.a, c.a) * qt_Opacity;
}
