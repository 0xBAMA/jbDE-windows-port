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
const float epsilon = 0.00001f;
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
layout( binding = 4, rg32ui ) readonly uniform uimage3D SpherePack;
//=============================================================================================================================
// copied straight from path2Dforward, will need to refine eventually
#define NOHIT						0
#define DIFFUSE						1
#define METALLIC					2
#define MIRROR						3

// air reserve value
#define AIR							5
// below this point, we have specific forms of glass
#define CAUCHY_FUSEDSILICA			6
#define CAUCHY_BOROSILICATE_BK7		7
#define CAUCHY_HARDCROWN_K5			8
#define CAUCHY_BARIUMCROWN_BaK4		9
#define CAUCHY_BARIUMFLINT_BaF10	10
#define CAUCHY_DENSEFLINT_SF10		11
// more coefficients available at https://web.archive.org/web/20151011033820/http://www.lacroixoptical.com/sites/default/files/content/LaCroix%20Dynamic%20Material%20Selection%20Data%20Tool%20vJanuary%202015.xlsm
#define SELLMEIER_BOROSILICATE_BK7	12
#define SELLMEIER_SAPPHIRE			13
#define SELLMEIER_FUSEDSILICA		14
#define SELLMEIER_MAGNESIUMFLOURIDE	15

// getting the wavelength-dependent IoR for materials
float evaluateCauchy ( float A, float B, float wms ) {
	return A + B / wms;
}

float evaluateSellmeier ( vec3 B, vec3 C, float wms ) {
	return sqrt( 1.0f + ( wms * B.x / ( wms - C.x ) ) + ( wms * B.y / ( wms - C.y ) ) + ( wms * B.z / ( wms - C.z ) ) );
}

// support for glass behavior
float Reflectance ( const float cosTheta, const float IoR ) {
	#if 0
	// Use Schlick's approximation for reflectance
	float r0 = ( 1.0f - IoR ) / ( 1.0f + IoR );
	r0 = r0 * r0;
	return r0 + ( 1.0f - r0 ) * pow( ( 1.0f - cosTheta ), 5.0f );
	#else
	// "Full Fresnel", from https://www.shadertoy.com/view/csfSz7
	float g = sqrt( IoR * IoR + cosTheta * cosTheta - 1.0f );
	float a = ( g - cosTheta ) / ( g + cosTheta );
	float b = ( ( g + cosTheta ) * cosTheta - 1.0f ) / ( ( g - cosTheta ) * cosTheta + 1.0f );
	return 0.5f * a * a * ( 1.0f + b * b );
	#endif
	//	another expression used here... https://www.shadertoy.com/view/wlyXzt - what's going on there?
}

float getIORForMaterial ( int material ) {
	// There are a couple ways to get IoR from wavelength
	float wavelengthMicrons = myWavelength / 1000.0f;
	const float wms = wavelengthMicrons * wavelengthMicrons;

	float IoR = 0.0f;
	switch ( material ) {
		// Cauchy second order approx
		case CAUCHY_FUSEDSILICA:			IoR = evaluateCauchy( 1.4580f, 0.00354f, wms ); break;
		case CAUCHY_BOROSILICATE_BK7:		IoR = evaluateCauchy( 1.5046f, 0.00420f, wms ); break;
		case CAUCHY_HARDCROWN_K5:			IoR = evaluateCauchy( 1.5220f, 0.00459f, wms ); break;
		case CAUCHY_BARIUMCROWN_BaK4:		IoR = evaluateCauchy( 1.5690f, 0.00531f, wms ); break;
		case CAUCHY_BARIUMFLINT_BaF10:		IoR = evaluateCauchy( 1.6700f, 0.00743f, wms ); break;
		case CAUCHY_DENSEFLINT_SF10:		IoR = evaluateCauchy( 1.7280f, 0.01342f, wms ); break;
		// Sellmeier third order approx
		case SELLMEIER_BOROSILICATE_BK7:	IoR = evaluateSellmeier( vec3( 1.03961212f, 0.231792344f, 1.01046945f ), vec3( 1.01046945f, 6.00069867e-3f, 2.00179144e-2f ), wms ); break;
		case SELLMEIER_SAPPHIRE:			IoR = evaluateSellmeier( vec3( 1.43134930f, 0.650547130f, 5.34140210f ), vec3( 5.34140210f, 5.27992610e-3f, 1.42382647e-2f ), wms ); break;
		case SELLMEIER_FUSEDSILICA:			IoR = evaluateSellmeier( vec3( 0.69616630f, 0.407942600f, 0.89747940f ), vec3( 0.89747940f, 0.00467914800f, 0.01351206000f ), wms ); break;
		case SELLMEIER_MAGNESIUMFLOURIDE:	IoR = evaluateSellmeier( vec3( 0.48755108f, 0.398750310f, 2.31203530f ), vec3( 2.31203530f, 0.00188217800f, 0.00895188800f ), wms ); break;
		default: IoR = 1.0f;
	}

	return IoR;
}

