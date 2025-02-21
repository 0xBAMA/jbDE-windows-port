#version 430

in flat uint vofiIndex;
in vec3 vofiPosition;
out vec4 outColor;

uniform vec3 eyePosition;	// location of the viewer
uniform float numPrimitives;

#include "colorRamps.glsl.h"
#include "consistentPrimitives.glsl.h"
#include "mathUtils.h"

// ===================================================================================================
uniform mat4 viewTransform;
// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 1, std430 ) buffer parametersBuffer {
	parameters_t parametersList[];
};

// TAA resources:
// https://ziyadbarakat.wordpress.com/2020/07/28/temporal-anti-aliasing-step-by-step/
// https://sugulee.wordpress.com/2021/06/21/temporal-anti-aliasingtaa-tutorial/

out float gl_FragDepth;
layout( location = 0 ) out uvec4 primitiveID;

// visalizing the fragments that get drawn
// #define SHOWDISCARDS

void main() {
	// eye parameter
	const vec3 eyeVectorToFragment = vofiPosition - eyePosition;
	const vec3 rayOrigin = eyePosition;
	const vec3 rayDirection = normalize( eyeVectorToFragment );

	// float result = iRoundedCone( rayOrigin - centerPoint, rayDirection, normal, vec3( -0.9f ), vec3( 0.7f ), 0.1f, 0.3f );
	// float result = iRoundedBox( rayOrigin - centerPoint, rayDirection, normal, vec3( 0.9f ), 0.1f );

	float result = MAX_DIST_CP;
	vec3 normal = vec3( 0.0f );
	parameters_t parameters = parametersList[ vofiIndex ];
	const int primitiveType = int( parameters.data[ 0 ] );

	#define SPHERE 0
	#define CAPSULE 1
	#define ROUNDEDBOX 2

	// intersecting with the contained primitive
	switch ( primitiveType ) {
		case CAPSULE:
			result = iCapsule( rayOrigin, rayDirection, normal,
				vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] ), // point A
				vec3( parameters.data[ 5 ], parameters.data[ 6 ], parameters.data[ 7 ] ), // point B
				parameters.data[ 4 ] ); // radius
			break;

		case ROUNDEDBOX:
			const vec3 centerPoint = vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] );
			const float packedEuler = parameters.data[ 4 ];
			const vec3 scaleFactors = vec3( parameters.data[ 5 ], parameters.data[ 6 ], parameters.data[ 7 ] );
			const float roundingFactor = parameters.data[ 8 ];

			const float theta = fract( packedEuler ) * 2.0f * pi;
			const float phi = ( floor( packedEuler ) / 255.0f ) * ( pi / 2.0f );

			const mat3 transform = ( Rotate3D( -phi, vec3( 1.0f, 0.0f, 0.0f ) ) * Rotate3D( -theta, vec3( 0.0f, 1.0f, 0.0f ) ) );
			const vec3 rayDirectionAdjusted = ( transform * rayDirection );
			const vec3 rayOriginAdjusted = transform * ( rayOrigin - centerPoint );

			// going to have to figure out what the transforms need to be, in order to intersect with the transformed primitve
			result = iRoundedBox( rayOriginAdjusted, rayDirectionAdjusted, normal, scaleFactors, roundingFactor );

			// is it faster to do this, or to do the euler angle stuff, in inverse? need to profile, at scale
			// const mat3 inverseTransform = ( Rotate3D( theta, vec3( 0.0f, 1.0f, 0.0f ) ) * Rotate3D( phi, vec3( 1.0f, 0.0f, 0.0f ) ) );
			const mat3 inverseTransform = inverse( transform );
			normal = inverseTransform * normal;
			break;

		default:
			break;
	}

	if ( result == MAX_DIST_CP ) { // miss condition
		#ifdef SHOWDISCARDS
			if ( ( int( gl_FragCoord.x ) % 2 == 0 ) ) {
				outColor = vec4( 1.0f ); // placeholder, helps visualize bounding box
				primitiveID = uvec4( 0xffff );
			} else {
		#endif
				discard;
		#ifdef SHOWDISCARDS
			}
		#endif
	} else {

		// rasterizer outputs
		primitiveID = uvec4( vofiIndex + 1, 0, 0, 0 );

		// writing correct depths
		const vec4 projectedPosition = viewTransform * vec4( rayOrigin + result * rayDirection, 1.0f );
		gl_FragDepth = ( projectedPosition.z / projectedPosition.w + 1.0f ) * 0.5f;

	}
}