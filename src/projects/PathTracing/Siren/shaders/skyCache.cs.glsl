#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba16f ) uniform image2D skyCache;
layout( rgba8 ) uniform image2D blueNoise; // todo: dithering
uniform float skyTime;

#include "sky.h"

void main () {
	vec3 col = skyColor();

	// write the data to the image
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	imageStore( skyCache, writeLoc, vec4( col, 1.0f ) );
}
