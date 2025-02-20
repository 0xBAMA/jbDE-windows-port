#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 2, r32ui ) uniform uimage2D ifsAccumulatorR;
layout( binding = 3, r32ui ) uniform uimage2D ifsAccumulatorG;
layout( binding = 4, r32ui ) uniform uimage2D ifsAccumulatorB;

#include "colorRamps.glsl.h"
#include "mathUtils.h"

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// clear the channels for each buffer
	imageStore( ifsAccumulatorR, writeLoc, uvec4( 0 ) );
	imageStore( ifsAccumulatorG, writeLoc, uvec4( 0 ) );
	imageStore( ifsAccumulatorB, writeLoc, uvec4( 0 ) );
}
