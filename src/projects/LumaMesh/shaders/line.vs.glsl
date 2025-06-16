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

// time offset, in terms of frame... we have to use this to animate, because this shader will be called many times per frame
uniform int frame;

#include "noise.h"

void main () {
    vec4 imageData = imageLoad( displacementImage, pixel );
    vec4 p = vec4( position, ( imageData.a - 0.5f ) / 5.0f, 1.0f );
    p = transform * ( p + vec4( curlNoise( 0.1f * p.xyz + vec3( frame / 70000.0f, frame / 100000.0f, frame / 10000.0f ) ) * 0.01f, 1.0f ) );
    // p = transform * ( p * vec4( 1.0f, 1.0f, perlinfbm( p.xyz + vec3( frame / 7000.0f, frame / 10000.0f, frame / 1000.0f ), 2.0f, 5 ) * 2.5f, 1.0f ) );
    // p = transform * ( p + vec4( 0.0f, 0.0f, perlinfbm( p.xyz + vec3( 0.0f, 0.0f, frame / 1000.0f ), 4.0f, 5 ) * 0.1f, 0.0f ) );
    gl_Position = apiPosition = p;
    color = vec4( imageData.rgb, 1.0f );
}