bool isRefractive ( int id ) {
	return id >= CAUCHY_FUSEDSILICA;
}

#include "intersect.h"
vec3 spherePackNormal = vec3( 0.0f );
float spherePackAlbedo = 0.0f;
bool spherePackFrontface = false;
float spherePackDTravel = -10000.0f;
float spherePackRoughness = 0.0f;
int spherePackMaterialID = AIR;

//=============================================================================================================================
//	explicit intersection primitives
//		refactor this, or put it in a header, it's a bunch of ugly shit
//=============================================================================================================================
struct iqIntersect {
	vec4 a;  // distance and normal at entry
	vec4 b;  // distance and normal at exit
};

// false Intersection
const iqIntersect kEmpty = iqIntersect(
vec4(  1e20f, 0.0f, 0.0f, 0.0f ),
vec4( -1e20f, 0.0f, 0.0f, 0.0f )
);

bool IsEmpty( iqIntersect i ) {
	return i.b.x < i.a.x;
}

iqIntersect IntersectSphere ( in vec3 rO, in vec3 rD, in vec3 center, float radius ) {
	// https://iquilezles.org/articles/intersectors/
	vec3 oc = rO - center;
	float b = dot( oc, rD );
	float c = dot( oc, oc ) - radius * radius;
	float h = b * b - c;
	if ( h < 0.0f ) return kEmpty; // no intersection
	h = sqrt( h );
	// h is known to be positive at this point, b+h > b-h
	float nearHit = -b - h; vec3 nearNormal = normalize( ( rO + rD * nearHit ) - center );
	float farHit  = -b + h; vec3 farNormal  = normalize( ( rO + rD * farHit ) - center );
	return iqIntersect( vec4( nearHit, nearNormal ), vec4( farHit, farNormal ) );
}

bool SpherePackDDA( in vec3 rO, in vec3 rD, in float maxDistance ) {
	// box intersection
	float tMin, tMax;
	const ivec3 iS = imageSize( SpherePack ).xyz;
	const float scale = 0.01f;
	const vec3 blockSize = vec3( iS ) * scale;
	const vec3 blockSizeHalf = vec3( blockSize ) / 2.0f;

	// then intersect with the AABB
	const bool hit = IntersectAABB( rO, rD, -blockSizeHalf, blockSizeHalf, tMin, tMax );
	const bool behindOrigin = ( tMin < 0.0f && tMax < 0.0f );
	const bool backface = ( tMin < 0.0f && tMax >= 0.0f );

	float dTravel = 1e30f;

	if ( hit && !behindOrigin ) { // texture sample
		// get a sample point in grid space... start at ray origin if you see a backface, or at the closest positive hit
		vec3 p = backface ? rO : ( rO + tMin * rD );
		const vec3 pCache = p;
		p = vec3(
			RangeRemapValue( p.x, -blockSizeHalf.x, blockSizeHalf.x, epsilon, iS.x - epsilon ),
			RangeRemapValue( p.y, -blockSizeHalf.y, blockSizeHalf.y, epsilon, iS.y - epsilon ),
			RangeRemapValue( p.z, -blockSizeHalf.z, blockSizeHalf.z, epsilon, iS.z - epsilon )
		);

		const uvec4 read = imageLoad( SpherePack, ivec3( p ) );
		float radius = uintBitsToFloat( read.r );
		const uint seedCache = seed;
		seed = read.g; // evaluating deterministic radii
		vec3 center = vec3( 5.0f ) + vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() ) * ( iS - vec3( 10.0f ) );
		seed = seedCache;

		// do the traversal
		// from https://www.shadertoy.com/view/7sdSzH
		vec3 deltaDist = 1.0f / abs( rD );
		ivec3 rayStep = ivec3( sign( rD ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( p + 0.0f ) );
		vec3 sideDist0 = ( sign( rD ) * ( vec3( mapPos0 ) - p ) + ( sign( rD ) * 0.5f ) + 0.5f ) * deltaDist;

		for ( int i = 0; i < 1000 && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, iS ) ) ); i++ ) {
			// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
			bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
			vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
			ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

			// consider using distance to bubble hit, when bubble is enabled
			uvec4 read = imageLoad( SpherePack, mapPos0 );
			if ( read.g != 0 ) { // this might be a hit condition
				float radius = uintBitsToFloat( read.r );
				const uint seedCache = seed;
				seed = read.g; // evaluating deterministic radii
				vec3 center = vec3( 5.0f ) * scale + vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() ) * ( blockSize - vec3( 10.0f ) * scale );
				seed = seedCache;

				iqIntersect test = IntersectSphere( rO, rD, center - blockSizeHalf, radius * scale );
				const bool behindOrigin = ( test.a.x < 0.0f && test.b.x < 0.0f );

				if ( !IsEmpty( test ) && !behindOrigin ) {
					// if you get a hit, fill out the details
					spherePackFrontface = !( test.a.x < 0.0f && test.b.x >= 0.0f );
					spherePackDTravel = ( spherePackFrontface ? test.a.x : test.b.x );
					spherePackNormal = normalize( spherePackFrontface ? test.a.yzw : -test.b.yzw );
					spherePackMaterialID = SELLMEIER_BOROSILICATE_BK7;
					spherePackAlbedo = 0.9f;
					spherePackRoughness = 0.0f;
					break;
				}
			}
			sideDist0 = sideDist1;
			mapPos0 = mapPos1;
		}
		// otherwise, fall through with a default intersection result
	}

	return spherePackDTravel < maxDistance;
}


