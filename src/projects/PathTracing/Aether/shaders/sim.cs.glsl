#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

//=============================================================================================================================
// RNG seeding...
#include "random.h"
uniform uint wangSeed;
//=============================================================================================================================
#include "mathUtils.h"
//=============================================================================================================================
// blue noise
uniform ivec2 noiseOffset;
vec4 blueNoiseRef ( ivec2 pos ) {
	pos.x = ( pos.x + noiseOffset.x ) % imageSize( blueNoiseTexture ).x;
	pos.y = ( pos.y + noiseOffset.y ) % imageSize( blueNoiseTexture ).y;
	return imageLoad( blueNoiseTexture, pos ) / 255.0f;
}
//=============================================================================================================================
// the field buffer image/tex
//uniform isampler3D bufferTexX;
//uniform isampler3D bufferTexY;
//uniform isampler3D bufferTexZ;
//uniform isampler3D bufferTexCount;
layout( binding = 2, r32ui ) uniform uimage3D bufferImageX;
layout( binding = 3, r32ui ) uniform uimage3D bufferImageY;
layout( binding = 4, r32ui ) uniform uimage3D bufferImageZ;
layout( binding = 5, r32ui ) uniform uimage3D bufferImageCount;
//=============================================================================================================================
const float epsilon = 0.00001f;
//=============================================================================================================================
float myWavelength = 0.0f;
//=============================================================================================================================
// CDF for the light wavelengths
uniform sampler2D iCDFtex;
float getWavelength ( int lightType ) {
	return texture( iCDFtex, vec2( NormalizedRandomFloat(), ( lightType + 0.5f ) / textureSize( iCDFtex, 0 ).y ) ).r;
}
//=============================================================================================================================
#include "colorspaceConversions.glsl"
#include "intersect.h"
uniform ivec3 dimensions;
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
// Materials stuff, copied straight from path2Dforward/Newton, will need to refine eventually
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

bool isRefractive ( int id ) {
	return id >= CAUCHY_FUSEDSILICA;
}

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
//=============================================================================================================================
struct intersectionResult {
// scene intersection representation etc loosely based on Daedalus
	float dist;
	float albedo;
	float IoR;
	float roughness;
	vec3 normal;
	bool frontFacing;
	int materialType;
};

intersectionResult getDefaultIntersection () {
	intersectionResult result;
	result.dist = 0.0f;
	result.albedo = 0.0f;
	result.IoR = 0.0f;
	result.roughness = 0.0f;
	result.normal = vec3( 0.0f );
	result.frontFacing = false;
	result.materialType = NOHIT;
	return result;
}

// raymarch parameters
const float maxDistance = 6000.0f;
const int maxSteps = 200;

// global state tracking
int hitSurfaceType = 0;
float hitRoughness = 0.0f;
float hitAlbedo = 0.0f;
bool invert = false;

#include "hg_sdf.glsl"

float de ( vec3 p ) {
	float sceneDist = 100000.0f;
	const vec3 pOriginal = p;

	hitAlbedo = 0.0f;
	hitSurfaceType = NOHIT;
	hitRoughness = 0.0f;

	{
		int l = int( pMod1( p.x, 88.0f ) );
		int k = int( pMod1( p.y, 108.0f ) );
		pMod1( p.z, 50.0f );
		const float d = ( invert ? -1.0f : 1.0f ) * ( ( ( k * l % 2 ) == 0 ) ? ( distance( p, vec3( 0.0f ) ) - 30.0f ) : fDodecahedron( p, 30.0f ) );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = SELLMEIER_BOROSILICATE_BK7;
			hitAlbedo = 0.99f;
		}
	}

	// get back final result
	return sceneDist;
}

// function to get the normal
vec3 SDFNormal ( vec3 p ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( p ) ) - vec3( de( p - e.xyy ), de( p - e.yxy ), de( p - e.yyx ) ) );
}

