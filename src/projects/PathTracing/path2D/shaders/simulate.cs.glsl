#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) uniform image2D bufferImage;

uniform float t;
uniform int rngSeed;

// I would like to convert to ray-primitive intersection, instead of raymarching
float rayPlaneIntersect ( in vec3 rayOrigin, in vec3 rayDirection ) {
	const vec3 normal = vec3( 0.0f, 1.0f, 0.0f );
	const vec3 planePt = vec3( 0.0f, 0.0f, 0.0f ); // not sure how far down this should be
	return -( dot( rayOrigin - planePt, normal ) ) / dot( rayDirection, normal );
}

#include "random.h"
#include "mathUtils.h"
#include "spectrumXYZ.h"
#include "colorspaceConversions.glsl"
#include "hg_sdf.glsl"

// drawing inspiration from https://www.shadertoy.com/view/M3jcDW
	// they use a "presence" term, which projects one color onto another to judge similarity... their usage is as a
	// russian roulette term, but I think it would work as a simple way to implement float-valued albedo, in terms of wavelength

float presence ( vec3 a, vec3 b ) { // call presence( a, b ) to see if
	return clamp( pow( dot( a, b ) / dot( a, vec3( 1.0f ) ), 3.0f ), 0.0f, 1.0f );
}

// trace against the scene
	// this abstraction makes it easier to target raymarch/other scene intersection methods when I want to swap it out
#define NOHIT						0
#define EMISSIVE					1
#define DIFFUSE						2
#define METALLIC					3
#define MIRROR						4

// below this point, we have specific forms of glass
#define CAUCHY_FUSEDSILICA			5
#define CAUCHY_BOROSILICATE_BK7		6
#define CAUCHY_HARDCROWN_K5			7
#define CAUCHY_BARIUMCROWN_BaK4		8
#define CAUCHY_BARIUMFLINT_BaF10	9
#define CAUCHY_DENSEFLINT_SF10		10
// more coefficients available at https://web.archive.org/web/20151011033820/http://www.lacroixoptical.com/sites/default/files/content/LaCroix%20Dynamic%20Material%20Selection%20Data%20Tool%20vJanuary%202015.xlsm
#define SELLMEIER_BOROSILICATE_BK7	11
#define SELLMEIER_SAPPHIRE			12
#define SELLMEIER_FUSEDSILICA		13
#define SELLMEIER_MAGNESIUMFLOURIDE	14

struct intersectionResult {
	// scene intersection representation etc loosely based on Daedalus
	float dist;
	float albedo;
	float IoR;
	float roughness;
	vec2 normal;
	bool frontFacing;
	int materialType;
};

intersectionResult getDefaultIntersection () {
	intersectionResult result;
	result.dist = 0.0f;
	result.albedo = 0.0f;
	result.IoR = 0.0f;
	result.roughness = 0.0f;
	result.normal = vec2( 0.0f );
	result.frontFacing = false;
	result.materialType = NOHIT;
	return result;
}

// for the values below that depend on access to the wavelength
float wavelength;

// global state tracking
int hitSurfaceType = 0;
float hitRoughness = 0.0f;
float hitAlbedo = 0.0f;

// raymarch parameters
const float epsilon = 0.00005f;
const float maxDistance = 10000.0f;
const int maxSteps = 1000;


// getting the wavelength-dependent IoR for materials
float evaluateCauchy ( float A, float B, float wms ) {
	return A + B / wms;
}

float evaluateSellmeier ( vec3 B, vec3 C, float wms ) {
	return sqrt( 1.0f + ( wms * B.x / ( wms - C.x ) ) + ( wms * B.y / ( wms - C.y ) ) + ( wms * B.z / ( wms - C.z ) ) );
}

