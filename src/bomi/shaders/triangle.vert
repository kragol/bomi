#version 440
layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 0) out vec4 color;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    mat4 scaler;
};
void main() {
    color = aColor;
    gl_Position = scaler * qt_Matrix * aPosition;
}
