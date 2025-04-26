#version 430

#include "consistentPrimitives.glsl.h"
#include "mathUtils.h"

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

// vertex shader needs to output a primitive ID, to pull parameters from parametersList[]
	// this is used to get the raytraced intersection

in flat uint vofiIndex;
in vec3 vofiPosition;
out vec4 outColor;

uniform vec3 eyePosition;	// location of the viewer... what about ortho case? I think the atlas is ortho
uniform float numPrimitives;

void main () {
	// going to do pixel-stratified jitter for now because I think it resolves nicer...
		// if I do decide to do more TAA stuff in the future, I'll want to make it view-stratified
	
	// I think the thing to do here is to look at the screenspace derivatives, since I'll have them
		// this will be on the interpolated vofiPosition, so we can get that to jitter the far end
		// of the view ray, across the pixel footprint

		// it might be a good thing to be able to do this multiple times and get an alpha result...
			// view stratified jitter makes the occlusion problem simpler, too, I think

	const vec3 eyeVectorToFragment = vofiPosition - eyePosition; // add ddx/ddy logic
	const vec3 rayOrigin = eyePosition;
	const vec3 rayDirection = normalize( eyeVectorToFragment );

	// ray results...
	float result = MAX_DIST_CP;
	vec3 normal = vec3( 0.0f );

	// load parameters and switch on contained primitive...
	parameters_t parameters = parametersList[ vofiIndex ];
	switch( int( parameters.data[ 0 ] ) ) {
	case CAPSULE: {
		result = iCapsule( rayOrigin, rayDirection, normal,
			vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] ), // point A
			vec3( parameters.data[ 4 ], parameters.data[ 5 ], parameters.data[ 6 ] ), // point B
			parameters.data[ 7 ] ); // radius
		} break;

	case CAPPEDCONE: {
		result = iRoundedCone( rayOrigin, rayDirection, normal,
			vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] ), // point A
			vec3( parameters.data[ 4 ], parameters.data[ 5 ], parameters.data[ 6 ] ), // point B
			parameters.data[ 7 ], parameters.data[ 8 ] ); // radii
		} break;

	case ROUNDEDBOX: {
		const vec3 centerPoint = vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] );
		const vec3 scaleFactors = vec3( parameters.data[ 4 ], parameters.data[ 5 ], parameters.data[ 6 ] );
		const float roundingFactor = parameters.data[ 10 ];

		const float theta = parameters.data[ 7 ];
		const float phi = parameters.data[ 8 ];
		// const float psi = parameters.data[ 9 ];

		const mat3 transform = // not sure if this is correct
			Rotate3D( -phi, vec3( 1.0f, 0.0f, 0.0f ) ) *
			Rotate3D( -theta, vec3( 0.0f, 1.0f, 0.0f ) );

		const vec3 rayDirectionAdjusted = ( transform * rayDirection );
		const vec3 rayOriginAdjusted = transform * ( rayOrigin - centerPoint );

		// going to have to figure out what the transforms need to be, in order to intersect with the transformed primitve
		result = iRoundedBox( rayOriginAdjusted, rayDirectionAdjusted, normal, scaleFactors, roundingFactor );

		// is it faster to do this, or to do the euler angle stuff, in inverse? need to profile, at scale
		// const mat3 inverseTransform = ( Rotate3D( theta, vec3( 0.0f, 1.0f, 0.0f ) ) * Rotate3D( phi, vec3( 1.0f, 0.0f, 0.0f ) ) );
		const mat3 inverseTransform = inverse( transform );
		normal = inverseTransform * normal;
		} break;

	case ELLIPSOID: {
		const vec3 centerPoint = vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] );
		const vec3 radii = vec3( parameters.data[ 4 ], parameters.data[ 5 ], parameters.data[ 6 ] );

		const float theta = parameters.data[ 7 ];
		const float phi = parameters.data[ 8 ];
		// const float psi = parameters.data[ 9 ];

		const mat3 transform = // not sure if this is correct
			Rotate3D( -phi, vec3( 1.0f, 0.0f, 0.0f ) ) *
			Rotate3D( -theta, vec3( 0.0f, 1.0f, 0.0f ) );

		const vec3 rayDirectionAdjusted = ( transform * rayDirection );
		const vec3 rayOriginAdjusted = transform * ( rayOrigin - centerPoint );

		// going to have to figure out what the transforms need to be, in order to intersect with the transformed primitve
		result = iEllipsoid( rayOriginAdjusted, rayDirectionAdjusted, normal, radii );
		const mat3 inverseTransform = inverse( transform );
		normal = inverseTransform * normal;
		} break;

	default:
		break;
	}

	if ( result == MAX_DIST_CP ) {
		// outColor = vec4( 1.0f, 0.0f, 0.0f, 1.0f );
		discard; // nohit condition
	} else {
		// write deferred surface results (depth, id, normal... what else?)
		// outColor = vec4( 0.0f, 1.0f, 0.0f, 1.0f );
		outColor = vec4( ( normal + vec3( 1.0f ) ) / 2.0f, 1.0f );

		// writing correct depths
		const vec4 projectedPosition = viewTransform * vec4( rayOrigin + result * rayDirection, 1.0f );
		gl_FragDepth = ( projectedPosition.z / projectedPosition.w + 1.0f ) * 0.5f;

	}
}