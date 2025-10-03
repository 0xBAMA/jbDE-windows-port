#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// environmental
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// holding the information for the light sources
uniform sampler2D lightICDF;

// film plane state - film plane is 3x as wide as it would be otherwise, to accomodate the separate channels
layout( binding = 3, r32ui ) uniform uimage2D tallyImage;

#include "random.h"
uniform uint seedValue;

// TODO: buffer of light sources... plus method to pick from them
// number of lights...
// per light:
	// spawn distribution + orientation
	// type of light
	// relative chance to be picked... should this do like a prefix sum, kind of thing? for discrete, weighted picking of a light to start from

float getWavelengthForLight( int selectedLight ) {
	return texture( lightICDF, vec2( NormalizedRandomFloat(), ( pickedLight + 0.5f ) / textureSize( lightICDF, 0 ).y ) ).r;
}

// buffer for tinyBVH... can we try doing the TLAS thing this time? I'm not sure what's involved...
	// wrapper function for scene intersection...

void main () {
	// my invocation...
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// seeding the RNG process...
	seed = seedValue + loc.x + 42069 + loc.y * 31415;

	// the role of this shader is to do the intersection tests, and update the ray state structure in the buffer with the closest intersection
		// it will also update the film plane...

	// pick a light source that we are starting from

	// generate a new ray, based on the properties of the selected light...
		// what is my starting position, direction?
		// what is my wavelength?

	// initialize pathtracing state for the given ray initialization

	// tracing paths, for N bounces
	for ( int b = 0; b < maxBounces; b++ ) {

		// scene intersection ( BVH, DDA maybe eventually )

		// ray hits film plane? if so, it tallies energy contribution and dies

		// material evaluation, if the ray is going to continue

		// ...

	}
	// if we fall out without hitting the film plane... nothing special happens
}
