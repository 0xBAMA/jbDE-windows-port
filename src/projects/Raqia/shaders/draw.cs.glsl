#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture; // moving away from using this, to a deferred setup
layout( binding = 2, rgba32ui ) uniform uimage2D deferredResult1;
layout( binding = 3, rgba32ui ) uniform uimage2D deferredResult2;
layout( binding = 4, r32ui ) uniform uimage2D deferredResult3;
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

uniform mat3 invBasis;
uniform float time;
uniform float scale;
uniform ivec2 blueNoiseOffset;
uniform ivec2 uvOffset;
uniform float globeIoR;
uniform float perspectiveFactor;

// DoF parameters
uniform float DoFRadius;
uniform float DoFDistance;

//=============================================================================================================================
// bayer matrix for indexing into the queues
const int bayerMatrix[ 16 ] = int [] (
	 0,  8,  2, 10,
	12,  4, 14,  6,
	 3, 11,  1,  9,
	15,  7, 13,  5
);
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
	// solve for jittered pixel uv, aspect ratio adjust
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const vec2 is = vec2( imageSize( accumulatorTexture ).xy );
	vec2 uv = scale * ( ( ( vec2( writeLoc + uvOffset ) + ( blue().xy - vec2( 0.5f ) ) ) / is ) - vec2( 0.5f ) );
	uv.y *= -float( is.y ) / float( is.x );

	// seeding the RNG
	seed = writeLoc.x * 6969 + writeLoc.y * 420 + blueNoiseOffset.x * 1313 + blueNoiseOffset.y * 31415;

	// initialize color value
	vec3 color = vec3( 0.0f, 0.0f, 0.0f );
	uvec4 deferredResultValue1 = uvec4( 0u );
	uvec4 deferredResultValue2 = uvec4( 0u );
	uvec4 deferredResultValue3 = uvec4( 0u );

	// initialize the surface ID, this will be kept unless updated later
	deferredResultValue1.w = NOHIT;

	// initial ray origin and direction
	vec3 rayOrigin = invBasis * vec3( uv, -2.0f );
	vec3 rayDirection = normalize( invBasis * vec3( perspectiveFactor * uv, 2.0f ) );

	if ( DoFRadius != 0.0f ) { // probably bring over more of the Voraldo13 camera https://github.com/0xBAMA/Voraldo13/blob/main/resources/engineCode/shaders/renderers/raymarch.cs.glsl#L70C28-L70C37

		// compute "perfect" ray
		vec2 uvNoJitter = scale * ( ( ( vec2( writeLoc + uvOffset ) + ( blue().zw - vec2( 0.5f ) ) ) / is ) - vec2( 0.5f ) );
		uvNoJitter.y *= -float( is.y ) / float( is.x );
	
		vec3 rayOriginNoJitter = invBasis * vec3( uvNoJitter, -2.0f );
		vec3 rayDirectionNoJitter = normalize( invBasis * vec3( perspectiveFactor * uvNoJitter, 2.0f ) );

		// DoF focus distance out along the ray
		vec3 focusPoint = rayOriginNoJitter + rayDirectionNoJitter * DoFDistance;

		// this is redundant, oh well
		uv = scale * ( ( ( vec2( writeLoc + uvOffset ) + DoFRadius * ( UniformSampleHexagon( blue().xy ) ) ) / is ) - vec2( 0.5f ) );
		uv.y *= -float( is.y ) / float( is.x );
		rayOrigin = invBasis * vec3( uv, -2.0f );
		rayDirection = normalize( focusPoint - rayOrigin );

	}

	// initial ray-sphere test against snowglobe
	vec4 initialSphereTest = sphereTrace( rayOrigin, rayDirection );

	// if the sphere test hits
	if ( initialSphereTest.x != MAX_DIST_CP ) {

		// update ray origin
		rayOrigin += rayDirection * initialSphereTest.x;

		// shoot a ray straight upwards... why is this negative? need to visualize positions and fix up some stuff
		vec3 skirtCheckOrigin = rayOrigin + 0.00001f * rayDirection;
		vec3 skirtCheckDirection = vec3( 0.0f, 0.0f, -1.0f );
		vec4 skirtCheckTerrain = terrainTrace( skirtCheckOrigin, skirtCheckDirection );
		vec4 skirtCheckSphere = sphereTrace( skirtCheckOrigin, skirtCheckDirection );

		// if you hit the ground
		if ( skirtCheckTerrain.x < skirtCheckSphere.x ) {

			// pixel gets skirts color... probably parameterize this
			color = vec3( 0.003f );
			deferredResultValue1.w = SKIRTS;

		} else {

			// refract the ray based on the sphere hit normal
			rayDirection = refract( rayDirection, initialSphereTest.yzw, 1.0f / globeIoR );

			// get new intersections with the intersectors
			vec4 terrainPrimaryHit = terrainTrace( rayOrigin, rayDirection );	// terrain
			vec4 SDFPrimaryHit = SDFTrace( rayOrigin, rayDirection );			// SDF
			vec4 grassPrimaryHit = grassTrace( rayOrigin, rayDirection );		// grass
			vec4 spherePrimaryHit = sphereTrace( rayOrigin, rayDirection );		// sphere
			
			// solve for minimum of the three distances
			float dClosest = min( min( terrainPrimaryHit.x, grassPrimaryHit.x ), min( spherePrimaryHit.x, SDFPrimaryHit.x ) );

			// if the sphere is not the closest of the three, we hit some surface
			if ( initialSphereTest.x < MAX_DIST_CP ) {

				// pull the vertex data for the hit terrain/grass triangles
				uint vertexIdx = 3 * floatBitsToUint( terrainPrimaryHit.w );
				vec3 vertex0t = triangleData[ vertexIdx + 0 ].xyz;
				vec3 vertex1t = triangleData[ vertexIdx + 1 ].xyz;
				vec3 vertex2t = triangleData[ vertexIdx + 2 ].xyz;
				// vec3 terrainColor = vec3( triangleData[ vertexIdx + 0 ].w, triangleData[ vertexIdx + 1 ].w, triangleData[ vertexIdx + 2 ].w ); // ... this adds a significant amount of time, will benefit from deferred
				vec3 terrainColor = honey / 50.0f;

				// the indexing for grass will change once grass blades have multiple tris
				vertexIdx = 4 * floatBitsToUint( grassPrimaryHit.w ); // stride of 4, caching the base point in the 4th coordinate
				vec3 vertex0g = triangleData2[ vertexIdx + 0 ].xyz;
				vec3 vertex1g = triangleData2[ vertexIdx + 1 ].xyz;
				vec3 vertex2g = triangleData2[ vertexIdx + 2 ].xyz;
				// vec3 grassColor = vec3( triangleData2[ vertexIdx + 0 ].w, triangleData2[ vertexIdx + 1 ].w, triangleData2[ vertexIdx + 2 ].w );
				vec3 grassColor = grassColorLeaf; // this is not nearly as bad for perf as the terrain bvh color stuff

				// solve for normal, baseColor
				vec3 normal = vec3( 0.0f );
				vec3 baseColor = vec3( 0.0f );
				uint idx = 0u;

				if ( terrainPrimaryHit.x == dClosest ) {

					// terrain is closest
					normal = normalize( cross( vertex1t - vertex0t, vertex2t - vertex0t ) );
					bool frontFace = dot( normal, rayDirection ) < 0.0f;
					normal = frontFace ? normal : -normal;
					idx = floatBitsToUint( terrainPrimaryHit.w );
					deferredResultValue1.w = TERRAIN;
					deferredResultValue3.x = packHalf2x16( terrainPrimaryHit.yz );

				} else if ( grassPrimaryHit.x == dClosest ) {

					// grass is closest
					normal = normalize( cross( vertex1g - vertex0g, vertex2g - vertex0g ) );
					bool frontFace = dot( normal, rayDirection ) < 0.0f;
					normal = frontFace ? normal : -normal;
					idx = floatBitsToUint( grassPrimaryHit.w );
					deferredResultValue1.w = GRASS;
					deferredResultValue3.x = packHalf2x16( grassPrimaryHit.yz );
					// baseColor = grassColor * ( 1.0f - grassPrimaryHit.z );

				} else if ( SDFPrimaryHit.x == dClosest ) {

					// SDF is closest
					normal = SDFPrimaryHit.yzw;
					baseColor = nickel;
					deferredResultValue1.w = SDF;

				} else {

					// hit the backface of the sphere
					normal = -spherePrimaryHit.yzw;
					deferredResultValue1.w = SPHERE;

				}

		// GBuffer data
			// result 1
				// .x is 4-byte encoded normal
				deferredResultValue1.x = encode( normal );

				// .y is 4-byte encoded post-refract ray direction
				deferredResultValue1.y = encode( rayDirection );

				// .z is the uint primitive ID
				deferredResultValue1.z = idx;

				// .w is going to indicate what object got hit ( NOHIT, SKIRTS, TERRAIN, GRASS, SDF, SPHEREBACKFACE, ... ? )

			// result 2
				// .x is distance traveled inside the sphere ( combine with ray direction to solve for position )
				deferredResultValue2.x = floatBitsToUint( dClosest );

				// .yzw is worldspace position... whatever
				rayOrigin = rayOrigin + rayDirection * dClosest * 0.99999f;
				deferredResultValue2.y = floatBitsToUint( rayOrigin.x );
				deferredResultValue2.z = floatBitsToUint( rayOrigin.y );
				deferredResultValue2.w = floatBitsToUint( rayOrigin.z );
			}
		}
	}

	// load previous color and blend with the result, write back to accumulator
	// vec4 previousColor = imageLoad( accumulatorTexture, writeLoc );
	// imageStore( accumulatorTexture, writeLoc, mix( vec4( color, 1.0f ), previousColor, blendAmount ) );

	// very minimal perf hit, since it's not raster... single write per pixel
	imageStore( deferredResult1, writeLoc, deferredResultValue1 );
	imageStore( deferredResult2, writeLoc, deferredResultValue2 );
	imageStore( deferredResult3, writeLoc, deferredResultValue3 );
}
