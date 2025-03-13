#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
//=============================================================================================================================
// gpu-side code for ray-BVH traversal
	// used for computing rD, reciprocal direction
float tinybvh_safercp( const float x ) { return x > 1e-12f ? ( 1.0f / x ) : ( x < -1e-12f ? ( 1.0f / x ) : 1e30f ); }
vec3 tinybvh_safercp( const vec3 x ) { return vec3( tinybvh_safercp( x.x ), tinybvh_safercp( x.y ), tinybvh_safercp( x.z ) ); }
//=============================================================================================================================

// leaving the binding points intact, in case we want to incorporate more dynamics here, collision, something like that

// node and triangle data specifically for the BVH
layout( binding = 0, std430 ) readonly buffer cwbvhNodesBuffer { vec4 cwbvhNodes[]; };
layout( binding = 1, std430 ) readonly buffer cwbvhTrisBuffer { vec4 cwbvhTris[]; };
// vertex data for the individual triangles' vertices, in a usable format ( .w can be used to pack more data )
layout( binding = 2, std430 ) readonly buffer triangleDataBuffer { vec4 triangleData[]; };

// second set, for the grass blades
layout( binding = 3, std430 ) readonly buffer cwbvhNodesBuffer2 { vec4 cwbvhNodes2[]; };
layout( binding = 4, std430 ) readonly buffer cwbvhTrisBuffer2 { vec4 cwbvhTris2[]; };
layout( binding = 5, std430 ) buffer triangleDataBuffer2 { vec4 triangleData2[]; };
//=============================================================================================================================
#include "consistentPrimitives.glsl.h" // ray-sphere, ray-box inside traverse.h
#include "noise.h"

#define NODEBUFFER cwbvhNodes
#define TRIBUFFER cwbvhTris
#define TRAVERSALFUNC traverse_cwbvh_terrain

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC

// /* what is the return type? */ leafTestFunc ( /* what is the parameterization? rO, rD, index of leaf node */ ) {
	// test against N triangles for one blade of grass
// }

#define NODEBUFFER cwbvhNodes2
#define TRIBUFFER cwbvhTris2
#define TRAVERSALFUNC traverse_cwbvh_grass
// #define CUSTOMLEAFTEST // leafTestFunc

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC
#undef CUSTOMLEAFTEST

#include "random.h"

//=============================================================================================================================
// offsetting the blue noise
uniform vec2 blueNoiseOffset;

// used for sampling the noise
uniform vec3 noiseOffset0;
uniform vec3 noiseOffset1;
uniform vec2 noiseScalars;

// used to scale the noise contribution to the grass blades
uniform vec2 displacementScalars;

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

//=============================================================================================================================

void main () {
	// compute the index into the list of triangle data
	uint index = ( gl_GlobalInvocationID.x ) + ( 4096 * gl_GlobalInvocationID.y );

// we need a couple pieces of information
	// base point, from which to displace
	vec3 basePoint = triangleData2[ 4 * index + 3 ].xyz;

	// three noise reads, for the displacement on each axis
	vec3 noiseReads = vec3(
	#if 0 // this is more efficient, since a smaller range of motion means tighter bounds... requires the corresponding change on the CPU side
		abs( displacementScalars.x * perlinfbm( basePoint + noiseOffset0, noiseScalars.x, 2 ) ),
		abs( displacementScalars.y * perlinfbm( basePoint + noiseOffset1, noiseScalars.y, 2 ) ),
	#else
		displacementScalars.x * perlinfbm( basePoint + noiseOffset0, noiseScalars.x, 2 ) ),
		displacementScalars.y * perlinfbm( basePoint + noiseOffset1, noiseScalars.y, 2 ) ),
	#endif
		0.0f );

	// writeback vertex 0's displaced point
	triangleData2[ 4 * index + 2 ].xyz = basePoint + noiseReads;
}
