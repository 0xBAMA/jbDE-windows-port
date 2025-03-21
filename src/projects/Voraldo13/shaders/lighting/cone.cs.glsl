#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
layout( binding = 4, rgba16f ) uniform image3D lightBlock;
layout( binding = 5, rgba8ui ) uniform uimage3D blueNoise;
// =============================================================================
uniform vec3 lightPosition;
uniform vec3 targetPosition;
uniform float coneAngle;
uniform float distancePower;
uniform float decay;
uniform vec4 color;
// =============================================================================

#include "VoraldoCompatibility/intersect.h"

// linearly interpolated sample of the block
vec4 linearlyInterpolatedSample ( vec3 p ) {
	const ivec3 wholePart = ivec3( floor( p ) );
	const vec3 fractPart = fract( p );

	// takes 8 image samples - expensive...
	vec4 samples[ 8 ];
	samples[ 0 ] = imageLoad( colorBlockBack, wholePart );
	samples[ 1 ] = imageLoad( colorBlockBack, wholePart + ivec3( 1, 0, 0 ) );
	samples[ 2 ] = imageLoad( colorBlockBack, wholePart + ivec3( 0, 1, 0 ) );
	samples[ 3 ] = imageLoad( colorBlockBack, wholePart + ivec3( 1, 1, 0 ) );
	samples[ 4 ] = imageLoad( colorBlockBack, wholePart + ivec3( 0, 0, 1 ) );
	samples[ 5 ] = imageLoad( colorBlockBack, wholePart + ivec3( 1, 0, 1 ) );
	samples[ 6 ] = imageLoad( colorBlockBack, wholePart + ivec3( 0, 1, 1 ) );
	samples[ 7 ] = imageLoad( colorBlockBack, wholePart + ivec3( 1, 1, 1 ) );

	vec4 interpolatedOnX[ 4 ];
	interpolatedOnX[ 0 ] = mix( samples [ 0 ], samples[ 1 ], fractPart.x );
	interpolatedOnX[ 1 ] = mix( samples [ 2 ], samples[ 3 ], fractPart.x );
	interpolatedOnX[ 2 ] = mix( samples [ 4 ], samples[ 5 ], fractPart.x );
	interpolatedOnX[ 3 ] = mix( samples [ 6 ], samples[ 7 ], fractPart.x );

	vec4 interpolatedOnY[ 2 ];
	interpolatedOnY[ 0 ] = mix( interpolatedOnX[ 0 ], interpolatedOnX[ 1 ], fractPart.y );
	interpolatedOnY[ 1 ] = mix( interpolatedOnX[ 2 ], interpolatedOnX[ 3 ], fractPart.y );

	return mix( interpolatedOnY[ 0 ], interpolatedOnY[ 1 ], fractPart.z );
}

vec3 conePointingDirection = vec3( 0.0 );

// trace throught the block
#define NUMSTEPS 5000
void traceRay ( vec3 direction ) {
	const vec3 lightBlockSize = vec3( imageSize( lightBlock ) );
	const vec3 origin = ( vec3( 2.0 * gl_GlobalInvocationID.xyz ) - lightBlockSize ) / lightBlockSize;
	const vec3 lightPos = ( vec3( 2.0 * lightPosition ) - lightBlockSize ) / lightBlockSize;

	Intersect( origin, direction );

	const float lightDistance = distance( origin, lightPos );
	const float boundDistance = distance( origin, origin + tMin * direction );

	float currentT = ( boundDistance < lightDistance ) ? tMin : -lightDistance;
	const float stepSize = 0.002;
	vec4 currentIntensity = color * color.w;
	vec4 previousIntensity = imageLoad( lightBlock, ivec3( gl_GlobalInvocationID.xyz ) );

	for ( int i = 0; i < NUMSTEPS; i++ ) {
		if ( currentT < 0.0 && ( currentIntensity.r > 0.0 || currentIntensity.g > 0.0 || currentIntensity.b > 0.0 ) ) {
			const vec3 sampleLocation = ( lightBlockSize / 2.0f ) * ( origin + currentT * direction + vec3( 1.0 ) );
			// float alphaSample = linearlyInterpolatedSample( sampleLocation ).a;
			float alphaSample = imageLoad( colorBlockBack, ivec3( sampleLocation ) ).a;
			currentIntensity *= 1.0 - pow( alphaSample, decay );
			currentT += stepSize;
		} else {
			break;
		}
	}
	// look at this instead of inverse distance squared - 1/d^2 is too peaky near the source
	// https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html
	currentIntensity *= 1.0 / pow( lightDistance, distancePower );
	currentIntensity *= smoothstep( coneAngle, 0.0, acos( dot( direction, conePointingDirection ) ) );
	currentIntensity += previousIntensity;
	imageStore( lightBlock, ivec3( gl_GlobalInvocationID.xyz ), vec4( currentIntensity ) );
}

// =============================================================================
void main () {
	vec3 lightDirection = normalize( vec3( gl_GlobalInvocationID.xyz ) - lightPosition );
	conePointingDirection = normalize( targetPosition - lightPosition );
	traceRay( lightDirection );
}