// trace against the scene
intersectionResult sceneTrace ( vec3 rayOrigin, vec3 rayDirection ) {
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

//=============================================================================================================================
void configureLightRay ( out vec3 rO, out vec3 rD, in lightSpecGPU pickedLight ) {
	// let's figure out what wavelength we are
	myWavelength = getWavelength( int( pickedLight.typeVec.y ) );

// generate a new ray, based on the properties of the selected light...
	// what is my starting position, direction?
	rO = vec3( 0.0f ), rD = vec3( 0.0f );
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
}

//=============================================================================================================================
/* scene SDF should be part of a header, so we can use it more than one place - eventually that wrapper can include a BVH, too
	we need it:
		1. here - forward pathtrace, light source emits a ray which interacts with one or more scene objects, to populate 3D fluence field
		2. to manage the scene intersections between bounces during the fluence trace for output pixel
			( if we look at glass, our view ray needs to refract accordingly, or our sampling is wrong... we will need to figure out particulars for this, what we use as a wavelength )
		3. potential future renderers...

		environment will expect ( *before* header's #include statement, because it will be used in the code substituted in by the preprocessor ):
			myWavelength should exist... we'll probably either use a random one or we have to do spectral samples for the output trace ( we can, it's not really a problem )
			bool invert toggle should exist - used for refractive objects, and we're assuming that I'm manually managing overlap of scene objects or else accepting artifacts
			...more?
*/

#include "spectrumXYZ.h"

void drawPixel ( ivec3 p, float AAFactor, vec3 XYZColor ) {
	// do the atomic increments for this sample
	ivec3 increment = ivec3(
		int( 16 * XYZColor.r ),
		int( 16 * XYZColor.g ),
		int( 16 * XYZColor.b )
	);
	// maintaining sum + count by doing atomic writes along the ray
	const ivec3 iS = imageSize( bufferImageY ).xyz - ivec3( 1 );
	if ( p == clamp( p, ivec3( 0 ), iS ) ) {
		imageAtomicAdd( bufferImageX, p, increment.x );
		imageAtomicAdd( bufferImageY, p, increment.y );
		imageAtomicAdd( bufferImageZ, p, increment.z );
		imageAtomicAdd( bufferImageCount, p, int( 256 * AAFactor ) );
	}
}

void drawLine ( vec3 p0, vec3 p1, float energyTotal ) {
	// compute the color once for this line
	const vec3 XYZColor = energyTotal * wavelengthColor( myWavelength );

	// figure out where these two endpoints lie, on the field, draw a line between them
	// use 0-255 AA factor as a scalar on the summand, so that we have soft edged rays
	const ivec3 iS = dimensions;

	const vec4 blue = blueNoiseRef( ivec2( gl_GlobalInvocationID.xy ) );
	const vec3 p0i = vec3( ( p0 + iS ) / 2.0f );
	const vec3 p1i = vec3( ( p1 + iS ) / 2.0f );
	const float stepSize = 0.619f + blue.a;

	const vec3 diff = normalize( p0i - p1i );
	const float l = length( p0i - p1i ) + 1.0f;
	float accum = blue.r;

	for ( int i = 0; i < 5000 && accum < l; i++ ) {
		const vec3 p = p1i + diff * accum;
		accum += stepSize + ( ( mod( i, 3 ) == 0 ) ? blue.b : ( mod( i, 2 ) == 0 ) ? blue.r : blue.g );
		drawPixel( ivec3(
			int( p.x + NormalizedRandomFloat() - 0.5f ),
			int( p.y + NormalizedRandomFloat() - 0.5f ),
			int( p.z + NormalizedRandomFloat() - 0.5f ) ),
		1.0f, XYZColor );
	}
}


void main () {
	// pixel location
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// initial wang state
	seed = wangSeed + loc.x * 69696 + loc.y * 8675309;

	// we need to first pick one of the lights out of the light buffer...
	lightSpecGPU lightSpec = lightList[ lightIStructure[ wangHash() % 1024 ] ];

	// wavelength is kept in global scope so it doesn't need to be passed around
	vec3 rO, rD;
	configureLightRay( rO, rD, lightSpec );

	// environment configuration for the forward pathtracing
		// ( energy starts at a maximum and attenuates, when we start from the light source - are these redunant? )
	float transmission = 1.0f;
	float energyTotal = 1.0f;

	// pathtracing loop
	const int maxBounces = 64;
	float previousIoR = 1.0f;
	for ( int i = 0; i < maxBounces; i++ ) {
		// trace the ray against the scene
		intersectionResult intersection = sceneTrace( rO, rD );

		// draw a line to the scene intersection point
		drawLine( rO, rO + intersection.dist * rD, energyTotal );

		// russian roulette termination
		if ( NormalizedRandomFloat() > energyTotal ) { break; }
		energyTotal *= 1.0f / energyTotal; // rr compensation term

		// attenuate by the surface albedo
		transmission *= intersection.albedo;
		energyTotal *= intersection.albedo;

		rO = rO + ( intersection.dist - epsilon ) * rD;

		// update ray origin + new ray direction logic
		switch ( intersection.materialType ) {
			case NOHIT:
//			i = maxBounces; // we're going to break out of the loop
			break;

			case DIFFUSE:
			rD = RandomUnitVector();
			// invert if going into the surface
			if ( dot( rD, intersection.normal ) < 0.0f ) {
				rD = -rD;
			}
			break;

			case METALLIC:
			// todo
			break;

			case MIRROR:
			rD = reflect( rD, intersection.normal );
			break;

			// below this point, we have to consider the IoR for the specific form of glass... because we precomputed all the
			// varying behavior already, we can just treat it uniformly, only need to consider frontface/backface for inversion
			default:
			rO -= intersection.normal * epsilon * 5;
			intersection.IoR = intersection.frontFacing ? ( 1.0f / intersection.IoR ) : ( intersection.IoR ); // "reverse" back to physical properties for IoR
			//			float IoRCache = result.IoR;
			//			result.IoR = result.IoR / previousIoR;

			float cosTheta = min( dot( -normalize( rD ), intersection.normal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( intersection.IoR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, intersection.IoR ) > NormalizedRandomFloat() ) {
				rD = normalize( mix( reflect( normalize( rD ), intersection.normal ), RandomUnitVector(), intersection.roughness ) );
			} else {
				rD = normalize( mix( refract( normalize( rD ), intersection.normal, intersection.IoR ), RandomUnitVector(), intersection.roughness ) );
			}

			//			previousIoR = IoRCache;
			break;
		}

	}

}
