#version 450

layout(location = 0) out vec2 coordinate;

vec2 positions[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, 1.0)
);

layout(binding = 0) uniform borders {
    float x_left;
    float x_right;
    float y_top;
    float y_bottom;
} b;


void main() {
    vec2 coordinates[4] = vec2[](
        vec2(b.x_left, b.y_bottom),
        vec2(b.x_right, b.y_bottom),
        vec2(b.x_left, b.y_top),
        vec2(b.x_right, b.y_top)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    coordinate = coordinates[gl_VertexIndex];
}
