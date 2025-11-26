#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// the buffer we're splatting into
layout( binding = 2, r32ui ) uniform uimage3D SplatBuffer;

// the list of points we want to look at
layout( binding = 0, std430 ) readonly buffer pointData {
	vec4 data[];
};

uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;

uniform int n;
uniform float scale;

#include "random.h"
uniform int wangSeed;

/*
todo
void writeAAPoint ( vec3 p ) {
	// scaled contribution to nearest 8 cells
	vec3 pBase = floor( p );
	vec3 pFrac = fract( p );

	imageAtomicAdd( SplatBuffer, ivec3( pBase + vec3() ), 1.0f - pFrac )
}
*/

void writeAt ( vec3 p ) {
	const vec3 iS = vec3( imageSize( SplatBuffer ).xyz );
	vec3 pWrite = vec3( p * iS + ( iS / 2.0f ) );
	imageAtomicAdd( SplatBuffer, ivec3( pWrite ), uint( 1024 * fract( pWrite.z ) ) );
	imageAtomicAdd( SplatBuffer, ivec3( pWrite ) + ivec3( 0, 0, 1 ), uint( 1024 * ( 1.0f - fract( pWrite.z ) ) ) );

//	imageAtomicAdd( SplatBuffer, ivec3( pWrite ), 1024 );
}

// transform, number of points to consider
void main () {
	seed = wangSeed + gl_GlobalInvocationID.x * 69420 + gl_GlobalInvocationID.y * 8675309;

	// load a vec4 from the buffer...
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;
	if ( index < n ) {

		for ( int i = 0; i < 10; i++ ) {
			vec4 pData = data[ index ] + vec4( 0.00001f * RandomCauchyVector(), 0.0f );
			vec3 p = basisX * scale * pData.x + -basisY * scale * pData.y + basisZ * scale * pData.z;

			// probably do something to normalize on x
			const vec3 iS = vec3( imageSize( SplatBuffer ).xyz );
			p.x *= -iS.y / iS.x;
			p.z *= 0.5f;

			writeAt( p );
		}
	}
}
