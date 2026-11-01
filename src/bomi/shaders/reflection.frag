#version 440
layout(location = 0) in vec2 coord;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float tilt;
};
layout(binding = 1) uniform sampler2D src;
void main() {
    vec4 tex = texture(src, coord);
    fragColor = tex * qt_Opacity * coord.y * coord.y * 0.6;
}
