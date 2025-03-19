#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, rgba32ui ) uniform uimage2D deferredResult1;
layout( binding = 3, rgba32ui ) uniform uimage2D deferredResult2;
layout( binding = 4, r32ui ) uniform uimage2D deferredResult3;
layout( binding = 5, r32f ) uniform image3D lightCache1;
layout( binding = 6, r32f ) uniform image3D lightCache2;
layout( binding = 7, r32f ) uniform image3D lightCache3;
//=============================================================================================================================
// gpu-side code for ray-BVH traversal
	// used for computing rD, reciprocal direction
float tinybvh_safercp( const float x ) { return x > 1e-12f ? ( 1.0f / x ) : ( x < -1e-12f ? ( 1.0f / x ) : 1e30f ); }
vec3 tinybvh_safercp( const vec3 x ) { return vec3( tinybvh_safercp( x.x ), tinybvh_safercp( x.y ), tinybvh_safercp( x.z ) ); }
//=============================================================================================================================
// node and triangle data specifically for the BVH
layout( binding = 0, std430 ) readonly buffer cwbvhNodesBuffer { vec4 cwbvhNodes[]; };
layout( binding = 1, std430 ) readonly buffer cwbvhTrisBuffer { vec4 cwbvhTris[]; };
// vertex data for the individual triangles' vertices, in a usable format ( .w can be used to pack more data )
layout( binding = 2, std430 ) readonly buffer triangleDataBuffer { vec4 triangleData[]; };

// second set, for the grass blades
layout( binding = 3, std430 ) readonly buffer cwbvhNodesBuffer2 { vec4 cwbvhNodes2[]; };
layout( binding = 4, std430 ) readonly buffer cwbvhTrisBuffer2 { vec4 cwbvhTris2[]; };
layout( binding = 5, std430 ) readonly buffer triangleDataBuffer2 { vec4 triangleData2[]; };
//=============================================================================================================================
#include "consistentPrimitives.glsl.h" // ray-sphere, ray-box inside traverse.h
#include "noise.h"
#include "pbrConstants.glsl"

#define NODEBUFFER cwbvhNodes
#define TRIBUFFER cwbvhTris
#define TRAVERSALFUNC traverse_cwbvh_terrain

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC

#define NODEBUFFER cwbvhNodes2
#define TRIBUFFER cwbvhTris2
#define TRAVERSALFUNC traverse_cwbvh_grass

vec3 grassColorLeaf = vec3( 0.0f );
bool leafTestFunc ( vec3 origin, vec3 direction, uint index, inout float tmax, inout vec2 uv ) {
	// test against N triangles for one blade of grass
		// initially, we will just be doing a single triangle

	const uint baseIndex = 4 * index;
	vec4 v0 = triangleData2[ baseIndex + 0 ];
	vec4 v1 = triangleData2[ baseIndex + 1 ];
	vec4 v2 = triangleData2[ baseIndex + 2 ];

// moller trombore on a dynamic triangle
	// precompute vectors representing edges - this computation should moved to the grass shader, out of the traversal code... should be at least a bit of a perf win
	const vec3 e1 = v1.xyz - v0.xyz; // edge1 = vertex1 - vertex0
	const vec3 e2 = v2.xyz - v0.xyz; // edge2 = vertex2 - vertex0

	const vec3 r = cross( direction.xyz, e1 );
	const float a = dot( e2, r );
	if ( abs( a ) < 0.0000001f )
		return false;
	const float f = 1.0f / a;
	const vec3 s = origin.xyz - v0.xyz;
	const float u = f * dot( s, r );
	if ( u < 0 || u > 1 )
		return false;
	const vec3 q = cross( s, e2 );
	const float v = f * dot( direction.xyz, q );
	if ( v < 0 || u + v > 1 )
		return false;
	const float d = f * dot( e1, q );
	if ( d <= 0.0f || d >= tmax )
		return false;
	uv = vec2( u, v ), tmax = d;
	grassColorLeaf = vec3( v0.w, v1.w, v2.w );
	return true;
}
#define CUSTOMLEAFTEST leafTestFunc

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC
#undef CUSTOMLEAFTEST

#include "random.h"
#include "normalEncodeDecode.h"
//=============================================================================================================================

uniform float time;
uniform float blendAmount;
uniform ivec2 blueNoiseOffset;

// enable flags for the three lights
uniform bvec3 lightEnable;

// Key Light
uniform vec3 lightDirections0[ 16 ];

// Fill Light
uniform vec3 lightDirections1[ 16 ];

// Back Light
uniform vec3 lightDirections2[ 16 ];

//=============================================================================================================================

// vector axis/angle rotation, from https://suricrasia.online/blog/shader-functions/
vec3 erot( vec3 p, vec3 ax, float ro ) {
	return mix( dot( ax, p ) * ax, p, cos( ro ) ) + cross( ax, p ) * sin( ro );
}

// blue noise helper
vec4 blue() {
	return vec4( imageLoad( blueNoiseTexture, ivec2( blueNoiseOffset + ivec2( gl_GlobalInvocationID.xy ) ) % ivec2( imageSize( blueNoiseTexture ).xy ) ) ) / 255.0f;
}

// terrain trace helper
vec4 terrainTrace ( vec3 origin, vec3 direction ) {
	return traverse_cwbvh_terrain( origin, direction, tinybvh_safercp( direction ), 1e30f );
}

