#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) uniform image2D bufferImage;

uniform float t;
uniform int rngSeed;

float rayPlaneIntersect ( in vec3 rayOrigin, in vec3 rayDirection ) {
	const vec3 normal = vec3( 0.0f, 1.0f, 0.0f );
	const vec3 planePt = vec3( 0.0f, 0.0f, 0.0f ); // not sure how far down this should be
	return -( dot( rayOrigin - planePt, normal ) ) / dot( rayDirection, normal );
}

#include "mathUtils.h"
#include "random.h"
#include "spectrumXYZ.h"

// drawing inspiration from https://www.shadertoy.com/view/M3jcDW

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	seed = rngSeed + 42069 * loc.x + 31415 * loc.y;


	// result is then averaged, in XYZ space

}