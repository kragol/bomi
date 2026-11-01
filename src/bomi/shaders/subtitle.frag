#version 440
layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 bboxColor;
};
layout(binding = 1) uniform sampler2D tex;
layout(binding = 2) uniform sampler2D bbox;
void main() {
    vec4 top = texture(tex, texCoord);
    float alpha = texture(bbox, texCoord).a * bboxColor.a * (1.0 - top.a);
    fragColor = top + bboxColor * alpha;
}
