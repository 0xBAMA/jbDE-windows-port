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

};

layout( binding = 0, std430 ) buffer agentData {
	agent_t data[];
};

#include "mathUtils.h"

// random utilites
uniform int wangSeed;
#include "random.h"

uniform int initMode;
uniform uint numAgents;

void main () {

	const uvec3 globalDims = gl_NumWorkGroups * gl_WorkGroupSize;
	const int index = int(
		gl_GlobalInvocationID.z * globalDims.x * globalDims.y +
		gl_GlobalInvocationID.y * globalDims.x +
		gl_GlobalInvocationID.x );

	if ( index < numAgents ) {
		// initialize the rng
		seed = wangSeed + 69420 * index;

		agent_t a = data[ index ];

// eventually do something to switch on initMode, for different behavior

	// setting position inside the volume
		const vec3 dims = vec3( imageSize( current ).xyz );
		switch ( initMode ) {
			case 0: // slab at center, trimmed margins
				a.position.xyz = vec3(
					RangeRemapValue( NormalizedRandomFloat(), 0.0f, 1.0f, 100.0f, dims.x - 100.0f ),
					RangeRemapValue( NormalizedRandomFloat(), 0.0f, 1.0f, 100.0f, dims.y - 100.0f ),
					RangeRemapValue( NormalizedRandomFloat(), 0.0f, 1.0f, 100.0f, dims.z - 100.0f )
				);
				break;

			case 1: // corner wrap blob
				a.position.xyz = NormalizedRandomFloat() * RandomUnitVector();
				break;

			case 2: // centered blob
				a.position.xyz = NormalizedRandomFloat() * RandomUnitVector() + dims / 2.0f;
				break;

			default:
				break;
		}

	// setting orientation - same as what was going on, on the CPU
		// initial basis
		a.basisX.xyz = vec3( 1.0f, 0.0f, 0.0f );
		a.basisY.xyz = vec3( 0.0f, 1.0f, 0.0f );
		a.basisZ.xyz = vec3( 0.0f, 0.0f, 1.0f );

		// apply some random rotations
		mat3 rot = Rotate3D( tau * NormalizedRandomFloat(), a.basisX.xyz );
		a.basisX.xyz = rot * a.basisX.xyz;
		a.basisY.xyz = rot * a.basisY.xyz;
		a.basisZ.xyz = rot * a.basisZ.xyz;
		rot = Rotate3D( tau * NormalizedRandomFloat(), a.basisY.xyz );
		a.basisX.xyz = rot * a.basisX.xyz;
		a.basisY.xyz = rot * a.basisY.xyz;
		a.basisZ.xyz = rot * a.basisZ.xyz;
		rot = Rotate3D( tau * NormalizedRandomFloat(), a.basisZ.xyz );
		a.basisX.xyz = rot * a.basisX.xyz;
		a.basisY.xyz = rot * a.basisY.xyz;
		a.basisZ.xyz = rot * a.basisZ.xyz;

		// update the state values in the buffer...
		data[ index ].position.xyz = a.position.xyz;
		data[ index ].basisX.xyz = a.basisX.xyz;
		data[ index ].basisY.xyz = a.basisY.xyz;
		data[ index ].basisZ.xyz = a.basisZ.xyz;
	}
}