// grass trace helper
vec4 grassTrace ( vec3 origin, vec3 direction ) {
	return traverse_cwbvh_grass( origin, direction, tinybvh_safercp( direction ), 1e30f );
}

// sphere trace helper
vec4 sphereTrace ( vec3 origin, vec3 direction ) {
	vec3 normal;
	float d = iSphere( origin, direction, normal, 1.0f );
	// same as the grass/terrain, x is distance, yzw in this case is normal
	return vec4( d, normal );
}

// SDF geometry helpers
float deSrc ( vec3 p ) {
	vec3 pOriginal = p;
	float s = 2.0f, l = 0.0f;
	p = abs( p );
	for ( int j = 0; j++ < 8; )
		p =- sign( p ) * ( abs( abs( abs( p ) - 2.0f ) - 1.0f ) - 1.0f ),
		p *= l = -1.3f / dot( p, p ),
		p -= 0.15f, s *= l;
	return max( length( p ) / s, ( length( pOriginal ) - 1.0f ) );
}

float de ( vec3 p ) {
	// offset, scaling, masking
	float scale = 2.0f;
	p.z -= 0.3f;
	return deSrc( p * scale ) / scale;
}

const float epsilon = 0.0001f;
vec3 SDFNormal( in vec3 position ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );
}

vec4 SDFTrace ( vec3 origin, vec3 direction ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	vec3 pQuery = origin;
	for ( int steps = 0; steps < 300; steps++ ) {
		pQuery = origin + dTotal * direction;
		dQuery = de( pQuery );
		dTotal += dQuery * 0.9f; // small understep
		if ( dTotal > 5.0f || abs( dQuery ) < epsilon ) {
			break;
		}
	}
	// matching interface, x is distance, yzw is normal
	return vec4( dTotal, SDFNormal( origin + dTotal * direction ) );
}

//==Surface=ID=Values==========================================================================================================
#define NOHIT	0
#define SKIRTS	1
#define TERRAIN	2
#define GRASS	3
#define SDF		4
#define SPHERE	5
//=============================================================================================================================

void main () {
	// solve for voxel position, so that it spans the entire snowglobe
	ivec3 writeLoc = ivec3( gl_GlobalInvocationID.xyz );

	// this will need to change to a jitter
	vec3 worldSpace = 2.0f * ( vec3( writeLoc + blue().xyz ) / imageSize( lightCache1 ).xyz ) - vec3( 1.0f );

	// reject out-of-sphere texels - potentially check SDF, here, too, since that's not very expensive
	if ( distance( worldSpace, vec3( 0.0f ) ) < 1.01 ) {

		// load previous values
		vec3 previousValues = vec3(
			imageLoad( lightCache1, writeLoc ).x,
			imageLoad( lightCache2, writeLoc ).x,
			imageLoad( lightCache3, writeLoc ).x
		);

		// maybe find some way to shuffle this at some point...
			// alternatively, check against all 16? We can do a 3d sequence like the bayer thing to reduce the number of updated texels per frame
		const int idx = 0;

		// trace a ray for each enabled light
		if ( lightEnable.x ) {
			// trace against potential occluders
			vec4 terrainShadowHit = terrainTrace( worldSpace, lightDirections0[ idx ] );	// terrain
			vec4 SDFShadowHit = SDFTrace( worldSpace, lightDirections0[ idx ] );			// SDF
			vec4 grassShadowHit = grassTrace( worldSpace, lightDirections0[ idx ] );		// grass
			vec4 sphereShadowHit = sphereTrace( worldSpace, lightDirections0[ idx ] );		// sphere

			// occlusion determination
			bool occluded = ( min( min( terrainShadowHit.x, SDFShadowHit.x ), grassShadowHit.x ) < sphereShadowHit.x );

			// mix and writeback
			imageStore( lightCache1, writeLoc, vec4( mix( occluded ? 0.0f : 1.0f, previousValues.x, blendAmount ) ) );
		}

		if ( lightEnable.y ) {
			vec4 terrainShadowHit = terrainTrace( worldSpace, lightDirections1[ idx ] );	// terrain
			vec4 SDFShadowHit = SDFTrace( worldSpace, lightDirections1[ idx ] );			// SDF
			vec4 grassShadowHit = grassTrace( worldSpace, lightDirections1[ idx ] );		// grass
			vec4 sphereShadowHit = sphereTrace( worldSpace, lightDirections1[ idx ] );		// sphere

			bool occluded = ( min( min( terrainShadowHit.x, SDFShadowHit.x ), grassShadowHit.x ) < sphereShadowHit.x );
			imageStore( lightCache2, writeLoc, vec4( mix( occluded ? 0.0f : 1.0f, previousValues.y, blendAmount ) ) );
		}

		if ( lightEnable.z ) {
			vec4 terrainShadowHit = terrainTrace( worldSpace, lightDirections2[ idx ] );	// terrain
			vec4 SDFShadowHit = SDFTrace( worldSpace, lightDirections2[ idx ] );			// SDF
			vec4 grassShadowHit = grassTrace( worldSpace, lightDirections2[ idx ] );		// grass
			vec4 sphereShadowHit = sphereTrace( worldSpace, lightDirections2[ idx ] );		// sphere

			bool occluded = ( min( min( terrainShadowHit.x, SDFShadowHit.x ), grassShadowHit.x ) < sphereShadowHit.x );
			imageStore( lightCache3, writeLoc, vec4( mix( occluded ? 0.0f : 1.0f, previousValues.z, blendAmount ) ) );
		}
	}
}
