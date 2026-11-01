#version 440
// Shared by every textured item: SimpleTextureItem and its subclasses, and
// the subtitle renderer.
layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 0) out vec2 texCoord;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};
void main() {
    texCoord = aTexCoord;
    gl_Position = qt_Matrix * aPosition;
}
