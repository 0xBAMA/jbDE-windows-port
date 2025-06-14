#version 450

// interpolated vertex data
in vec4 position;
in vec4 color;

// output fragment color
out vec4 fragColor;

void main () {
    fragColor = color;
}