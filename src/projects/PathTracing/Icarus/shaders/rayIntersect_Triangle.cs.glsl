#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "random.h"
#include "pbrConstants.glsl"
#include "rayState2.h.glsl"
//=============================================================================================================================
layout( binding = 1, std430 ) readonly buffer rayState { rayState_t state[]; };
layout( binding = 2, std430 ) writeonly buffer intersectionBuffer { intersection_t intersectionScratch[]; };
//=============================================================================================================================
vec4 triangle( vec3 o, vec3 d, vec3 v0, vec3 v1, vec3 v2 ) {
	// find a,b such that:
	// p + kr = av0 + bv1 + (1-a-b)v2 = a(v0-v2)+b(v1-v2)+v2
	// ie. -kr + a(v0-v2) + b(v1-v2) = p-v2
	vec3 e1 = v0 - v2;
	vec3 e2 = v1 - v2;
	vec3 t = o - v2;
	vec3 p = cross( d, e2 );
	vec3 q = cross( t, e1 );
	vec3 a = vec3( dot( q, e2 ), dot( p, t ), dot( q, d ) ) / dot( p, e1 );
	return vec4( a, 1.0f - a.y - a.z );
}
//=============================================================================================================================
// triangle parameters
const float scale = 1.0f;
const vec3 offset = vec3( 0.3f, 0.0f, 0.0f );
const vec3 p0 = scale * vec3(  0.0f, 0.0f,  0.5f ).yzx + offset;
const vec3 p1 = scale * vec3(  0.5f, 0.0f, -0.5f ).yzx + offset;
const vec3 p2 = scale * vec3( -0.5f, 0.0f, -0.5f ).yzx + offset;

vec4 result;
bool intersect ( rayState_t rayState ) {
	// update the intersection info
	result = triangle( GetRayOrigin( rayState ), GetRayDirection( rayState ), p0, p1, p2 );
	return all( bvec2( all( greaterThanEqual( result, vec4( 0.0f ) ) ), all( lessThanEqual( result.yzw, vec3( 1.0f ) ) ) ) );
}
//=============================================================================================================================
void main () {
	const rayState_t myState = state[ gl_GlobalInvocationID.x ];

	// seed the RNG
	const uvec2 loc = uvec2( GetPixelIndex( myState ) );
	seed = loc.x * 10625 + loc.y * 23624 + gl_GlobalInvocationID.x * 42069;

	// ray living check
	if ( IsLiving( myState ) ) {

		// do the intersection operation
		bool hit = intersect( myState );

		// fill out the intersection struct
		intersection_t TriangleIntersection;
		TriangleIntersection.data1.x = 0.0f; // suppressing warning
		IntersectionReset( TriangleIntersection );

		// set color based on barycentrics
		// SetHitAlbedo( TriangleIntersection, ( ( result.z < 0.1f ) ? vec3( 2.0f ) : result.yzw * vec3( 1.0f, 0.0f, 0.0f ) ) );
		// SetHitAlbedo( TriangleIntersection, vec3( 5.0f ) );
		// SetHitAlbedo( TriangleIntersection, result.yzw * 3.0f );
		SetHitAlbedo( TriangleIntersection, mix( mix( blood, honey, result.y ), aqua, result.z ) * 10.0f );
		SetHitRoughness( TriangleIntersection, 0.1f );
		SetHitDistance( TriangleIntersection, result.x );
		SetHitMaterial( TriangleIntersection, EMISSIVE );

		const vec3 normal = normalize( cross( p0 - p1, p0 - p2 ) );
		SetHitFrontface( TriangleIntersection, ( dot( GetRayDirection( myState ), normal ) > 0 ) );
		SetHitNormal( TriangleIntersection, GetHitFrontface( TriangleIntersection ) ? normal : -normal );

		// write it back to the buffer
		if ( hit ) {
			const int index = int( gl_GlobalInvocationID.x ) * NUM_INTERSECTORS + INTERSECT_TRIANGLE;
			intersectionScratch[ index ] = TriangleIntersection;
		}
	}
}