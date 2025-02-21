#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

#include "mathUtils.h"

#define SPHERE 0
#define CAPSULE 1
#define ROUNDEDBOX 2

// ===================================================================================================
// output from the bounds stage
// ===================================================================================================
layout( binding = 0, std430 ) buffer transformsBuffer {
	mat4 transforms[];
};

// ===================================================================================================
// input to the bounds stage
// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 1, std430 ) buffer parametersBuffer {
	parameters_t parametersList[];
};

// ===================================================================================================
// capsule bounds - primary axis is between endpoints, then some cross products to get the others
// ===================================================================================================
mat4 CapsuleBounds( parameters_t parameters ) {
	const vec3 pointA = vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] );
	const vec3 pointB = vec3( parameters.data[ 5 ], parameters.data[ 6 ], parameters.data[ 7 ] );
	const vec3 midPoint = ( pointA + pointB ) / 2.0f;
	const float radius = parameters.data[ 4 ];

	// compute the primary axis between pointA and pointB, and is of length 2 * radius + length( A - B )
	const vec3 d = ( pointA - pointB );
	const vec3 refVector = 0.5f * d * ( ( length( d ) + 2.0f * radius ) / length( d ) );

	vec3 displacementVector = normalize( d );
	const vec3 up = vec3( 1.0f, 1.0f, 1.0f );
	if ( dot( normalize( displacementVector ), up ) > 0.999f ) {
		// is it along some reference vector? if yes, we need to do the math with some linearly independent vector
		displacementVector = normalize( vec3( 1.0f, 1.0f, 0.0f ) );
	}

	// box transform needs to be constructed such that:
		// cross product to get first orthogonal vector
			// second axis is that first orthogonal vector, of length radius
	const vec3 firstOrthoVec = radius * normalize( cross( up, displacementVector ) );
		// cross product again to get second orthogonal vector
			// third axis is that second orthogonal vector, of length radius
	const vec3 secondOrthoVec = radius * normalize( cross( displacementVector, firstOrthoVec ) );

	return mat4(
		refVector, 0.0f,
		secondOrthoVec, 0.0f,
		firstOrthoVec, 0.0f,
		midPoint, 1.0f
	);
}

// ===================================================================================================
// rounded box bounds - what does this need to look like?
// ===================================================================================================
mat4 RoundedBoxBounds ( parameters_t parameters ) {
	const vec3 centerPoint = vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] );
	const vec3 scaleFactors = vec3( parameters.data[ 5 ], parameters.data[ 6 ], parameters.data[ 7 ] );
	const float packedEuler = parameters.data[ 4 ];
	const float roundingFactor = parameters.data[ 8 ];

	const float theta = fract( packedEuler ) * 2.0f * pi;
	const float phi = ( floor( packedEuler ) / 255.0f ) * ( pi / 2.0f );

	// need to apply euler angles, angle axis
	const mat3 base = Rotate3D( theta, vec3( 0.0f, 1.0f, 0.0f ) )
		* Rotate3D( phi, vec3( 1.0f, 0.0f, 0.0f ) )
		* mat3( scaleFactors.x + roundingFactor, 0.0f, 0.0f,
				0.0f, scaleFactors.y + roundingFactor, 0.0f,
				0.0f, 0.0f, scaleFactors.z + roundingFactor );

	mat4 current = mat4(
		base[ 0 ][ 0 ], base[ 0 ][ 1 ], base[ 0 ][ 2 ], 0.0f,
		base[ 1 ][ 0 ], base[ 1 ][ 1 ], base[ 1 ][ 2 ], 0.0f,
		base[ 2 ][ 0 ], base[ 2 ][ 1 ], base[ 2 ][ 2 ], 0.0f,
		centerPoint, 1.0f
	);

	return current;
}

// ===================================================================================================
void main () {

	// getting this object's parameters
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;
	parameters_t parameters = parametersList[ index ];
	mat4 result;

	// changing behavior, based on the contained primitive
	switch( int( parameters.data[ 0 ] ) ) {

		case CAPSULE:
			// compute the capsule bounding box
			result = CapsuleBounds( parameters );
			break;

		case ROUNDEDBOX:
			// bounding box for the rounded cube
			result = RoundedBoxBounds( parameters );
			break;

		default:
			break;
	}

	// write the result to the other SSBO
	transforms[ index ] = result;
}