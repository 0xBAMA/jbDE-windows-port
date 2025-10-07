#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// environmental
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// holding the information for the light sources
uniform sampler2D lightICDF;

// film plane state - film plane is 3x as wide as it would be otherwise, to accomodate the separate channels
layout( binding = 3, r32ui ) uniform uimage2D filmPlaneImage;

#include "random.h"
uniform uint seedValue;

// TODO: buffer of light sources... plus method to pick from them

// ===================================================================================================
struct lightSpecGPU {
	// less than ideal way to do this...
	vec4 typeVec;		// emitter type, LUT type, 0, 0
	vec4 parameters0;	// varies
	vec4 parameters1;	// varies
	vec4 pad;			// zeroes

	// so, everything we need to know:
		// type of light ( 1 float -> int )
		// type of emitter ( 1 float -> int )
		// emitter parameterization
			// varies by emitter...

	// Parameters:
		// point emitter has 1x vec3 parameter
		// cauchy beam has 2x vec3 parameters and 1x float parameter
		// laser disk has 2x vec3 parameters and 1x float parameter
		// uniform line emitter has 2x vec3 parameters...
};

layout( binding = 0, std430 ) buffer lightBuffer {
	int lightIStructure[ 1024 ]; // we uniformly sample an index out of this list of 1024 to know which light we want to pick...
	lightSpecGPU lightList[]; // we do not need to know how many lights exist, because it is implicitly encoded in the importance structure's indexing
};


// ===================================================================================================
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
