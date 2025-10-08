#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// environmental
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// film plane state - film plane is 3x as wide as it would be otherwise, to accomodate the separate channels
layout( binding = 3, r32ui ) uniform uimage2D filmPlaneImage;

#include "colorspaceConversions.glsl"
#include "biasGain.h"

uniform float slope;
uniform float thresh;
uniform float powerScalar;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// I want some additional UI drawn here... I'd like to see the current exposure level on something like a log scale...
		// info about orientation... this kind of thing would be nice


	// grab the current state of the film plane... try to figure out how to map this to a color
	vec3 tallySample = vec3(
		biasGain( saturate( imageLoad( filmPlaneImage, ivec2( 3, 1 ) * writeLoc + ivec2( 0, 0 ) ).r / powerScalar ), slope, thresh ),
		biasGain( saturate( imageLoad( filmPlaneImage, ivec2( 3, 1 ) * writeLoc + ivec2( 1, 0 ) ).r / powerScalar ), slope, thresh ),
		biasGain( saturate( imageLoad( filmPlaneImage, ivec2( 3, 1 ) * writeLoc + ivec2( 2, 0 ) ).r / powerScalar ), slope, thresh )
	);

	// write the data to the accumulator, which will then be postprocessed and presented
	imageStore( accumulatorTexture, writeLoc, vec4( tallySample, 1.0f ) );
}
