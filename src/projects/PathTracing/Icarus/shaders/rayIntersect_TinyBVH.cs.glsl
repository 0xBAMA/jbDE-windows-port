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
// gpu-side code for ray-BVH traversal

// used for computing rD, reciprocal direction
float tinybvh_safercp( const float x ) { return x > 1e-12f ? ( 1.0f / x ) : ( x < -1e-12f ? ( 1.0f / x ) : 1e30f ); }

struct Ray {
	// data is defined here as 16-byte values to encourage the compilers
	// to fetch 16 bytes at a time: 12 (so, 8 + 4) will be slower.
	vec4 O, D, rD; // 48 byte - rD is reciprocal direction
	vec4 hit; // 16 byte
};

struct BVHNodeAlt {
	vec4 lmin; // uint left in w
	vec4 lmax; // uint right in w
	vec4 rmin; // uint triCount in w
	vec4 rmax; // uint firstTri in w
};

layout( binding = 3, std430 ) readonly buffer bvhNodes { BVHNodeAlt altNode[]; };
layout( binding = 4, std430 ) readonly buffer indices { uint idx[]; };
layout( binding = 5, std430 ) readonly buffer vertices { vec4 verts[]; }; // eventually extend to contain other material info

#include "consistentPrimitives.glsl.h"

