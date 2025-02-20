#version 430 core
layout( local_size_x = 1024, local_size_y = 1, local_size_z = 1 ) in;

layout( r32ui ) uniform uimage3D current;

struct agent_t {

	// location, extra float available
	vec4 position;

	// direction now needs to specify the basis vectors to construct sample positions
	vec4 basisX;
	vec4 basisY;
	vec4 basisZ;

	// other info? parameters per-agent?

};

layout( binding = 0, std430 ) buffer agentData {
	agent_t data[];
};

uniform float stepSize;
uniform float senseAngle;
uniform float senseDistance;
uniform float turnAngle;
uniform uint depositAmount;
uniform uint numAgents;

#include "mathUtils.h"

vec3 wrapPosition ( vec3 pos ) {
	const ivec3 is = imageSize( current ).xyz;
	if ( pos.x >= is.x ) pos.x -= is.x;
	if ( pos.x < 0.0f ) pos.x += is.x;
	if ( pos.y >= is.y ) pos.y -= is.y;
	if ( pos.y < 0.0f ) pos.y += is.y;
	if ( pos.z >= is.z ) pos.z -= is.z;
	if ( pos.z < 0.0f ) pos.z += is.z;
	return pos;
}

// random utilites
uniform int wangSeed;
#include "random.h"

void main () {

	const uvec3 globalDims = gl_NumWorkGroups * gl_WorkGroupSize;
	const int index = int(
		gl_GlobalInvocationID.z * globalDims.x * globalDims.y +
		gl_GlobalInvocationID.y * globalDims.x +
		gl_GlobalInvocationID.x );

	if ( index < numAgents ) {
		// initialize the rng
		seed = wangSeed + 69420 * index;

		// figure out sense positions
		agent_t a = data[ index ];

		vec3 forwards = a.basisZ.xyz;

		vec3 sensePositions[ 4 ];
		vec3 movePositions[ 4 ];

		// the forward step
		sensePositions[ 0 ] = a.position.xyz + forwards * senseDistance;
		movePositions[ 0 ] = a.position.xyz + forwards * stepSize;

		// we are starting with one vector tipped up, and then rolling about z
		vec3 tippedUpSense = Rotate3D( senseAngle, a.basisX.xyz ) * forwards * senseDistance;
		vec3 tippedUpMove = Rotate3D( turnAngle, a.basisX.xyz ) * forwards * stepSize;

		for ( int i = 1; i <= 3; i++ ) {
			sensePositions[ i ] = a.position.xyz + Rotate3D( 2.0f * pi * float( i ) / 3.0f, a.basisZ.xyz ) * tippedUpSense;
			movePositions[ i ] = a.position.xyz + Rotate3D( 2.0f * pi * float( i ) / 3.0f, a.basisZ.xyz ) * tippedUpMove;
		}

		// take sense samples
		uint bufferSamples[ 4 ] = uint[ 4 ](
			imageLoad( current, ivec3( sensePositions[ 0 ] ) ).r,
			imageLoad( current, ivec3( sensePositions[ 1 ] ) ).r,
			imageLoad( current, ivec3( sensePositions[ 2 ] ) ).r,
			imageLoad( current, ivec3( sensePositions[ 3 ] ) ).r
		);

		// turning 

		// determine the max sampled value
		uint maxSample = max( max( bufferSamples[ 0 ], bufferSamples[ 1 ] ), max( bufferSamples[ 2 ], bufferSamples[ 3 ] ) );

		// if it is the same value across all samples, we need to pick a random direction
		int selected = 0;
		if ( maxSample == bufferSamples[ 0 ] &&
			maxSample == bufferSamples[ 1 ] &&
			maxSample == bufferSamples[ 2 ] &&
			maxSample == bufferSamples[ 3 ] ) { // pick from all samples
				selected = int( ( NormalizedRandomFloat() - 0.001f ) * 4.0f );
		} else if ( maxSample == bufferSamples[ 1 ] &&
			maxSample == bufferSamples[ 2 ] &&
			maxSample == bufferSamples[ 3 ] ) { // pick from flanking samples
				selected = int( ( NormalizedRandomFloat() - 0.001f ) * 3.0f ) + 1;
		} else if ( maxSample == bufferSamples[ 0 ] ) { // just go forwards
			selected = 0;
		} else if ( maxSample == bufferSamples[ 1 ] ) { // or towards the flanking directions
			selected = 1;
		} else if ( maxSample == bufferSamples[ 2 ] ) {
			selected = 2;
		} else if ( maxSample == bufferSamples[ 3 ] ) {
			selected = 3;
		}

		// processing required changes to the basis vectors
		selected = clamp( selected, 0, 3 );
		if ( selected == 0 ) {
			// no changes needed, we are continuing forwards
		} else {
			// it's the transforms from before, to update the basis vectors
			mat3 transform = Rotate3D( 2.0f * pi * float( selected ) / 3.0f, a.basisZ.xyz ) * Rotate3D( senseAngle, a.basisX.xyz );
			a.basisX.xyz = transform * a.basisX.xyz;
			a.basisY.xyz = transform * a.basisY.xyz;
			a.basisZ.xyz = transform * a.basisZ.xyz;
		}

		// take a step
		vec3 newPosition = wrapPosition( movePositions[ selected ] );

		// update the state values in the buffer...
		const bool writeBack = true;
		if ( writeBack ) {
			data[ index ].position.xyz = newPosition;
			data[ index ].basisX.xyz = a.basisX.xyz;
			data[ index ].basisY.xyz = a.basisY.xyz;
			data[ index ].basisZ.xyz = a.basisZ.xyz;
		}

		// deposit at the location currently stored
		imageAtomicAdd( current, ivec3( newPosition ), depositAmount );
	}
}