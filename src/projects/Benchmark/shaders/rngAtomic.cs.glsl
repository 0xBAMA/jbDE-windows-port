#version 430
layout( local_size_x = 4, local_size_y = 4, local_size_z = 1 ) in;

// buffer for atomic writes
layout( binding = 0, std430 ) buffer dataBuffer { uint values[ 1024 ]; };

// wang RNG
uniform int wangSeed;
#include "random.h"

void main () {
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	seed = wangSeed + 42069 * location.x + 451 * location.y;

    // generate 1k random numbers + increment associated bin
    for ( int i = 0; i < 1024; i++ ) {
        // generating a new float
        float value = NormalizedRandomFloat();

        // round so that we know where we're writing
        int bin = clamp( int( 1024 * value ), 0, 1023 );
        atomicAdd( values[ bin ], 1 );
    }
}
