#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// environmental
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// film plane state - film plane is 3x as wide as it would be otherwise, to accomodate the separate channels
layout( binding = 3, r32ui ) uniform uimage2D filmPlaneImage;

#include "colorspaceConversions.glsl"

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// grab the current state of the film plane... try to figure out how to map this to a color
	uvec3 tallySample = uvec3(
		imageLoad( filmPlaneImage, ivec2( 3, 1 ) * writeLoc + ivec2( 0, 0 ) ).r,
		imageLoad( filmPlaneImage, ivec2( 3, 1 ) * writeLoc + ivec2( 1, 0 ) ).r,
		imageLoad( filmPlaneImage, ivec2( 3, 1 ) * writeLoc + ivec2( 2, 0 ) ).r
	);

	// write the data to the accumulator, which will then be postprocessed and presented
	imageStore( accumulatorTexture, writeLoc, vec4( rgb_to_srgb( xyz_to_rgb( vec3( tallySample ) / 1000.0f ) ), 1.0f ) );
}
