#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform ivec2 noiseOffset;
uniform sampler2D blueNoise;
vec4 blueNoiseRead ( ivec2 loc ) {
	ivec2 wrappedLoc = ( loc + noiseOffset ) % textureSize( blueNoise, 0 );
	return texelFetch( blueNoise, wrappedLoc, 0 );
}

layout( r32i ) uniform iimage2D bufferImageX;
layout( r32i ) uniform iimage2D bufferImageY;
layout( r32i ) uniform iimage2D bufferImageZ;
layout( r32i ) uniform iimage2D bufferImageCount;
uniform sampler2D iCDFtex;

uniform float t;
uniform int rngSeed;

// I would like to convert to ray-primitive intersection, instead of raymarching
float rayPlaneIntersect ( in vec3 rayOrigin, in vec3 rayDirection ) {
	const vec3 normal = vec3( 0.0f, 1.0f, 0.0f );
	const vec3 planePt = vec3( 0.0f, 0.0f, 0.0f ); // not sure how far down this should be
	return -( dot( rayOrigin - planePt, normal ) ) / dot( rayDirection, normal );
}

#include "random.h"
#include "noise.h"
#include "hg_sdf.glsl"
#include "mathUtils.h"
#include "spectrumXYZ.h"
#include "colorspaceConversions.glsl"

// drawing inspiration from https://www.shadertoy.com/view/M3jcDW
	// they use a "presence" term, which projects one color onto another to judge similarity... their usage is as a
	// russian roulette term, but I think it would work as a simple way to implement float-valued albedo, in terms of wavelength

float presence ( vec3 a, vec3 b ) { // call presence( a, b ) to see if
	return clamp( pow( dot( a, b ) / dot( a, vec3( 1.0f ) ), 3.0f ), 0.0f, 1.0f );
}

// trace against the scene
	// this abstraction makes it easier to target raymarch/other scene intersection methods when I want to swap it out
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
const float epsilon = 0.1f;
const float maxDistance = 6000.0f;
const int maxSteps = 200;

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

bool isRefractive ( int id ) {
	return id >= CAUCHY_FUSEDSILICA;
}

// should we invert the lenses?
bool invert = false;

float rectangle ( vec2 samplePosition, vec2 halfSize ) {
	vec2 componentWiseEdgeDistance = abs( samplePosition ) - halfSize;
	float outsideDistance = length( max( componentWiseEdgeDistance, 0 ) );
	float insideDistance = min( max( componentWiseEdgeDistance.x, componentWiseEdgeDistance.y ), 0 );
	return outsideDistance + insideDistance;
}

// from belmu - https://www.shadertoy.com/view/WfSXRV
struct LensElement {
	float radius;    // Radius of the circle primitive (if negative, element is concave)
	float thickness; // Distance to next element
	float aperture;  // Aperture diameter of the element
	bool  isAir;
};

const LensElement doubleGauss[] = LensElement[](
LensElement( 085.50, 11.6, 76.0, false),
LensElement( 408.33, 01.5, 76.0, true ),
LensElement( 040.35, 17.0, 66.0, false),
LensElement( 156.05, 03.5, 66.0, false),
LensElement( 025.05, 13.7, 44.0, true ),
LensElement( 000.00, 08.3, 42.6, true ),
LensElement(-036.80, 03.5, 44.0, false),
LensElement( 055.00, 23.0, 52.0, false),
LensElement(-051.50, 01.0, 52.0, true ),
LensElement( 123.50, 17.0, 52.0, false),
LensElement(-204.96, 00.0, 52.0, true )
);

const LensElement cookeTriplet[] = LensElement[](
LensElement( 032.25, 06.0, 50.0, false),
LensElement( 188.25, 08.1, 50.0, true ),
LensElement(-144.50, 01.0, 40.0, false),
LensElement( 028.72, 17.9, 40.0, true ),
LensElement( 139.60, 02.5, 30.0, false),
LensElement(-088.00, 00.0, 30.0, true )
);

