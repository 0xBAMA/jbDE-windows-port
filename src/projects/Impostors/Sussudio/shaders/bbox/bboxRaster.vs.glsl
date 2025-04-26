#version 430

#define CAPSULE		0
#define CAPPEDCONE	1
#define ROUNDEDBOX	2
#define ELLIPSOID	3

// ===================================================================================================
// input to the bounds stage, used again by fragment raytracing
// ===================================================================================================
uniform mat4 viewTransform;
// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 0, std430 ) buffer parametersBuffer {
	parameters_t parametersList[];
};

// ===================================================================================================
// output from the bounds stage
// ===================================================================================================
layout( binding = 1, std430 ) buffer transformsBuffer {
	mat4 transforms[];
};

// uint has to be flat, single value across primitive
out flat uint vofiIndex;
out vec3 vofiPosition;

#include "mathUtils.h"
#include "cubeVerts.h"

void main () {
	vofiIndex = gl_VertexID / 36;
	vec4 position = transforms[ vofiIndex ] * vec4( CubeVert( gl_VertexID % 36 ), 1.0f );
	vofiPosition = position.xyz;
	gl_Position = viewTransform * position;
}