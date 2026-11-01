#version 440
layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec2 aFrameTexCoord;
layout(location = 2) in vec2 aOsdTexCoord; // part of the vertex layout, unused
layout(location = 0) out vec2 frameTexCoord;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};
void main() {
    frameTexCoord = aFrameTexCoord;
    gl_Position = qt_Matrix * aPosition;
}