const LensElement tessar[] = LensElement[](
LensElement( 042.970, 09.800, 38.4, false),
LensElement(-115.330, 02.100, 38.4, false),
LensElement( 306.840, 04.160, 38.4, true ),
LensElement( 000.000, 04.000, 30.0, true ),
LensElement(-059.060, 01.870, 34.6, false),
LensElement( 040.930, 10.640, 34.6, true ),
LensElement( 183.920, 07.050, 33.0, false),
LensElement(-048.910, 79.831, 33.0, true )
);

const LensElement fisheye[] = LensElement[](
LensElement( 302.249 * 0.3, 008.335 * 0.3, 303.4 * 0.3, false),
LensElement( 113.931 * 0.3, 074.136 * 0.3, 206.8 * 0.3, true ),
LensElement( 752.019 * 0.3, 010.654 * 0.3, 178.0 * 0.3, false),
LensElement( 083.349 * 0.3, 111.549 * 0.3, 134.2 * 0.3, true ),
LensElement( 095.882 * 0.3, 020.054 * 0.3, 090.2 * 0.3, false),
LensElement( 438.677 * 0.3, 053.895 * 0.3, 081.4 * 0.3, true ),
LensElement( 000.000 * 0.3, 014.163 * 0.3, 060.8 * 0.3, true ),
LensElement( 294.541 * 0.3, 021.934 * 0.3, 059.6 * 0.3, false),
LensElement(-052.265 * 0.3, 009.714 * 0.3, 058.4 * 0.3, false),
LensElement(-142.884 * 0.3, 000.627 * 0.3, 059.6 * 0.3, true ),
LensElement(-223.726 * 0.3, 009.400 * 0.3, 059.6 * 0.3, false),
LensElement(-150.404 * 0.3, 000.000 * 0.3, 065.2 * 0.3, true )
);

const LensElement petzval[] = LensElement[](
LensElement( 055.9, 05.2, 16.0, false),
LensElement(-043.7, 00.8, 16.0, false),
LensElement( 460.4, 33.6, 16.0, true ),
LensElement( 110.6, 01.5, 16.0, false),
LensElement( 038.9, 03.3, 16.0, true ),
LensElement( 048.0, 03.6, 16.0, false),
LensElement(-157.8, 30.0, 16.0, true )
);


// CHOOSE LENS SYSTEM HERE

const LensElement lensSystem[] = fisheye;

// ^^^^^^^^^^^^^^^^^^^^^^^

vec3 randomColor(float seed) {
	seed += 65.0;

	float x = fract(sin(seed * 12.9898) * 43758.5453);
	float y = fract(sin(seed * 78.233)  * 127.1);
	float z = fract(sin(seed * 39.3467) * 311.7);

	return vec3(x, y, z);
}

float sdCircle(vec2 position, vec2 center, float radius) {
	return distance(position, center) - radius;
}

