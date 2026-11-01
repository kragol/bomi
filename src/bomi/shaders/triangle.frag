#version 440
layout(location = 0) in vec4 color;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    mat4 scaler;
};
void main() {
    fragColor = color * qt_Opacity;
}
