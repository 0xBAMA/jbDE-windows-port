#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, rgba32ui ) uniform uimage3D bufferTexture;

#include "pbrConstants.glsl"
#include "intersect.h"
#include "random.h"

uniform ivec2 noiseOffset;
vec4 blueNoiseRead ( ivec2 loc ) {
	ivec2 wrappedLoc = ( loc + noiseOffset ) % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	return sampleValue / 255.0f;
}

float RemapRange ( const float value, const float iMin, const float iMax, const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) / ( iMax - iMin ) ) * ( value - iMin ) );
}

uniform vec2 viewOffset;
uniform float scale;
uniform mat3 invBasis;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec2 resolution = vec2( imageSize( accumulatorTexture ).xy );

	// tbd how we're going to draw this, just using renderdoc initially
	vec3 col = vec3( 0.0f );

	{
		// remapped uv
		vec2 subpixelOffset = blueNoiseRead( writeLoc ).xy;
		vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + subpixelOffset + viewOffset ) / resolution.xy;
		uv = ( uv - 0.5f ) * 2.0f;

		// aspect ratio correction
		uv.x *= ( resolution.x / resolution.y );

		// box intersection
		float tMin, tMax;
		vec3 Origin = invBasis * vec3( scale * uv, -1000.0f );
		vec3 Direction = invBasis * normalize( vec3( uv * 0.0f, 1.0f ) );

		const ivec3 blockSize = imageSize( bufferTexture ).xyz;
		const vec3 blockSizeHalf = vec3( blockSize ) / 2.0f;

		// then intersect with the AABB
		const bool hit = IntersectAABB( Origin, Direction, -blockSizeHalf, blockSizeHalf, tMin, tMax );

		if ( hit ) { // texture sample
			col += vec3( 0.01f );

			// for trimming edges
			const float epsilon = 0.001f;
			const vec3 hitpointMin = Origin + tMin * Direction;
			const vec3 hitpointMax = Origin + tMax * Direction;
			const vec3 blockUVMin = vec3(
				RemapRange( hitpointMin.x, -blockSizeHalf.x, blockSizeHalf.x, 0 + epsilon, blockSize.x - epsilon ),
				RemapRange( hitpointMin.y, -blockSizeHalf.y, blockSizeHalf.y, 0 + epsilon, blockSize.y - epsilon ),
				RemapRange( hitpointMin.z, -blockSizeHalf.z, blockSizeHalf.z, 0 + epsilon, blockSize.z - epsilon )
			);

			// DDA traversal
			// from https://www.shadertoy.com/view/7sdSzH
			vec3 deltaDist = 1.0f / abs( Direction );
			ivec3 rayStep = ivec3( sign( Direction ) );
			bvec3 mask0 = bvec3( false );
			ivec3 mapPos0 = ivec3( floor( blockUVMin + 0.0f ) );
			vec3 sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - blockUVMin ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;

			#define MAX_RAY_STEPS 2200
			for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, imageSize( bufferTexture ) ) ) ); i++ ) {
				// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
				bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
				vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
				ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

				// consider using distance to bubble hit, when bubble is enabled
				uvec4 read = imageLoad( bufferTexture, mapPos0 );
				if ( read.b != 0 ) { // this might be a hit condition
					float radius = uintBitsToFloat( read.y );
					seed = read.b;
					vec3 center = vec3( 5.0f ) + vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() ) * ( imageSize( bufferTexture ).xyz - vec3( 10.0f ) );

					vec2 result = RaySphereIntersect( Origin, Direction, center - blockSizeHalf, radius );
					if ( result != vec2( -1.0f ) ) {
						// col = vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() ) * ( 100.0f / result.x );
						col = mix( nvidia, vec3( 0.0f ), pow( saturate( RemapRange( radius, 30.0f, 0.0f, 0.0f, 1.0f ) ), 1.5f ) ) * ( 500.0f / result.x );
						break;
					}
				}

				sideDist0 = sideDist1;
				mapPos0 = mapPos1;
			}
		}
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
