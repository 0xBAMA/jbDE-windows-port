#version 450

// interpolated vertex data
in vec4 position;
in vec4 color;

// fragment output
out float depth;
out vec4 fragColor;

// #include "random.h"
uniform int wangSeed;

void main () {
    // seed = wangSeed + int( gl_FragCoord.x ) * 420 + int( gl_FragCoord.y ) * 31415;

    // if ( NormalizedRandomFloat() < 0.9f )
        // discard;

    fragColor = color;
}