float sdBox(vec2 position, vec2 center, vec2 size) {
	vec2 d = abs(position - center) - size;
	return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float signNonZero(float x) {
	return x > 0.0 ? 1.0 : -1.0;
}

// The sagitta of a circular arc is the distance from the midpoint of the arc to the midpoint of its chord
float sagitta(float radius, float aperture) {
	if (radius == 0.0) return 0.0;
	float r = abs(radius);
	float h = aperture * 0.5;
	return r - sqrt(r * r - h * h);
}

// Lens element SDF, we subtract the front surface to a box, then intersect the result with the rear surface
float sdLensElement(LensElement lens1, LensElement lens2, vec2 uv, float opticalAxisX) {
	float center1 = opticalAxisX + lens1.radius;
	float center2 = opticalAxisX + lens1.thickness + lens2.radius;

	float sag1 = sagitta(lens1.radius, lens1.aperture);
	float sag2 = sagitta(lens2.radius, lens2.aperture);

	float totalWidth = sag1 + lens1.thickness + sag2;

	vec2  stockSize = vec2(totalWidth, max(lens1.aperture, lens2.aperture));
	float stock     = sdBox(uv, vec2(opticalAxisX + lens1.thickness * 0.5, 0.0), stockSize * 0.5);

	float interface1 = sdCircle(uv, vec2(center1, 0.0), abs(lens1.radius)) *  signNonZero(lens1.radius);
	float interface2 = sdCircle(uv, vec2(center2, 0.0), abs(lens2.radius)) * -signNonZero(lens2.radius);

	return max(max(interface1, interface2), stock);
}

int lensSystemResult = 0;

float deLensSystem ( vec2 uv ) {
	lensSystemResult = NOHIT;
	float opticalAxisX = 0.0;
	float minDist = 100000.0f;
	for (int i = 0; i < lensSystem.length() - 1; i++) {
		LensElement lens1 = lensSystem[i];
		LensElement lens2 = lensSystem[i + 1];

		float element = sdLensElement(lens1, lens2, uv, opticalAxisX);
		float minDistCache = minDist;
		minDist = min( minDist, ( invert ? -1.0f : 1.0f ) * element );

		if ( lens1.isAir ) {
			minDist = min( minDistCache, abs( element ) );
		} else {
			minDist = min( minDist, ( invert ? -1.0f : 1.0f ) * element );
			lensSystemResult = SELLMEIER_BOROSILICATE_BK7;
			// break;
		}

		/*
		if ( element < 0.0 ) {
			if ( lens1.isAir ) {
				// fragColor.rgb = vec3(0.0, 0.0, 0.0);
			} else {
				// fragColor.rgb = randomColor(float(i));

				break;
			}
		}
		*/

		opticalAxisX += lens1.thickness;
	}

	return minDist;
}

float sdParabola( in vec2 pos, in float wi, in float he ) {
	// "width" and "height" of a parabola segment
	pos.x = abs(pos.x);

	float ik = wi*wi/he;
	float p = ik*(he-pos.y-0.5*ik)/3.0;
	float q = pos.x*ik*ik*0.25;
	float h = q*q - p*p*p;

	float x;
	if( h>0.0 ) // 1 root
	{
		float r = sqrt(h);
		x = pow(q+r,1.0/3.0) + pow(abs(q-r),1.0/3.0)*sign(p);
	}
	else        // 3 roots
	{
		float r = sqrt(p);
		x = 2.0*r*cos(acos(q/(p*r))/3.0); // see https://www.shadertoy.com/view/WltSD7 for an implementation of cos(acos(x)/3) without trigonometrics
	}

	x = min(x,wi);

	return length(pos-vec2(x,he-x*x/ik)) *
	sign(ik*(pos.y-he)+pos.x*pos.x);
}

float de ( vec2 p ) {
	float sceneDist = 100000.0f;
	const vec2 pOriginal = p;

	hitAlbedo = 0.0f;
	hitSurfaceType = NOHIT;
	hitRoughness = 0.0f;

	if ( false ) {
		float opticalAxisOffset = 0.0f;
		const float scale = 5.0f;
		for ( int i = 0; i < lensSystem.length(); i++ ) {
			// const float d = deLensSystem( p / scale ) * scale;
			const float d = ( ( invert && !lensSystem[ i + 1 ].isAir ) ? -1.0f : 1.0f ) * sdLensElement( lensSystem[ i ], lensSystem[ i + 1 ], p / scale, opticalAxisOffset ) * scale;
			sceneDist = min( sceneDist, d );
			if ( sceneDist == d && d < epsilon ) {
				hitSurfaceType = lensSystem[ i ].isAir ? AIR : SELLMEIER_BOROSILICATE_BK7;
				hitAlbedo = 1.0f;
			}
			opticalAxisOffset += lensSystem[ i ].thickness;
		}
	}

	/*
	{
		const float d = abs( sdParabola( p - vec2( 0.0f, 400.0f ), 300.0f, 200.0f ) ) - 15.0f;
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = MIRROR;
			hitAlbedo = 0.1f;
		}
	}
	{
		const float d = abs( sdParabola( vec2( 1.0f, -1.0f ) * p - vec2( 0.0f, 400.0f ), 500.0f, 100.0f ) ) - 15.0f;
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = MIRROR;
			hitAlbedo = 1.0f;
		}
	}

	if ( false ) { // an example object (refractive)
		pModPolar( p.xy, 17.0f );
		// const float d = ( invert ? -1.0f : 1.0f ) * ( max( distance( p, vec2( 90.0f, 0.0f ) ) - 100.0f, distance( p, vec2( 110.0f, 0.0f ) ) - 150.0f ) );
		const float d = ( invert ? -1.0f : 1.0f ) * ( distance( p, vec2( 300.0f, 0.0f ) ) - 40.0f );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = SELLMEIER_BOROSILICATE_BK7;
			hitAlbedo = 0.99f;
		}
	}
	*/

	if ( true ) {
		p = Rotate2D( 0.3f ) * pOriginal;
		vec2 gridIndex;
		gridIndex.x = pModInterval1( p.x, 100.0f, -100.0f, 100.0f );
		gridIndex.y = pModInterval1( p.y, 100.0f, -6.0f, 16.0f );
		{ // an example object (refractive)
			uint seedCache = seed;
			seed = 31415 * uint( gridIndex.x ) + uint( gridIndex.y ) * 42069 + 999999;
			const vec3 noise = 0.5f * hash33( vec3( gridIndex.xy, 0.0f ) ) + vec3( 2.0f );
//			 const float d = ( invert ? -1.0f : 1.0f ) * ( ( noise.z > 2.25f ) ? ( rectangle( Rotate2D( noise.z * tau ) * p, vec2( 5.0f * noise.y, 15.0f * noise.z ) ) ) : ( ( distance( p, vec2( 0.0f ) ) - ( 14.0f * noise.y ) ) ) );
			const float d = ( invert ? -1.0f : 1.0f ) * ( ( noise.z > 0.25f ) ? ( distance( p, vec2( 0.0f ) ) - 20.0f * noise.z ) : ( ( distance( p, vec2( 0.0f ) ) - ( 24.0f * noise.y ) ) ) );
//			const float d = ( invert ? -1.0f : 1.0f ) * ( distance( p, vec2( 0.0f ) ) - 15.0f * noise.z );
			seed = seedCache;
			sceneDist = min( sceneDist, d );
			if ( sceneDist == d && d < epsilon ) {
				 hitSurfaceType = SELLMEIER_BOROSILICATE_BK7;
				 hitAlbedo = 1.0f;
				// * RangeRemapValue( wavelength, 300, 900, RangeRemapValue( noise.y, 0.0f, 1.0f, 0.5f, 1.0f ), RangeRemapValue( noise.x, 0.0f, 1.0f, 0.85f, 1.0f ) );
			}
		}
	}

	// walls at the edges of the screen for the rays to bounce off of
	if ( true ) {
		const float d = min( min( min(
			rectangle( pOriginal - vec2( 0.0f, -768.0f ), vec2( 4000.0f, 20.0f ) ),
			rectangle( pOriginal - vec2( 0.0f, 768.0f ), vec2( 4000.0f, 20.0f ) ) ),
			rectangle( pOriginal - vec2( -1280.0f, 0.0f ), vec2( 20.0f, 3000.0f ) ) ),
			rectangle( pOriginal - vec2( 1280.0f, 0.0f ), vec2( 20.0f, 3000.0f ) ) );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = MIRROR;
			hitAlbedo = 0.3f;
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

void drawPixel ( int x, int y, float AAFactor, vec3 XYZColor ) {
	// do the atomic increments for this sample
	ivec3 increment = ivec3(
		int( 16 * XYZColor.r ),
		int( 16 * XYZColor.g ),
		int( 16 * XYZColor.b )
	);
	// maintaining sum + count by doing atomic writes along the ray
	ivec2 p = ivec2( x, y );
	const ivec2 iS = imageSize( bufferImageY ).xy - ivec2( 1 );
	if ( p == clamp( p, ivec2( 0 ), iS ) ) {
		imageAtomicAdd( bufferImageX, p, increment.x );
		imageAtomicAdd( bufferImageY, p, increment.y );
		imageAtomicAdd( bufferImageZ, p, increment.z );
		imageAtomicAdd( bufferImageCount, p, int( 256 * AAFactor ) );
	}
}

void drawLine ( vec2 p0, vec2 p1, float energyTotal, float wavelength ) {
	// compute the color once for this line
	vec3 XYZColor = energyTotal * wavelengthColor( wavelength );

	// figure out where these two endpoints lie, on the field, draw a line between them
		// use 0-255 AA factor as a scalar on the summand, so that we have soft edged rays
	const ivec2 iS = imageSize( bufferImageX ).xy;

	// vec2 p0i = vec2( p0 / 2.0f + iS / 2 + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) );
	// vec2 p1i = vec2( p1 / 2.0f + iS / 2 + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) );

	vec4 blue = blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ) ) / 255.0f;
	vec2 p0i = vec2( p0 / 2.0f + iS / 2 );
	vec2 p1i = vec2( p1 / 2.0f + iS / 2 );
	// float stepSize = clamp( length( p0i - p1i ) / 2000.0f + blue.a, 0.619f, 1.4f );
	float stepSize = 0.619f + blue.a;

	vec2 diff = normalize( p0i - p1i );
	float l = length( p0i - p1i );
	float accum = blue.r;

	for ( int i = 0; i < 5000 && accum < l; i++ ) {
		vec2 p = p1i + diff * accum;
		accum += stepSize + ( ( mod( i, 3 ) == 0 ) ? blue.b : ( mod( i, 2 ) == 0 ) ? blue.r : blue.g );
		drawPixel( int( p.x + NormalizedRandomFloat() - 0.5f ), int( p.y + NormalizedRandomFloat() - 0.5f ), 1.0f, XYZColor );
	}

	/*
	int x0, y0, x1, y1;
	x0 = int( p0.x / 2.0f + iS.x / 2 + NormalizedRandomFloat() );
	y0 = int( p0.y / 2.0f + iS.y / 2 + NormalizedRandomFloat() );
	x1 = int( p1.x / 2.0f + iS.x / 2 + NormalizedRandomFloat() );
	y1 = int( p1.y / 2.0f + iS.y / 2 + NormalizedRandomFloat() );

	int dx = abs( x1 - x0 ), sx = x0 < x1 ? 1 : -1;
	int dy = abs( y1 - y0 ), sy = y0 < y1 ? 1 : -1;
	int err = dx - dy, e2, x2;
	int ed = int( dx + dy == 0 ? 1 : sqrt( float( dx * dx ) + float( dy * dy ) ) );

	int maxIterations = 2000;
	while ( maxIterations-- != 0 && ( x0 > 0 && x0 < iS.x && y0 > 0 && y0 < iS.y ) ) {
		drawPixel( x0, y0, ( 1.0f - ( 255 * abs( err - dx + dy ) / ed ) / 255.0f ), XYZColor );
		e2 = err; x2 = x0;
		if ( 2 * e2 >= -dx ) {
			if ( x0 == x1 ) break;
			if ( e2 + dy < ed )
			drawPixel( x0, y0 + sy, ( 1.0f - ( 255 * ( e2 + dy ) / ed ) / 255.0f ), XYZColor );
			err -= dy; x0 += sx;
		}
		if ( 2 * e2 <= dy ) {
			if ( y0 == y1 ) break;
			if ( dx - e2 < ed )
			drawPixel( x2 + sx,y0, ( 1.0f - ( 255 * ( dx - e2 ) / ed ) / 255.0f ), XYZColor );
			err += dx; y0 += sy;
		}
	}
	*/
}

// do something with this at some point
float BlackBody ( float t, float w_nm ) {
	float h = 6.6e-34; // Planck constant
	float k = 1.4e-23; // Boltzmann constant
	float c = 3e8;// Speed of light

	float w = w_nm / 1e9;

	// Planck's law https://en.wikipedia.org/wiki/Planck%27s_law

	float w5 = w*w*w*w*w;
	float o = 2.*h*(c*c) / (w5 * (exp(h*c/(w*k*t)) - 1.0));

	return o;
}

uniform vec2 mousePos;
uniform int pickedLight;

void main () {
	seed = rngSeed + 42069 * gl_GlobalInvocationID.x + 6969 * gl_GlobalInvocationID.y + 619 * gl_GlobalInvocationID.z;
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// need to pick a light source, point on the light source, plus emission spectra, plus direction

	vec2 rayOrigin, rayDirection; // emission spectra will not match, oh well, I can run it again with some tweaks

	// we have 13 entries in the LUT texture
	const int numLights = textureSize( iCDFtex, 0 ).y;
	// const int pickedLight = int( NormalizedRandomFloat() * 1000 ) % numLights;

//	rayOrigin = vec2( 100.0f * ( NormalizedRandomFloat() - 0.5f ), -1600.0f );
	// rayOrigin = vec2( -2000.0f + pickedLight * 200.0f, 0.0f ) + Rotate2D( pickedLight * 0.3f ) * vec2( 100.0f * ( NormalizedRandomFloat() - 0.5f ), 0.0f );
	// rayOrigin = vec2( -400 + 5.0f * ( NormalizedRandomFloat() - 0.5f ), -330.0f );
	rayOrigin.x = RangeRemapValue( mousePos.x, 0.0f, 1.0f, -1280.0f, 1280.0f);
	rayOrigin.y = RangeRemapValue( mousePos.y, 0.0f, 1.0f, -768.0f, 768.0f);

	mat2 rotation = Rotate2D( 0.5f );

//	rayOrigin = mousePos + rotation * vec2( 0.0f, -100.0f + 200.0f * NormalizedRandomFloat() );
//	rayOrigin = mix( vec2( -1200.0f, -1600.0f ), vec2( 1200.0f, -1600.0f ), int( count * NormalizedRandomFloat() ) / float( count ) );
	 rayDirection = normalize( vec2( 1.0f, 0.1f + 0.01f * rnd_disc_cauchy() ) );
//	rayDirection = normalize( vec2( 1.0f, 0.0f ) );

//	rayOrigin = vec2( -2000.0f + 300.0f * pickedLight, 0.0f ) + clamp( 0.1f * rnd_disc_cauchy(), vec2( -10.0f ), vec2( 10.0f ) );
//	rayDirection = normalize( CircleOffset() );

	// transmission and energy totals... energy starts at a maximum and attenuates, when we start from the light source
	float transmission = 1.0f;
	float energyTotal = 1.0f;

	// selected wavelength - y picks which light it is
	wavelength = texture( iCDFtex, vec2( NormalizedRandomFloat(), ( pickedLight + 0.5f ) / textureSize( iCDFtex, 0 ).y ) ).r;
//	wavelength = texture( iCDFtex, vec2( NormalizedRandomFloat(), 2.5f / textureSize( iCDFtex, 0 ).y ) ).r;

	// pathtracing loop
	const int maxBounces = 128;
	float previousIoR = 1.0f;
	for ( int i = 0; i < maxBounces; i++ ) {
		// trace the ray against the scene...
		intersectionResult result = sceneTrace( rayOrigin, rayDirection );

	// instead of averaging like before... we need to keep tally sums for the three channels, plus a count
		// additionally, this has to happen between each bounce... basically the preceeding ray will be
		// drawn as part of the material evaluation for the point where it intersects the next surface
		drawLine( rayOrigin, rayOrigin + rayDirection * result.dist + result.normal * epsilon, energyTotal, wavelength );

		// if we did not hit anything, break out of the loop (after drawing the escaping ray)
		/*
		if ( result.dist < 0.0f ) {
			break;
		}
		*/

//		 russian roulette termination
		if ( NormalizedRandomFloat() > energyTotal ) break;
		energyTotal *= 1.0f / energyTotal; // compensation term

		// attenuate transmission by the surface albedo
		energyTotal *= result.albedo;
		transmission *= result.albedo;

		// update position + epsilon bump
		rayOrigin = rayOrigin + result.dist * rayDirection + result.normal * epsilon * 3;

		// material evaluation/new value of rayDirection
		switch ( result.materialType ) {
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
			result.IoR = result.frontFacing ? ( 1.0f / result.IoR ) : ( result.IoR ); // "reverse" back to physical properties for IoR
//			float IoRCache = result.IoR;
//			result.IoR = result.IoR / previousIoR;

			float cosTheta = min( dot( -normalize( rayDirection ), result.normal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( result.IoR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, result.IoR ) > NormalizedRandomFloat() ) {
				rayDirection = normalize( mix( reflect( normalize( rayDirection ), result.normal ), CircleOffset(), result.roughness ).xy );
			} else {
				rayDirection = normalize( mix( refract( normalize( rayDirection ), result.normal, result.IoR ), CircleOffset(), result.roughness ).xy );
			}

//			previousIoR = IoRCache;
			break;
		}
	}
}