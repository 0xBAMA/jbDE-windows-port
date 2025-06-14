#version 450

// the image which displaces and colors the lines
layout( binding = 0, rgba32f ) uniform image2D displacementImage;

// vertex data - base position + pixel index for displacement
in vec2 position;
in ivec2 pixel;

// output values to solve for
out vec4 apiPosition;
out vec4 color;

// rotation, scaling, translation + lookat, perspective
uniform mat4 transform;

void main () {
    vec4 imageData = imageLoad( displacementImage, pixel );
    vec4 p = transform * vec4( position, ( imageData.a - 0.5f ) / 10.0f, 1.0f );

    gl_Position = apiPosition = p;
    color = vec4( imageData.rgb, 1.0f );
}