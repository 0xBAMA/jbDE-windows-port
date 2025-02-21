#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "random.h"
#include "noise.h"
#include "hg_sdf.glsl"
#include "pbrConstants.glsl"
#include "rayState2.h.glsl"
#include "wood.h"
//=============================================================================================================================
layout( binding = 1, std430 ) readonly buffer rayState { rayState_t state[]; };
layout( binding = 2, std430 ) writeonly buffer intersectionBuffer { intersection_t intersectionScratch[]; };
//=============================================================================================================================
const float scaleFactor = 10.0f;
// layout( rgba8ui ) readonly uniform uimage3D VoraldoModel;
// layout( r32f ) readonly uniform image2D HeightmapTex;
//=============================================================================================================================
vec3 scatterColor;
float GetVolumeDensity( ivec3 pos ) {

	float density;
	// pos = pos + ivec3( 100 );

	// const ivec3 iS = imageSize( VoraldoModel );
	// if ( any( greaterThanEqual( pos, iS ) ) || any( lessThan( pos, ivec3( 0 ) ) ) ) {
		// out of bounds
		// scatterColor = sapphire;
		// density = 10.0f;
	// } else {
	// 	uvec4 VoraldoSample = imageLoad( VoraldoModel, pos );
	// 	// scatterColor = vec3( VoraldoSample.xyz ) / 255.0f;
	// 	// scatterColor = vec3( VoraldoSample.xyz );
	// 	scatterColor = vec3( 0.618f );
	// 	// density = pow( VoraldoSample.w / 255.0f, 2.0f ) * 2.0f;
	// 	density = 20.0f + VoraldoSample.a;
	// }

	vec3 p = pos / scaleFactor;
	vec3 pOriginal = p; // for SDF usage

	const vec3 bboxSize = vec3( 10.0f );
	const float bboxDist = fBox( p, bboxSize );

	if ( bboxDist < 0.0f ) {
		scatterColor = sapphire;
		density = 1.0f;
	} else {
		density = 0.0f;
	}

	return density;
}
//=============================================================================================================================
void main () {
	const rayState_t myState = state[ gl_GlobalInvocationID.x ];

	const uvec2 loc = uvec2( GetPixelIndex( myState ) );
	seed = loc.x * 10625 + loc.y * 23624 + gl_GlobalInvocationID.x * 2335;

	const vec3 origin = GetRayOrigin( myState );
	const vec3 direction = GetRayDirection( myState );

	if ( IsLiving( myState ) ) {

		// DDA traversal to a potential scattering event
		bool hit = false;
		float hitDistance = MAX_DIST;
		vec3 transmission = vec3( 0.0f );
		const vec3 startPos = origin * scaleFactor;

		// start the DDA traversal...
		// from https://www.shadertoy.com/view/7sdSzH
		vec3 deltaDist = 1.0f / abs( direction );
		ivec3 rayStep = ivec3( sign( direction ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( startPos ) );
		vec3 sideDist0 = ( sign( direction ) * ( vec3( mapPos0 ) - startPos ) + ( sign( direction ) * 0.5f ) + 0.5f ) * deltaDist;

		#define MAX_RAY_STEPS 250
		for ( int i = 0; i < MAX_RAY_STEPS && !hit; i++ ) {

			bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
			vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
			ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

			// doing Beer's law - if we pass a threshold check, we scatter
			// int densityRead = max( 0, int( imageLoad( continuum, mapPos0 ).r ) - noiseFloor );
			float densityRead = GetVolumeDensity( mapPos0 ); // constant density for now
			int densityThreshold = 5000;
			float density = exp( -densityRead / float( densityThreshold ) );

			if ( NormalizedRandomFloat() > density ) {

				hit = true;
				transmission = saturate( density ) * scatterColor;
				hitDistance = length( ivec3( floor( startPos ) ) - mapPos0 ) / scaleFactor;

			} else {
				// take a regular step
				sideDist0 = sideDist1;
				mapPos0 = mapPos1;
			}
		}

		// fill out intersection struct
		intersection_t VolumeIntersection;
		VolumeIntersection.data1.x = 0.0f; // supressing warning
		IntersectionReset( VolumeIntersection );

		SetHitAlbedo( VolumeIntersection, transmission );
		SetHitRoughness( VolumeIntersection, 0.0f );
		SetHitDistance( VolumeIntersection, hitDistance );

		SetHitMaterial( VolumeIntersection, VOLUME );
		// SetHitNormal( VolumeIntersection, ... ); // normal doesn't really mean anything here

		if ( hit ) { // this ray hits in the specified range, write back to the buffer
			const int index = int( gl_GlobalInvocationID.x ) * NUM_INTERSECTORS + INTERSECT_VOLUME;
			intersectionScratch[ index ] = VolumeIntersection;
		}
	}
}