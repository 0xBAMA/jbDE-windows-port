#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// ===================================================================================================
// buffer for the particle locations
// ===================================================================================================
layout( binding = 0, std430 ) buffer particleBuffer {
	vec4 particleList[];
};

// ===================================================================================================
// buffer for anchoring
// ===================================================================================================
layout( binding = 0, r32ui ) uniform uimage3D DLATexture;

// ===================================================================================================
#include "random.h"
uniform int randomSeed;

#include "noise.h"
// ===================================================================================================
// uniform parameters
// wind direction?
// jitter magnitude?
// ===================================================================================================

void main () {

	// load current particle location
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;

	// init RNG seeding
	seed = randomSeed + 69420 * index;

	// apply a jitter to the location
	const float jitterMagnitude = 2.0f * 4.0f;
	vec4 oldPosition = particleList[ index ];
	// vec4 newPosition = oldPosition + vec4( curlNoise( oldPosition.xyz * 0.0001f ) * vec3( jitterMagnitude * ( NormalizedRandomFloat() - 0.5f ), jitterMagnitude * ( NormalizedRandomFloat() - 0.5f ), jitterMagnitude * ( NormalizedRandomFloat() - 0.5f ) ), 0.0f );
	vec4 newPosition = oldPosition + vec4( jitterMagnitude * ( NormalizedRandomFloat() - 0.5f ), jitterMagnitude * ( NormalizedRandomFloat() - 0.5f ), jitterMagnitude * ( NormalizedRandomFloat() - 0.5f ), 0.0f );

	// apply some wind
	// newPosition.xyz += vec3( 0.0f, 0.0f, 5.0f );
	newPosition.xyz += curlNoise( newPosition.xyz * 0.05f ) * 0.2f;

	// quantized position
	ivec3 pos = ivec3( newPosition );

	// state flags
	bool anchored = false;
	bool oob = any( greaterThanEqual( pos, ivec3( imageSize( DLATexture ).xyz ) ) ) || any( lessThan( pos, ivec3( 0 ) ) );
	
	// close enough to anchor?
		// sum of reads will be nonzero
	if ( !oob ) {
		if ( 0 !=
			// corner neighbors
			imageLoad( DLATexture, pos + ivec3( -1, -1, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  1, -1, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3( -1,  1, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  1,  1, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3( -1, -1,  1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  1, -1,  1 ) ).r +
			imageLoad( DLATexture, pos + ivec3( -1,  1,  1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  1,  1,  1 ) ).r +
			// edge neighbors
			imageLoad( DLATexture, pos + ivec3(  1,  0, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  0, -1, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  0,  1, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3( -1,  0, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  1,  0,  1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  0, -1,  1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  0,  1,  1 ) ).r +
			imageLoad( DLATexture, pos + ivec3( -1,  0,  1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  1,  1,  0 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  1, -1,  0 ) ).r +
			imageLoad( DLATexture, pos + ivec3( -1,  1,  0 ) ).r +
			imageLoad( DLATexture, pos + ivec3( -1, -1,  0 ) ).r +
			// face neighbors
			imageLoad( DLATexture, pos + ivec3( -1,  0,  0 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  1,  0,  0 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  0, -1,  0 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  0,  1,  0 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  0,  0, -1 ) ).r +
			imageLoad( DLATexture, pos + ivec3(  0,  0,  1 ) ).r
		) {
			anchored = true;
			imageAtomicAdd( DLATexture, pos, 1 ); // nonzero value says cell is anchored
		}
	}

	// anchored or out of bounds, respawn location
	if ( anchored || oob ) {
		newPosition.x = NormalizedRandomFloat() * imageSize( DLATexture ).x;
		newPosition.y = NormalizedRandomFloat() * imageSize( DLATexture ).y;
		newPosition.z = NormalizedRandomFloat() * imageSize( DLATexture ).z;
	}

	// write location back to the buffer
	particleList[ index ] = newPosition;
}
