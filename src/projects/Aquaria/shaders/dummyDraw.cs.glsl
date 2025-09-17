#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D dataCacheBuffer;

uniform float time;
uniform vec2 resolution;
uniform float scale;
uniform vec2 viewOffset;
uniform mat3 invBasis;
uniform vec3 blockSize;
uniform float thinLensIntensity;
uniform float thinLensDistance;
uniform float blendAmount;
uniform int refractiveBubble;
uniform float bubbleIoR;
uniform float fogScalar;
uniform vec3 fogColor;

#define PI 3.1415f

uniform int wangSeed;
#include "random.h"
#include "intersect.h"

// ==============================================================================================
vec3 eliNormal ( in vec3 pos, in vec3 center, in vec3 radii ) {
	return normalize( ( pos - center ) / ( radii * radii ) );
}
float eliIntersect ( in vec3 ro, in vec3 rd, in vec3 center, in vec3 radii ) {
	vec3 oc = ro - center;
	vec3 ocn = oc / radii;
	vec3 rdn = rd / radii;
	float a = dot( rdn, rdn );
	float b = dot( ocn, rdn );
	float c = dot( ocn, ocn );
	float h = b * b - a * ( c - 1.0f );
	if ( h < 0.0f ) return -1.0f;
	return ( -b - sqrt( h ) ) / a;
}

float RemapRange ( const float value, const float iMin, const float iMax, const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) / ( iMax - iMin ) ) * ( value - iMin ) );
}

uniform ivec2 noiseOffset;
float blueNoiseRead ( ivec2 loc, int idx ) {
	ivec2 wrappedLoc = ( loc + noiseOffset ) % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	switch ( idx ) {
	case 0:
		return sampleValue.r / 255.0f;
		break;
	case 1:
		return sampleValue.g / 255.0f;
		break;
	case 2:
		return sampleValue.b / 255.0f;
		break;
	case 3:
		return sampleValue.a / 255.0f;
		break;
	default:
		break;
	}
}

