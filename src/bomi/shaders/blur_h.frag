#version 440
// One pass of SimpleBlur.qml's separable 5-tap blur.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float dx;
};
layout(binding = 1) uniform sampler2D src;
void main() {
    vec2 step = vec2(dx, 0.0);
    vec4 tex = texture(src, qt_TexCoord0 - 2.0*step) * 0.064;
    tex += texture(src, qt_TexCoord0 - step) * 0.244;
    tex += texture(src, qt_TexCoord0) * 0.383;
    tex += texture(src, qt_TexCoord0 + step) * 0.244;
    tex += texture(src, qt_TexCoord0 + 2.0*step) * 0.064;
    fragColor = tex;
}