float getIORForMaterial ( int material ) {
	// There are a couple ways to get IoR from wavelength
	float wavelengthMicrons = wavelength / 1000.0f;
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

float rectangle ( vec2 samplePosition, vec2 halfSize ) {
	vec2 componentWiseEdgeDistance = abs( samplePosition ) - halfSize;
	float outsideDistance = length( max( componentWiseEdgeDistance, 0 ) );
	float insideDistance = min( max( componentWiseEdgeDistance.x, componentWiseEdgeDistance.y ), 0 );
	return outsideDistance + insideDistance;
}

// should we invert the lenses?
bool invert = false;

float de ( vec2 p ) {
	const vec2 pOriginal = p;
	float sceneDist = 100000.0f;

	hitAlbedo = 0.0f;
	hitSurfaceType = NOHIT;
	hitRoughness = 0.0f;

	if ( false ) { // an example object (emissive)
		const float d = distance( p, vec2( -250.0f, -250.0f ) ) - 5.0f;
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = EMISSIVE;
			hitAlbedo = 1.0f * RangeRemapValue( wavelength, 300, 900, 0.0f, 1.0f );
		}
	}

	if ( false ) { // an example object (emissive)
		const float d = min( distance( p, vec2( -460.0f, 85.0f ) ) - 20.0f, distance( p, vec2(  460.0f, 115.0f ) ) - 20.0f );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = EMISSIVE;
			hitAlbedo = 1.0f * RangeRemapValue( wavelength, 300, 900, 1.0f, 0.0f );
		}
	}

	if ( true ) {
		pModInterval1( p.x, 700.0f, -1.0f, 1.0f );
		const float d = min( distance( p, vec2( 0.0f, 350.0f ) ), distance( p, vec2( 0.0f, -350.0f ) ) ) - 10.0f;
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = EMISSIVE;
			hitAlbedo = 0.9f;
		}
	}

	p = Rotate2D( 0.3f ) * pOriginal;
	vec2 gridIndex;
	gridIndex.x = pModInterval1( p.x, 100.0f, -10.0f, 10.0f );
	gridIndex.y = pModInterval1( p.y, 100.0f, -6.0f, 6.0f );

	{ // an example object (refractive)
		bool checker = checkerBoard( 1.0f, vec3( gridIndex / 3.0f + vec2( 0.1f ), 0.5f ) );
		const float d = ( invert ? -1.0f : 1.0f ) * ( checker ? ( rectangle( p, vec2( checker ? 20.0f : 32.0f ) ) ) : ( distance( p, vec2( 0.0f ) ) - ( checker ? 20.0f : 32.0f ) ) );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			if ( checker ) {
				// hitSurfaceType = SELLMEIER_BOROSILICATE_BK7;
				hitSurfaceType = MIRROR;
				hitAlbedo = 1.0f;
			} else {
				hitSurfaceType = SELLMEIER_BOROSILICATE_BK7;
				bool checker2 =  checkerBoard( 1.0f, vec3( gridIndex / 2.0f + vec2( 0.1f ), 0.5f ) );
				// hitAlbedo = 1.0f * RangeRemapValue( wavelength, 300, 900, checker2 ? 0.5f : 1.0f, checker2 ? 1.0f : 0.3f );
				hitAlbedo = 1.0f;
			}
		}
	}

	// get back final result
	return sceneDist;
}

// function to get the normal
vec2 SDFNormal ( vec2 p ) {
	const vec2 k = vec2( 1.0f, -1.0f );
	return normalize(
		k.xx * de( p + k.xx * epsilon ).x +
		k.xy * de( p + k.xy * epsilon ).x +
		k.yx * de( p + k.yx * epsilon ).x +
		k.yy * de( p + k.yy * epsilon ).x );
}

// trace against the scene
intersectionResult sceneTrace ( vec2 rayOrigin, vec2 rayDirection ) {
	intersectionResult result = getDefaultIntersection();

	// is the initial sample point inside? -> toggle invert so we correctly handle refractive objects
	if ( de( rayOrigin ) < 0.0f ) { // this is probably a solution for the same problem in Daedalus, too...
		invert = !invert;
	}

	// if, after managing potential inversion, we still get a negative result back... we are inside solid scene geometry
	if ( de( rayOrigin ) < 0.0f ) {
		result.dist = -1.0f;
		result.materialType = NOHIT;
		result.albedo = hitAlbedo;
	} else {
		// we're in a valid location and clear to do a raymarch
		result.dist = 0.0f;
		for ( int i = 0; i < maxSteps; i++ ) {
			float d = de( rayOrigin + result.dist * rayDirection );
			if ( d < epsilon ) {
			// we have a hit - gather intersection information
				result.materialType = hitSurfaceType;
				result.albedo = hitAlbedo;
				result.frontFacing = !invert; // for now, this will be sufficient to make decisions re: IoR
				result.IoR = getIORForMaterial( hitSurfaceType );
				result.normal = SDFNormal( rayOrigin + result.dist * rayDirection );
				result.roughness = hitRoughness;
			} else if ( result.dist > maxDistance ) {
				result.materialType = NOHIT;
				break;
			}
			result.dist += d;
		}
	}

	// and give back whatever we got
	return result;
}

