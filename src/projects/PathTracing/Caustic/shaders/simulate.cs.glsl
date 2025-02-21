#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( r32ui ) uniform uimage2D bufferImage;
layout( binding = 0, std430 ) buffer maxBuffer { uint maxCount; };

uniform float t;
uniform int rngSeed;

float de ( vec3 p ) {
	p.xz -= vec2( 5.0f );
	float e, i=0., j, f, a, w;
	// p.yz *= mat2(cos(0.7f),sin(0.7f),-sin(0.7f),cos(0.7f));
	f = 0.1; // wave amplitude
	i < 45. ? p : p -= .001;
	e = p.y + 5.;
	for( a = j = .9; j++ < 30.; a *= .8 ){
		vec2 m = vec2( 1. ) * mat2(cos(j),sin(j),-sin(j),cos(j));
		float x = dot( p.xz, m ) * f + t + t; // time varying behavior
		// float x = dot( p.xz, m ) * f + 0.;
		w = exp( sin( x ) - 1. );
		p.xz -= m * w * cos( x ) * a;
		e -= w * a;
		f *= 1.2;
	}
	return e;
}

const float epsilon = 0.001f;

vec3 normal ( in vec3 position ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );
}

float raymarch ( in vec3 rayOrigin, in vec3 rayDirection ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	vec3 pQuery = rayOrigin;
	for ( int steps = 0; steps < 20; steps++ ) {
		pQuery = rayOrigin + dTotal * rayDirection;
		dQuery = de( pQuery );
		dTotal += dQuery;
		if ( dTotal > 10.0f || abs( dQuery ) < epsilon ) {
			break;
		}
	}
	return dTotal;
}

float rayPlaneIntersect ( in vec3 rayOrigin, in vec3 rayDirection ) {
	const vec3 normal = vec3( 0.0f, 1.0f, 0.0f );
	const vec3 planePt = vec3( 0.0f, 0.0f, 0.0f ); // not sure how far down this should be
	return -( dot( rayOrigin - planePt, normal ) ) / dot( rayDirection, normal );
}

#include "mathUtils.h"
#include "random.h"

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	seed = rngSeed;

	const float fieldScale = 3.0f;

	for ( int i = 0; i < 5; i++ ) {
		const vec2 remappedLoc = vec2(
			RangeRemapValue( float( loc.x ) + NormalizedRandomFloat(), 0, imageSize( bufferImage ).x, 0.0f, fieldScale ),
			RangeRemapValue( float( loc.y ) + NormalizedRandomFloat(), 0, imageSize( bufferImage ).y, 0.0f, fieldScale )
		);

		// so we create the ray
		const vec3 initialRayPosition = vec3( remappedLoc.x, 0.0f, remappedLoc.y );
		const vec3 initialRayDirection = vec3( 0.0f, -1.0f, 0.0f );

		// raymarch to the water's surface
		const float distanceToWater = raymarch( initialRayPosition, initialRayDirection );

		// refract at the surface
		const vec3 hitPoint = initialRayPosition + initialRayDirection * distanceToWater;
		const vec3 refractedRay = refract( initialRayDirection, normal( hitPoint ), 1.3f );

		// intersect the refracted ray with a plane
		const float dPlane = rayPlaneIntersect( hitPoint, refractedRay );
		const vec3 finalPoint = vec3( hitPoint + dPlane * refractedRay );

		// do the increment, where this ray-plane intersection says
		const ivec2 writeLoc = ivec2(
			int( RangeRemapValue( finalPoint.x, 0.0f, fieldScale, 0, imageSize( bufferImage ).x ) ),
			int( RangeRemapValue( finalPoint.z, 0.0f, fieldScale, 0, imageSize( bufferImage ).y ) )
		);

		if ( all( lessThan( writeLoc.xy, imageSize( bufferImage ) ) ) && all( greaterThan( writeLoc, ivec2( 0 ) ) ) ) {
			// atomicMax( maxCount, imageAtomicAdd( bufferImage, writeLoc, 1 ) + 1 );
			imageAtomicAdd( bufferImage, writeLoc, 1 );
		}
	}
}