// for hexagon rejection sampling - trying hex grid, sort of experimental
float hexSdf ( vec2 p ) {
	p.x *= 0.57735f * 2.0f;
	p.y += mod( floor( p.x ), 2.0f ) * 0.5f;
	p = abs( ( mod( p, 1.0f ) - 0.5f ) );
	return abs( max( p.x * 1.5f + p.y, p.y * 2.0f ) - 1.0f );
}

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 prevColor = imageLoad( accumulatorTexture, writeLoc ).rgb;
	vec3 col = vec3( 0.0f );

	vec4 blue = vec4(
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 0 ),
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 1 ),
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 2 ),
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 3 )
	);

	// intialize the rng
	seed = uint( wangSeed * 10 + writeLoc.x * 69 + writeLoc.y * 420 + blue.x * 255 + blue.y * 255 + blue.z * 255 + blue.w * 255 );

	// used later
	// vec3 ditherValue = blue.xyz / 64.0f - vec3( 1.0f / 128.0f );
	vec3 ditherValue = blue.xyz / 32.0f - vec3( 1.0f / 64.0f );

	// remapped uv
	vec2 subpixelOffset = blue.xy;
	vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + subpixelOffset + viewOffset ) / resolution.xy;
	uv = ( uv - 0.5f ) * 2.0f;

	// aspect ratio correction
	uv.x *= ( resolution.x / resolution.y );

	// box intersection
	float tMin, tMax;
	vec3 Origin = invBasis * vec3( scale * uv, -2.0f );
	vec3 Direction = invBasis * normalize( vec3( uv * 0.1f, 2.0f ) );

	// thin lens calculations
	// if ( hexSdf( blue.zw ) > 0.5f ) { // this rejection sampling just acts weird, unconstrained hex grid is not a reasonable choice
		// vec3 focusPoint = Origin + ( thinLensDistance * uv.x * uv.y ) * Direction; // FUCKED
		vec3 focusPoint = Origin + thinLensDistance * Direction;
		// Origin = invBasis * vec3( scale * uv + blue.zw * thinLensIntensity, -2.0f );
		// Origin = invBasis * vec3( scale * uv + pow( randomInUnitDisk(), vec2( 0.2f ) ) * thinLensIntensity, -2.0f ); // pushes bokeh out towards edge of the ring
		// Origin = invBasis * vec3( scale * uv + pow( randomInUnitDisk(), vec2( 2.0f ) ) * thinLensIntensity, -2.0f );
		
		// Origin = invBasis * vec3( scale * uv + randomInUnitDisk() * thinLensIntensity, -2.0f );
		// Origin = invBasis * vec3( scale * uv + RejectionSampleHexOffset() * thinLensIntensity, -2.0f );
		Origin = invBasis * vec3( scale * uv + UniformSampleHexagon( blue.zw ) * thinLensIntensity, -2.0f );
		Direction = focusPoint - Origin;
	// }

	bool hitBubble = false;
	if ( refractiveBubble != 0 ) {
		float sHit = eliIntersect( Origin, Direction, vec3( 0.0f ), ( blockSize / 2.0f ) );
		if ( sHit > 0.0f ) {
			// the ray hits
			hitBubble = true;
			// update ray position to be at the sphere's surface
			Origin = Origin + sHit * Direction;
			// update ray direction to the refracted ray
			Direction = refract( Direction, eliNormal( Origin, vec3( 0.0f ), blockSize / 2.0f ), bubbleIoR );
		}
	}

	if ( hitBubble && refractiveBubble != 0 || refractiveBubble == 0 ) {

		// then intersect with the AABB
		const bool hit = IntersectAABB( Origin, Direction, -blockSize / 2.0f, blockSize / 2.0f, tMin, tMax );

		// what are the dimensions
		const ivec3 blockDimensions = imageSize( dataCacheBuffer );

		if ( hit ) { // texture sample
			// for trimming edges
			const float epsilon = 0.001f;
			const vec3 hitpointMin = Origin + tMin * Direction;
			const vec3 hitpointMax = Origin + tMax * Direction;
			const vec3 blockUVMin = vec3(
			RemapRange( hitpointMin.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0 + epsilon, blockDimensions.x - epsilon ),
			RemapRange( hitpointMin.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0 + epsilon, blockDimensions.y - epsilon ),
			RemapRange( hitpointMin.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0 + epsilon, blockDimensions.z - epsilon )
			);

			const vec3 blockUVMax = vec3(
			RemapRange( hitpointMax.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0 + epsilon, blockDimensions.x - epsilon ),
			RemapRange( hitpointMax.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0 + epsilon, blockDimensions.y - epsilon ),
			RemapRange( hitpointMax.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0 + epsilon, blockDimensions.z - epsilon )
			);

			// DDA traversal
			// from https://www.shadertoy.com/view/7sdSzH
			vec3 deltaDist = 1.0f / abs( Direction );
			ivec3 rayStep = ivec3( sign( Direction ) );
			bvec3 mask0 = bvec3( false );
			ivec3 mapPos0 = ivec3( floor( blockUVMin + 0.0f ) );
			vec3 sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - blockUVMin ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;

			#define MAX_RAY_STEPS 2200
			for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, imageSize( dataCacheBuffer ) ) ) ); i++ ) {
				// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
				bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
				vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
				ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

				// consider using distance to bubble hit, when bubble is enabled
				vec4 read = imageLoad( dataCacheBuffer, mapPos0 );
				if ( read.a != 0.0f ) { // this should be the hit condition
					col = vec3( 1.0f - exp( -i * fogScalar ) ) * fogColor + ditherValue + read.rgb;
					// col = read.rgb;
					break;
				} else {
					col = vec3( 1.0f - exp( -i * fogScalar ) ) * fogColor + ditherValue;
				}

				sideDist0 = sideDist1;
				mapPos0 = mapPos1;
			}
		}

	}
	// write the data to the image
	// imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
	imageStore( accumulatorTexture, writeLoc, vec4( mix( col, prevColor, blendAmount ), 1.0f ) );
}