// support for glass behavior
float Reflectance ( const float cosTheta, const float IoR ) {
	/*
	// Use Schlick's approximation for reflectance
	float r0 = ( 1.0f - IoR ) / ( 1.0f + IoR );
	r0 = r0 * r0;
	return r0 + ( 1.0f - r0 ) * pow( ( 1.0f - cosTheta ), 5.0f );
	*/

	// "Full Fresnel", from https://www.shadertoy.com/view/csfSz7
	float g = sqrt( IoR * IoR + cosTheta * cosTheta - 1.0f );
	float a = ( g - cosTheta ) / ( g + cosTheta );
	float b = ( ( g + cosTheta ) * cosTheta - 1.0f ) / ( ( g - cosTheta ) * cosTheta + 1.0f );
	return 0.5f * a * a * ( 1.0f + b * b );
}

// where we are updating on the image
uniform ivec2 tileOffset;

void main () {
	seed = rngSeed + 42069 * gl_GlobalInvocationID.x + gl_GlobalInvocationID.y;

	const ivec2 loc = tileOffset + ivec2( gl_GlobalInvocationID.xy );

	// initial ray origin coming from jittered subpixel location, random direction
	vec2 rayOrigin = 1.125f * ( ( vec2( loc ) + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) ) - imageSize( bufferImage ).xy / 2 );
	vec2 rayDirection = normalize( CircleOffset() ); // consider uniform remappings, might create diffraction spikes?

	// transmission and energy totals
	float transmission = 1.0f;
	float energyTotal = 0.0f;

	// selected wavelength - using full range, we can revisit this later
	wavelength = RangeRemapValue( NormalizedRandomFloat(), 0.0f, 1.0f, 360.0f, 830.0f );

	// pathtracing loop
	const int maxBounces = 15;
	for ( int i = 0; i < maxBounces; i++ ) {
		// trace the ray against the scene...
		intersectionResult result = sceneTrace( rayOrigin, rayDirection );

		// if we did not hit anything, break out of the loop
		if ( result.dist < 0.0f ) {
			energyTotal += transmission * result.albedo;
			break;
		}

		// "chance to consume" russian roulette term... tbd

		// russian roulette termination
		if ( NormalizedRandomFloat() > transmission ) break;
		transmission *= 1.0f / transmission; // compensation term

		// attenuate transmission by the surface albedo
		if ( result.materialType != EMISSIVE ) transmission *= result.albedo;

		// if we hit something emissive, add emission term times the transmission to the accumulated energy total
		if ( result.materialType == EMISSIVE ) energyTotal += transmission * result.albedo;

		// update position + epsilon bump
		rayOrigin = rayOrigin + result.dist * rayDirection + result.normal * epsilon * 3;

		// material evaluation/new value of rayDirection
		switch ( result.materialType ) {
		case EMISSIVE:
		case DIFFUSE:
			rayDirection = normalize( CircleOffset() );
			// invert if going into the surface
			if ( dot( rayDirection, result.normal ) < 0.0f ) {
				rayDirection = -rayDirection;
			}
			break;

		case METALLIC:
			// todo
			break;

		case MIRROR:
			rayDirection = reflect( rayDirection, result.normal );
			break;

		// below this point, we have to consider the IoR for the specific form of glass... because we precomputed all the
			// varying behavior already, we can just treat it uniformly, only need to consider frontface/backface for inversion
		default:
			rayOrigin -= result.normal * epsilon * 5;
			result.IoR = result.frontFacing ? ( 1.0f / result.IoR ) : ( result.IoR );

			float cosTheta = min( dot( -normalize( rayDirection ), result.normal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( result.IoR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, result.IoR ) > NormalizedRandomFloat() ) {
				rayDirection = normalize( mix( reflect( normalize( rayDirection ), result.normal ), CircleOffset(), result.roughness ).xy );
			} else {
				rayDirection = normalize( mix( refract( normalize( rayDirection ), result.normal, result.IoR ), CircleOffset(), result.roughness ).xy );
			}

			break;
		}
	}

	// result is then averaged, in XYZ space, and written back
	vec4 previousValue = imageLoad( bufferImage, loc );
	float sampleCount = previousValue.a + 1.0f;
	const float mixFactor = 1.0f / sampleCount;
	const vec4 mixedColor = vec4( mix( previousValue.xyz, energyTotal * wavelengthColor( wavelength ), mixFactor ), sampleCount );
	imageStore( bufferImage, loc, mixedColor );
}