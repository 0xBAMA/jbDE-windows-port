#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;

// utils
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// results of the precompute step
layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D dataCacheBuffer;

uniform ivec3 offset;
uniform int wangSeed;

#include "random.h"
#include "noise.h"
#include "wood.h"
#include "hg_sdf.glsl"

// sphere data
const int numSpheres = 65535;
struct sphere_t {
	vec4 positionRadius;
	vec4 colorMaterial;
};
layout( binding = 0, std430 ) buffer sphereData {
	sphere_t spheres[];
};

// uint topFourIDs[ 4 ] = { 0 };
// float topFourDists[ 4 ] = { 0.0f };
// #define REPLACES_FIRST	0
// #define REPLACES_SECOND	1
// #define REPLACES_THIRD	2
// #define REPLACES_FOURTH	3
// #define REPLACES_NONE	4
// // returns true if it replaces
// bool CheckSphere ( uint index, float dist ) {
// 	int status = REPLACES_NONE;
// 	for ( int i = 0; i < 4; i++ ) {
// 		if ( dist >  ) {

// 			break;
// 		}
// 	}
// 	switch ( status ) {
	
// 	}
// }

mat3 Rotate3D ( const float angle, const vec3 axis ) {
	const vec3 a = normalize( axis );
	const float s = sin( angle );
	const float c = cos( angle );
	const float r = 1.0f - c;
	return mat3(
		a.x * a.x * r + c,
		a.y * a.x * r + a.z * s,
		a.z * a.x * r - a.y * s,
		a.x * a.y * r - a.z * s,
		a.y * a.y * r + c,
		a.z * a.y * r + a.x * s,
		a.x * a.z * r + a.y * s,
		a.y * a.z * r - a.x * s,
		a.z * a.z * r + c
	);
}

float frame ( vec3 p ) {
	ivec3 bSize = imageSize( dataCacheBuffer );
	vec3 pCache = p;
	pMod1( p.x, 120.0f );
	p = Rotate3D( pCache.x / 600.0f, vec3( 1.0f, 0.0f, 0.0f ) ) * p;

	return fOpUnionRound( fCylinder( p, bSize.z / 14.0f, bSize.y * 0.35f ), max( fCylinder( p.yxz, bSize.z, bSize.x ), -fCylinder( p.yxz, bSize.z - 69.0f, bSize.x ) ), 69.0f );
}

void main () {
	seed = wangSeed;
	vec3 noiseOffset = 1000.0f * vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() );

	// voxel location
	const ivec3 writeLoc = ivec3( gl_GlobalInvocationID.xyz ) + offset;
	const vec3 pos = vec3( writeLoc ) + vec3( 0.5f ) - ( vec3( imageSize( dataCacheBuffer ) ) / 2.0f );

	// running color
	vec4 color = vec4( 0.0f );
	uint indexOfNearest = 0;

	{

	// iterate through the spheres
		// keep top four nearest
		// keep distance to nearest
		// keep direction away from nearest as normal

		float minDistance = 10000.0f;
		for ( uint i = 0; i < numSpheres; i++ ) {
			vec4 pr = spheres[ i ].positionRadius;
			float d = distance( pos, pr.xyz ) - pr.w;
			minDistance = min( minDistance, d );
			if ( minDistance == d && d <= 0.0f ) {
			// if ( minDistance == d ) {
				// color = vec4( spheres[ i ].colorMaterial.rgb, clamp( -minDistance, 0.0f, 1.0f ) );
				color = spheres[ i ].colorMaterial;
				indexOfNearest = i;

				// ... consider checking spheres in random order?
				break; // I speculate that there is actually only ever one sphere affecting a voxel, this is a good optimization for now
					// eventually I will want to disable this, when I'm collecting the nearest N spheres
			}
		}
	}

	// write the data to the image - pack 16-bit ids into 32 bit storage
	// imageStore( idxBuffer, writeLoc, uvec4( <closest> <second>, <third> <fourth>, 0.0f, 1.0f ) );
	imageStore( idxBuffer, writeLoc, uvec4( indexOfNearest, 0, 0, 0 ) );
	imageStore( dataCacheBuffer, writeLoc, color );
}
