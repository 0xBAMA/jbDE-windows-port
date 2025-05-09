#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// current state of the DLA buffer
layout( r32ui, binding = 2 ) uniform uimage3D DLATexture;

uniform vec3 viewerPosition;
uniform float zoom;
uniform float verticalOffset;

const float maxDist = 1e10f;
const vec2 distBound = vec2( 0.0001f, maxDist );
float box( in vec3 ro, in vec3 rd, in vec3 boxSize, in vec3 boxPosition ) {
	ro -= boxPosition;

	vec3 m = sign( rd ) / max( abs( rd ), 1e-8 );
	vec3 n = m * ro;
	vec3 k = abs( m ) * boxSize;

	vec3 t1 = -n - k;
	vec3 t2 = -n + k;

	float tN = max( max( t1.x, t1.y ), t1.z );
	float tF = min( min( t2.x, t2.y ), t2.z );

	if ( tN > tF || tF <= 0.0f ) {
		return maxDist;
	} else {
		if ( tN >= distBound.x && tN <= distBound.y ) {
			return tN;
		} else if ( tF >= distBound.x && tF <= distBound.y ) {
			return tF;
		} else {
			return maxDist;
		}
	}
}

uniform int sliceOffset;
ivec3 getOffsetPos( ivec3 pos ) {
	// need to do the ring buffer offset/wrap logic for CAStateBuffer
	pos.z += sliceOffset;
	pos.z = pos.z % imageSize( DLATexture ).z;
	return pos;
}

uint sampleBlock ( ivec3 pos ) {
	return imageLoad( DLATexture, getOffsetPos( pos ) ).r;
}

vec3 colorFromIterations ( int i ) {
	// return vec3( 1.0f - exp( -0.005f * i ) ) * vec3( 0.618f, 0.1618f, 0.1f );
	// return vec3( 1.0f - exp( -0.00025f * i ) ) * -vec3( 0.618f, 0.1618f, 0.1f );
	return vec3( exp( -0.00025f * i ) ) * -vec3( 0.1618f, 0.618f, 0.5f );
}

#include "mathUtils.h"
#include "colorRamps.glsl.h"

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 color = vec3( 0.0f );
	const vec3 DLASimDims = imageSize( DLATexture ).xyz;
	const vec3 DLABlockDims = vec3( 2.0f );

	// generate eye ray
	const vec2 iS = imageSize( accumulatorTexture ).xy;
	const vec2 uv = vec2( writeLoc + 0.5f ) / iS;
	const vec2 p = zoom * ( uv * 2.0f - 1.0f ) * vec2( iS.x / iS.y, 1.0f );

	// lookat
	const vec3 forwards = normalize( -viewerPosition );
	const vec3 left = normalize( cross( forwards, vec3( 0.0f, 0.0f, 1.0f ) ) );
	const vec3 up = normalize( cross( forwards, left ) );

	const vec3 location = viewerPosition + p.x * left + ( p.y - verticalOffset ) * up;

	// ray-box test
	const float d = box( location, forwards, DLABlockDims, vec3( 0.0f ) );
	const vec3 hitPosition = location + forwards * d * 1.0001f;

	// dda traversal, from intersection point
	if ( d < maxDist ) {
		const vec3 hitInUVSpace = vec3(
			RangeRemapValue( hitPosition.x, -DLABlockDims.x, DLABlockDims.x, 0.0f, DLASimDims.x ),
			RangeRemapValue( hitPosition.y, -DLABlockDims.y, DLABlockDims.y, 0.0f, DLASimDims.y ),
			RangeRemapValue( hitPosition.z, -DLABlockDims.z, DLABlockDims.z, 0.0f, DLASimDims.z )
		);

		// DDA traversal
		// from https://www.shadertoy.com/view/7sdSzH
		vec3 deltaDist = 1.0f / abs( forwards );
		ivec3 rayStep = ivec3( sign( forwards ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( hitInUVSpace + 0.0f ) );
		vec3 sideDist0 = ( sign( forwards ) * ( vec3( mapPos0 ) - hitInUVSpace ) + ( sign( forwards ) * 0.5f ) + 0.5f ) * deltaDist;

		#define MAX_RAY_STEPS 2000
		for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, DLASimDims ) ) ); i++ ) {
			// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
			bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
			vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
			ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

			// consider using distance to hit
			uint read = sampleBlock( mapPos0 );
			if ( read != 0 ) { // this should be the hit condition
				// color = colorFromIterations( i ) + magma( pow( 1.0f - float( mapPos0.z ) / DLASimDims.z, 2.0f ) );
				color = colorFromIterations( i ) + vec3( 0.1618f );
				break;
			} else {
				color = colorFromIterations( i );
			}

			sideDist0 = sideDist1;
			mapPos0 = mapPos1;
		}
	}

	imageStore( accumulatorTexture, writeLoc, vec4( color, 1.0f ) );
}