// ============================================================================
//	T R A V E R S E _ A I L A L A I N E
//		x is d, yz is u and v, w is the index of the primitive in the list, uint bits in a float variable
// ============================================================================
vec3 solvedNormal;
vec4 solvedColor;
vec4 traverse_ailalaine ( rayState_t rayState ) {
	const vec3 O = GetRayOrigin( rayState );
	const vec3 D = GetRayDirection( rayState );
	const vec3 rD = vec3( tinybvh_safercp( D.x ), tinybvh_safercp( D.y ), tinybvh_safercp( D.z ) );

	vec4 hit; hit.x = 1e30f;
	uint node = 0;
	uint stack[ 64 ];
	uint stackPtr = 0;

	while ( true ) {
		// fetch the node
		const vec4 lmin = altNode[ node ].lmin, lmax = altNode[ node ].lmax;
		const vec4 rmin = altNode[ node ].rmin, rmax = altNode[ node ].rmax;
		const uint triCount = floatBitsToUint( rmin.w );
		if ( triCount > 0 ) {
			// process leaf node
			const uint firstTri = floatBitsToUint( rmax.w );
			for ( uint i = 0; i < triCount; i++ ) {
				const uint triIdx = idx[ firstTri + i ];

				// // ======================================================================
				// // triangle intersection - M�ller-Trumbore
				// const vec4 edge1 = verts[ 3 * triIdx + 1 ] - verts[ 3 * triIdx + 0 ];
				// const vec4 edge2 = verts[ 3 * triIdx + 2 ] - verts[ 3 * triIdx + 0 ];
				// const vec3 h = cross( D, edge2.xyz );
				// const float a = dot( edge1.xyz, h );
				// if ( abs( a ) < 0.0000001f )
				// 	continue;
				// const float f = 1 / a;
				// const vec3 s = O - verts[ 3 * triIdx + 0 ].xyz;
				// const float u = f * dot( s, h );
				// if ( u < 0.0f || u > 1.0f )
				// 	continue;
				// const vec3 q = cross( s, edge1.xyz );
				// const float v = f * dot( D, q );
				// if ( v < 0.0f || u + v > 1.0f )
				// 	continue;
				// const float d = f * dot( edge2.xyz, q );
				// if ( d > 0.0f && d < hit.x ) {
				// 	hit = vec4(d, u, v, uintBitsToFloat( triIdx ) );
				// 	solvedNormal = normalize( cross( edge1.xyz, edge2.xyz ) ); // frontface determination should happen here, too
				// }

				// ======================================================================
				// sphere intersection... hacky
					// determine bounding box...
						// center + radius - material stuff from idx later
						// you then test your ray against that sphere
				const vec3 center = vec3( verts[ 2 * triIdx ].xyz );
				const float radius = verts[ 2 * triIdx ].w;
				const float d = iSphereOffset( O, D, solvedNormal, radius, center );
				if ( d > 0.0f && d < hit.x ) {
					hit = vec4( d, 0.0f, 0.0f, uintBitsToFloat( triIdx ) );
				}
				// ======================================================================

			}
			if ( stackPtr == 0 ) break;
			node = stack[ --stackPtr ];
			continue;
		}
		uint left = floatBitsToUint( lmin.w ), right = floatBitsToUint( lmax.w );
		// child AABB intersection tests
		const vec3 t1a = ( lmin.xyz - O ) * rD, t2a = ( lmax.xyz - O ) * rD;
		const vec3 t1b = ( rmin.xyz - O ) * rD, t2b = ( rmax.xyz - O ) * rD;
		const vec3 minta = min( t1a, t2a ), maxta = max( t1a, t2a );
		const vec3 mintb = min( t1b, t2b ), maxtb = max( t1b, t2b );
		const float tmina = max( max( max( minta.x, minta.y ), minta.z ), 0 );
		const float tminb = max( max( max( mintb.x, mintb.y ), mintb.z ), 0 );
		const float tmaxa = min( min( min( maxta.x, maxta.y ), maxta.z ), hit.x );
		const float tmaxb = min( min( min( maxtb.x, maxtb.y ), maxtb.z ), hit.x );
		float dist1 = tmina > tmaxa ? 1e30f : tmina;
		float dist2 = tminb > tmaxb ? 1e30f : tminb;

		// traverse nearest child first
		if ( dist1 > dist2 ) {
			float h = dist1; dist1 = dist2; dist2 = h;
			uint t = left; left = right; right = t;
		}
		if ( dist1 == 1e30f ) {
			if ( stackPtr == 0 ) break; else node = stack[ --stackPtr ];
		} else {
			node = left;
			if ( dist2 != 1e30f ) stack[ stackPtr++ ] = right;
		}
	}

	// return hit value ( distance, u, v, and then the triangle idx you can decode with floatBitsToUint )
	return hit;
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
		vec4 hit = traverse_ailalaine( myState );

		// fill out the intersection struct
		intersection_t TinyBVHIntersection;
		TinyBVHIntersection.data1.x = 0.0f; // suppressing warning
		IntersectionReset( TinyBVHIntersection );

		const uint primitiveIdx = floatBitsToUint( hit.a );
		// const uint baseIdx = primitiveIdx * 3;
		// vec3 color = vec3(
		// 	verts[ baseIdx + 0 ].a,
		// 	verts[ baseIdx + 1 ].a,
		// 	verts[ baseIdx + 2 ].a
		// );

		vec3 p = hit.x * GetRayDirection( myState ) + GetRayOrigin( myState );

		// pulling material data from the buffer
		const vec3 color = vec3( verts[ 2 * primitiveIdx + 1 ].xyz );
		const float materialValue =  verts[ 2 * primitiveIdx + 1 ].w;

		SetHitAlbedo( TinyBVHIntersection, color );
		// SetHitAlbedo( TinyBVHIntersection, blood );
		// SetHitAlbedo( TinyBVHIntersection, vec3( 0.8f ) );
		SetHitDistance( TinyBVHIntersection, hit.x );
		// SetHitMaterial( TinyBVHIntersection, ( NormalizedRandomFloat() > 0.33f ) ? DIFFUSE : MIRROR );
		SetHitMaterial( TinyBVHIntersection, REFRACTIVE );
		// SetHitMaterial( TinyBVHIntersection, DIFFUSE );
		SetHitRoughness( TinyBVHIntersection, 0.0f );

		float IoR = 1.3f;

		// if ( primitiveIdx % 181 == 0 ) {
		// 	SetHitMaterial( TinyBVHIntersection, EMISSIVE );
		// 	SetHitAlbedo( TinyBVHIntersection, ( primitiveIdx % 2 != 0 ) ? vec3( 5.0f ) : ( 5.0f * vec3( hit.y, hit.z, 1.0f - hit.y - hit.z ) ) );
		// }

		const vec3 normal = solvedNormal;
		SetHitFrontface( TinyBVHIntersection, ( dot( GetRayDirection( myState ), normal ) > 0 ) );

		bool frontface = GetHitFrontface( TinyBVHIntersection );
		SetHitNormal( TinyBVHIntersection, frontface ? normal : -normal );
		SetHitIoR( TinyBVHIntersection, frontface ? IoR : ( 1.0f / IoR ) );

		// write it back to the buffer
		if ( hit.x > 0.0f ) {
			const int index = int( gl_GlobalInvocationID.x ) * NUM_INTERSECTORS + INTERSECT_TINYBVH;
			intersectionScratch[ index ] = TinyBVHIntersection;
		}
	}
}