#version 430
layout( local_size_x = 4, local_size_y = 16, local_size_z = 1 ) in;
//=============================================================================================================================
// environmental
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
// film plane state - film plane is 3x as wide as it would be otherwise, to accomodate the separate channels
layout( binding = 3, r32ui ) uniform uimage2D filmPlaneImage;
//=============================================================================================================================
#include "mathUtils.h"
#include "spectrumXYZ.h"
#include "colorspaceConversions.glsl"
//=============================================================================================================================
#include "random.h"
uniform uint seedValue;
//=============================================================================================================================
float myWavelength = 0.0f;
//=============================================================================================================================
struct lightSpecGPU {
	// less than ideal way to do this...
	vec4 typeVec;		// emitter type, LUT type, 0, 0
	vec4 parameters0;	// varies
	vec4 parameters1;	// varies
	vec4 pad;			// zeroes

	// so, everything we need to know:
		// type of light ( 1 float -> int )
		// type of emitter ( 1 float -> int )
		// emitter parameterization
			// varies by emitter...

	// Parameters:
		// point emitter has 1x vec3 parameter
		// cauchy beam has 2x vec3 parameters and 1x float parameter
		// laser disk has 2x vec3 parameters and 1x float parameter
		// uniform line emitter has 2x vec3 parameters...
};

layout( binding = 0, std430 ) readonly buffer lightBuffer {
	int lightIStructure[ 1024 ]; // we uniformly sample an index out of this list of 1024 to know which light we want to pick...
	lightSpecGPU lightList[]; // we do not need to know how many lights exist, because it is implicitly encoded in the importance structure's indexing
};
//=============================================================================================================================
layout( binding = 1, std430 ) readonly buffer cwbvhNodesBuffer { vec4 cwbvhNodes[]; };
layout( binding = 2, std430 ) readonly buffer cwbvhTrisBuffer { vec4 cwbvhTris[]; };
layout( binding = 3, std430 ) readonly buffer triangleDataBuffer { vec4 triangleData[]; };

// setup for preprocessor operation
#define NODEBUFFER cwbvhNodes
#define TRIBUFFER cwbvhTris
#define TRAVERSALFUNC traverse_cwbvh
#include "traverse.h" // all support code for CWBVH8 traversal

// helpers for computing rD, reciprocal direction
float tinybvh_safercp( const float x ) { return x > 1e-12f ? ( 1.0f / x ) : ( x < -1e-12f ? ( 1.0f / x ) : 1e30f ); }
vec3 tinybvh_safercp( const vec3 x ) { return vec3( tinybvh_safercp( x.x ), tinybvh_safercp( x.y ), tinybvh_safercp( x.z ) ); }
//=============================================================================================================================

// keep some global state for hit color, normal, etc
vec3 hitNormal = vec3( 0.0f );
uint hitID = 0u;
float hitAlbedo = 1.0f;
float sceneIntersection( vec3 rO, vec3 rD ) {
	// return value of this function has distance, 2d barycentrics, then uintBitsToFloat(triangleID)...
		// I think the most straightforward way to get the normal will just be from the triangle buffer

	vec4 result = traverse_cwbvh( rO, rD, tinybvh_safercp( rD ), 1e30f );

	// placeholder
	hitAlbedo = 0.9f;

	hitID = floatBitsToUint( result.w );

	vec3 a, b, c;
	a = triangleData[ 3 * hitID ].xyz;
	b = triangleData[ 3 * hitID + 1 ].xyz;
	c = triangleData[ 3 * hitID + 2 ].xyz;

	// cross product of the two edges gives us a potential normal vector
	hitNormal = normalize( cross( a - c, b - c ) );

	// need to invert if we created an opposite-facing normal
	if ( dot( rD, hitNormal ) > 0.0f ) hitNormal = -hitNormal;

	return result.r;
}

// buffer for tinyBVH... can we try doing the TLAS thing this time? I'm not sure what's involved...
	// wrapper function for scene intersection...

void main () {
	// my invocation...
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// seeding the RNG process...
	seed = seedValue + loc.x + 42069 + loc.y * 31415;

	// pick a light source that we are starting from... this is importance sampling by "power"
	lightSpecGPU pickedLight = lightList[ lightIStructure[ wangHash() % 1024 ] ];

	// generate a new ray, based on the properties of the selected light...
	// what is my starting position, direction?
	vec3 rO = vec3( 0.0f ), rD = vec3( 0.0f );
	switch ( int( pickedLight.typeVec.x ) ) { // based on the type of emitter specified...
	case 0: // point light
		rO = pickedLight.parameters0.xyz;
		rD = RandomUnitVector();
		break;

	case 1: // cauchy beam
		// emitting from a single point
		r0 = pickedLight.parameters0.xyz;
		// we need to be able to place a jittered target position...
		vec3 x, y;
		createBasis( normalize( pickedLight.parameters1.xyz ), x, y );
		vec2 c = rnd_disc_cauchy();
		rD = normalize( pickedLight.parameters0.w * ( x * c.x + y * c.y ) + pickedLight.parameters1.xyz );
		break;

	case 2: // laser disk
		// similar to above, but using a constant direction value, and using the basis jitter for a disk offset
		vec3 x, y;
		createBasis( normalize( pickedLight.parameters1.xyz ), x, y );
		vec2 c = randCircle();
		rO = pickedLight.parameters0.xyz + pickedLight.parameters0.w * ( x * c.x + y * c.y );
		// emitting along a single direction vector
		rD = normalize( pickedLight.parameters1.xyz );
		break;

	case 3: // uniform line emitter
		r0 = mix( pickedLight.parameters0.xyz, pickedLight.parameters1.xyz, NormalizedRandomFloat() );
		rD = RandomUnitVector();
		break;

	default:
		break;
	}

	// what is my wavelength?
	float myWavelength = getWavelengthForLight( int( pickedLight.typeVec.y ) );

	// initialize pathtracing state for the given ray initialization

	// tracing paths, for N bounces
	for ( int b = 0; b < maxBounces; b++ ) {

		// scene intersection ( BVH, DDA maybe eventually )

		// ray hits film plane? if so, it tallies energy contribution and dies

		// material evaluation, if the ray is going to continue

		// ...

	}
	// if we fall out without hitting the film plane... nothing special happens
}
