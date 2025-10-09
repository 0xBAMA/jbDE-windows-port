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
//=============================================================================================================================
uniform vec3 viewerPosition;
uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;
bool hitFilmPlane ( in vec3 rO, in vec3 rD, in float maxDistance, in float energy, in float wavelength ) {
	// we are going to define basically an arbitrary plane location, for now... eventually we will need this to be more interactive
		// the plane will only accept forward hits, closer than the max specified distance

	vec3 planePoint = viewerPosition;
	vec3 planeNormal = basisZ;

	const float planeDistance = -( dot( rO - planePoint, planeNormal ) ) / dot( rD, planeNormal );

	// hitting front side of plane, and hitting with a positive distance...
	if ( ( dot( rD, planeNormal ) < 0.0f ) && planeDistance > 0.0f && planeDistance < maxDistance ) {
		// we hit the plane... where?
		vec3 x = basisX, y = basisY;

		// we're going to then solve for basically a pixel index...
		const vec3 pHit = rO + planeDistance * rD;

		// we know where we hit. We know the base point of the plane. We're going to mask a portion of the plane to represent the image...
			// we have to decompose the offset from the point we hit to the base point of the plane, which will be the center of the image...
		const vec3 pHitOffset = pHit - planePoint;
		const vec2 pHitOffsetXY = vec2( dot( pHitOffset, x ), dot( pHitOffset, y ) );

		const vec2 iS = vec2( imageSize( filmPlaneImage ).xy ) * vec2( 1.0f / 3.0f, 1.0f ) * filmScale;
		const vec2 iShalf = iS / 2.0f;
		if ( all( lessThanEqual( abs( pHitOffsetXY - iShalf ), iS ) ) ) {
			// we hit a valid pixel... we need to apply an energy increment
			for ( int i = 0; i < 8; i++ ) {
				ivec2 pixelSelect = ivec2( 0.01f * rnd_disc_cauchy() + ( pHitOffsetXY + iShalf ) / filmScale );
				vec3 XYZColor = 0.01f * RandomUnitVector() + rgb_to_srgb( xyz_to_rgb( energy * wavelengthColor( wavelength ) ) );
				imageAtomicAdd( filmPlaneImage, ivec2( 3, 1 ) * pixelSelect, uint( 256 * XYZColor.x ) );
				imageAtomicAdd( filmPlaneImage, ivec2( 3, 1 ) * pixelSelect + ivec2( 1, 0 ), uint( 256 * XYZColor.y ) );
				imageAtomicAdd( filmPlaneImage, ivec2( 3, 1 ) * pixelSelect + ivec2( 2, 0 ), uint( 256 * XYZColor.z ) );
			}
			return true;
		}
	}
	return false;
}
//=============================================================================================================================
// sampling the iCDF texture with a random offset on x will give us importance sampled wavelengths for different light sources
uniform sampler2D lightICDF;
float getWavelengthForLight( int selectedLight ) {
	return texture( lightICDF, vec2( NormalizedRandomFloat(), ( selectedLight + 0.5f ) / textureSize( lightICDF, 0 ).y ) ).r;
}
//=============================================================================================================================
void main () {
	// my invocation...
	const ivec3 loc = ivec3( gl_GlobalInvocationID.xyz );

	// seeding the RNG process...
	seed = seedValue + loc.x + 42069 + loc.y * 31415 + loc.z * 45081;

	// pick a light source that we are starting from... this is importance sampling by "power"
	lightSpecGPU pickedLight = lightList[ lightIStructure[ wangHash() % 1024 ] ];

	// generate a new ray, based on the properties of the selected light...
	// what is my starting position, direction?
	vec3 rO = vec3( 0.0f ), rD = vec3( 0.0f );
	vec3 x, y;
	vec2 c;
	switch ( int( pickedLight.typeVec.x ) ) { // based on the type of emitter specified...
	case 0: // point light
		rO = pickedLight.parameters0.xyz;
		rD = RandomUnitVector();
		break;

	case 1: // cauchy beam
		// emitting from a single point
		rO = pickedLight.parameters0.xyz;
		// we need to be able to place a jittered target position... it's a 2D offset in the plane whose normal is defined by the beam direction
		createBasis( normalize( pickedLight.parameters1.xyz ), x, y );
		c = rnd_disc_cauchy();
		rD = normalize( pickedLight.parameters0.w * ( x * c.x + y * c.y ) + pickedLight.parameters1.xyz );
		break;

	case 2: // laser disk
		// similar to above, but using a constant direction value, and using the basis jitter for a scaled disk offset
		createBasis( normalize( pickedLight.parameters1.xyz ), x, y );
		c = CircleOffset();
		rO = pickedLight.parameters0.xyz + pickedLight.parameters0.w * ( x * c.x + y * c.y );
		// emitting along a single direction vector
		rD = normalize( pickedLight.parameters1.xyz );
		break;

	case 3: // uniform line emitter
		rO = mix( pickedLight.parameters0.xyz, pickedLight.parameters1.xyz, NormalizedRandomFloat() );
		rD = RandomUnitVector();
		break;

	default:
		break;
	}

	// what is my wavelength?
	myWavelength = getWavelengthForLight( int( pickedLight.typeVec.y ) );

	// initialize pathtracing state for the given ray initialization
	float energy = 1.0f;

	// tracing paths, for N bounces
	const int maxBounces = 64;
	for ( int b = 0; b < maxBounces; b++ ) {

		// scene intersection ( BVH, DDA maybe eventually )
		float dIntersection = sceneIntersection( rO, rD );

		if ( hitFilmPlane( rO, rD, dIntersection, energy, myWavelength ) || energy == 0.0f ) {
		// ray hits film plane? if so, it tallies energy contribution and dies... this also catches dead rays (fully attenuated)
			break;
		} else {
		// material evaluation, update r0, rD, if the ray is going to continue
			rO = rO + dIntersection * rD + 0.00001f * hitNormal;
			energy *= hitAlbedo;

			switch ( hitMaterial ) {
				case NOHIT: {
				energy = 0.0f;
				break;
				}

				case DIFFUSE: {
				rD = RandomUnitVector();
				// flip if it's the opposite direction from the normal
				if ( dot( hitNormal, rD ) < 0.0f )
					rD = -rD;
				break;
				}

				case METALLIC: {
				// todo
				break;
				}

				case MIRROR: {
				rD = reflect( rD, hitNormal );
				break;
				}

				default: { // all refractive materials

					// additional correction for glass...
					rO -= hitNormal * 0.001f;

					float myIoR = getIORForMaterial( hitMaterial );
					myIoR = hitFrontface ? ( myIoR ) : ( 1.0f / myIoR ); // "reverse" back to physical properties for IoR

					float cosTheta = min( dot( -normalize( rD ), hitNormal ), 1.0f );
					float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
					bool cannotRefract = ( myIoR * sinTheta ) > 1.0f; // accounting for TIR effects
					if ( cannotRefract || Reflectance( cosTheta, myIoR ) > NormalizedRandomFloat() ) {
						rD = normalize( mix( reflect( normalize( rD ), hitNormal ), RandomUnitVector(), hitRoughness ) );
					} else {
						rD = normalize( mix( refract( normalize( rD ), hitNormal, myIoR ), RandomUnitVector(), hitRoughness ) );
					}
					break;
				}
			}
		}
	}
	// if we fall out without hitting the film plane... nothing special happens
}
