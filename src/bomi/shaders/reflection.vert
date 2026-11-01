#version 440
// BarVisualizer.qml's mirrored, tilted reflection.
layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec2 coord;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float tilt;
};
void main() {
    coord = qt_MultiTexCoord0;
    vec4 vtx = qt_Vertex;
    vtx.x -= tilt * coord.y;
    coord.y = 1.0 - coord.y;
    gl_Position = qt_Matrix * vtx;
}
