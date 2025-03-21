#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
layout( binding = 4, rgba16f ) uniform image3D lightBlock;
layout( binding = 5, rgba8ui ) uniform uimage3D blueNoise;
// =============================================================================
uniform float theta;
uniform float phi;
uniform float decay;
uniform vec4 color;
// =============================================================================
mat3 RotationMatrix ( vec3 axis, float angle ) {
	vec3 ax = normalize( axis );
	float s = sin( angle );
	float c = cos( angle );
	float oc = 1.0 - c;
	return mat3( oc * ax.x * ax.x + c,        oc * ax.x * ax.y - ax.z * s,  oc * ax.z * ax.x + ax.y * s,
				oc * ax.x * ax.y + ax.z * s,  oc * ax.y * ax.y + c,         oc * ax.y * ax.z - ax.x * s,
				oc * ax.z * ax.x - ax.y * s,  oc * ax.y * ax.z + ax.x * s,  oc * ax.z * ax.z + c );
}

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

// trace throught the block
#define NUMSTEPS 5000
void traceRay ( vec3 direction ) {
	const vec3 lightBlockSize = vec3( imageSize( lightBlock ) );
	const vec3 origin = ( vec3( 2 * gl_GlobalInvocationID.xyz ) - lightBlockSize ) / lightBlockSize;

	Intersect( origin, direction );

	float currentT = tMin;
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
	currentIntensity += previousIntensity;
	imageStore( lightBlock, ivec3( gl_GlobalInvocationID.xyz ), vec4( currentIntensity ) );
}

// =============================================================================
void main () {
	vec3 lightDirection = vec3( 0.0, 0.0, -1.0 );
	lightDirection *= RotationMatrix( vec3( 1.0, 0.0, 0.0 ), phi );
	lightDirection *= RotationMatrix( vec3( 0.0, 1.0, 0.0 ), theta );
	traceRay( lightDirection );
}
