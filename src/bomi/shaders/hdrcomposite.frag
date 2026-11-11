#version 440
// HDR output: the window is PQ/BT.2020, so everything Qt Quick draws in sRGB
// is composited here (see MainWindow::Data::updateHdr()). The video arrives
// in PQ from mpv; the skin and the dialog overlay arrive premultiplied sRGB
// and are blended over it the way an SDR window blends them -- on gamma 2.2
// values with SDR white at 1.0, i.e. 203 nits -- then encoded PQ. Where the
// UI is transparent the video passes through untouched. With darker set, the
// UI is converted to PQ first and blended on PQ values instead, which makes
// translucent panels much darker.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 videoRect; // the video item in the window: x, y, w, h in 0..1
    vec4 videoTex;  // its texture coordinates: top-left x, y, then w, h (flips negative)
    float darker;
    float uiOpacity;
    float overlayOpacity;
};
layout(binding = 1) uniform sampler2D video;
layout(binding = 2) uniform sampler2D ui;
layout(binding = 3) uniform sampler2D overlay;

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

// sRGB UI colour -> linear BT.2020 relative to SDR white
vec3 linearFromUi(vec3 c)
{
    return BT709_TO_BT2020 * pow(max(c, 0.0), vec3(2.2));
}

// One premultiplied UI layer over a PQ picture
vec3 over(vec3 pq, vec4 layer)
{
    if (layer.a <= 0.0)
        return pq;
    vec3 rgb = layer.rgb / layer.a;
    if (darker > 0.5)
        return mix(pq, pqFromNits(linearFromUi(rgb) * SDR_WHITE), layer.a);
    vec3 base = pow(nitsFromPq(pq) / SDR_WHITE, vec3(1.0 / 2.2));
    vec3 g = mix(base, pow(linearFromUi(rgb), vec3(1.0 / 2.2)), layer.a);
    return pqFromNits(pow(g, vec3(2.2)) * SDR_WHITE);
}

void main()
{
    vec2 v = (qt_TexCoord0 - videoRect.xy) / videoRect.zw;
    vec3 pq = all(greaterThanEqual(v, vec2(0.0))) && all(lessThanEqual(v, vec2(1.0)))
            ? texture(video, videoTex.xy + v * videoTex.zw).rgb : vec3(0.0);
    pq = over(pq, texture(ui, qt_TexCoord0) * uiOpacity);
    pq = over(pq, texture(overlay, qt_TexCoord0) * overlayOpacity);
    fragColor = vec4(pq, 1.0) * qt_Opacity;
}
