#version 450

// the image which displaces and colors the lines
layout( binding = 0, rgba32f ) uniform image2D displacementImage;

// vertex data - base position + pixel index for displacement
in vec2 position;
in ivec2 pixel;

// output values to solve for
out vec4 apiPosition;
out vec4 color;

// rotation
uniform vec3 xBasis;
uniform vec3 yBasis;
uniform vec3 zBasis;
uniform float scale;

void main () {
    vec4 imageData = imageLoad( displacementImage, pixel );
    vec3 p = scale * vec3( position, ( imageData.a - 0.5f ) / 10.0f );
    p = p.x * xBasis + p.y * yBasis + p.z * zBasis;
    p.x /= ( 3840.0f / 2160.0f );
    p.z /= 3.0f;

    gl_Position = apiPosition = vec4( p, 1.0f );
    color = vec4( imageData.rgb, 1.0f );
}