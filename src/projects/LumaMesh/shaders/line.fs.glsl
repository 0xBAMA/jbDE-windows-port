#version 450

// interpolated vertex data
in vec4 position;
in vec4 color;

// fragment output
out float depth;
out vec4 fragColor;

void main () {
    fragColor = color;
}