//=============================================================================================================================
// keep some global state for hit color, normal, etc
vec3 hitNormal = vec3( 0.0f );
uint hitID = 0u;
float hitAlbedo = 1.0f;
int hitMaterial = MIRROR;
float hitRoughness = 0.0f;
bool hitFrontface = false;
//=============================================================================================================================
//=============================================================================================================================
float sceneIntersection( vec3 rO, vec3 rD ) {
	// return value of this function has distance, 2d barycentrics, then uintBitsToFloat(triangleID)...
		// I think the most straightforward way to get the normal will just be from the triangle buffer

	vec4 result = traverse_cwbvh( rO, rD, tinybvh_safercp( rD ), 1e30f );

	// placeholder
	hitAlbedo = 0.9f;
	hitRoughness = 0.0f;
	hitMaterial = MIRROR;

	hitID = floatBitsToUint( result.w );

	vec3 a, b, c;
	a = triangleData[ 3 * hitID ].xyz;
	b = triangleData[ 3 * hitID + 1 ].xyz;
	c = triangleData[ 3 * hitID + 2 ].xyz;
	hitNormal = normalize( cross( a - c, b - c ) ); // cross product of the two edges gives us a potential normal vector
	if ( dot( rD, hitNormal ) < 0.0f ) hitFrontface = false, hitNormal = -hitNormal; // need to invert if we created an opposite-facing normal

	 if ( SpherePackDDA( rO, rD, result.r ) ) {
//	if ( SpherePackDDA( rO, rD, 1e30f ) ) {
		// we hit a sphere, closer than the BVH
		hitAlbedo = spherePackAlbedo;
		hitMaterial = spherePackMaterialID;
		hitRoughness = spherePackRoughness;
		hitNormal = spherePackNormal;
		if ( dot( rD, spherePackNormal ) < 0.0f )
			hitNormal = -spherePackNormal, hitFrontface = false;
		return spherePackDTravel;
	} else {
		return result.r;
		// return 1e30f;
	}
}
//=============================================================================================================================
uniform vec3 viewerPosition;
uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;
uniform float filmScale;
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
			for ( int i = 0; i < 32; i++ ) {
				ivec2 pixelSelect = ivec2( 0.1f * rnd_disc_cauchy() + ( pHitOffsetXY + iShalf ) / filmScale );
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
			rO = rO + dIntersection * rD + epsilon * hitNormal;
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
					rO -= hitNormal * 3.0f * epsilon;

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
