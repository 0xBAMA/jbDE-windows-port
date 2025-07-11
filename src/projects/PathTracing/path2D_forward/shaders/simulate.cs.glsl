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

uniform float t;
uniform int rngSeed;

// I would like to convert to ray-primitive intersection, instead of raymarching
float rayPlaneIntersect ( in vec3 rayOrigin, in vec3 rayDirection ) {
	const vec3 normal = vec3( 0.0f, 1.0f, 0.0f );
	const vec3 planePt = vec3( 0.0f, 0.0f, 0.0f ); // not sure how far down this should be
	return -( dot( rayOrigin - planePt, normal ) ) / dot( rayDirection, normal );
}

#include "random.h"
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
const float epsilon = 0.0001f;
const float maxDistance = 2000.0f;
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

// should we invert the lenses?
bool invert = false;

float rectangle ( vec2 samplePosition, vec2 halfSize ) {
	vec2 componentWiseEdgeDistance = abs( samplePosition ) - halfSize;
	float outsideDistance = length( max( componentWiseEdgeDistance, 0 ) );
	float insideDistance = min( max( componentWiseEdgeDistance.x, componentWiseEdgeDistance.y ), 0 );
	return outsideDistance + insideDistance;
}

float de ( vec2 p ) {
	float sceneDist = 100000.0f;
	const vec2 pOriginal = p;

	hitAlbedo = 0.0f;
	hitSurfaceType = NOHIT;
	hitRoughness = 0.0f;

	if ( false ) { // an example object (refractive)
		pModPolar( p.xy, 13.0f );
		// const float d = ( invert ? -1.0f : 1.0f ) * ( max( distance( p, vec2( 90.0f, 0.0f ) ) - 100.0f, distance( p, vec2( 110.0f, 0.0f ) ) - 150.0f ) );
		const float d = ( invert ? -1.0f : 1.0f ) * ( distance( p, vec2( 250.0f, 0.0f ) ) - 35.0f );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = SELLMEIER_FUSEDSILICA;
			hitAlbedo = 0.99f;
		}
	}

	p = Rotate2D( 0.3f ) * pOriginal;
	vec2 gridIndex;
	gridIndex.x = pModInterval1( p.x, 100.0f, -100.0f, 100.0f );
	gridIndex.y = pModInterval1( p.y, 100.0f, -6.0f, 6.0f );

	{ // an example object (refractive)
		// bool checker = checkerBoard( 1.0f, vec3( gridIndex / 3.0f + vec2( 0.1f ), 0.5f ) );
		bool checker = false;
		const float d = ( invert ? -1.0f : 1.0f ) * ( checker ? ( rectangle( p, vec2( checker ? 20.0f : 32.0f ) ) ) : ( distance( p, vec2( 0.0f ) ) - ( checker ? 20.0f : 40.0f ) ) );
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

	// walls at the edges of the screen for the rays to bounce off of
	{
		const float d = min( min( min(
			rectangle( pOriginal - vec2( 0.0f, -1900.0f ), vec2( 4000.0f, 20.0f ) ),
			rectangle( pOriginal - vec2( 0.0f, 1700.0f ), vec2( 4000.0f, 20.0f ) ) ),
			rectangle( pOriginal - vec2( -2300.0f, 0.0f ), vec2( 20.0f, 3000.0f ) ) ),
			rectangle( pOriginal - vec2( 2300.0f, 0.0f ), vec2( 20.0f, 3000.0f ) ) );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = MIRROR;
			hitAlbedo = 0.99f;
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
}

void drawPixel ( int x, int y, float AAFactor, vec3 XYZColor ) {
	// do the atomic increments for this sample
	ivec3 increment = ivec3(
		int( 1024 * XYZColor.r ),
		int( 1024 * XYZColor.g ),
		int( 1024 * XYZColor.b )
	);
	// maintaining sum + count by doing atomic writes along the ray
	ivec2 p = ivec2( x, y );
	imageAtomicAdd( bufferImageX, p, increment.x );
	imageAtomicAdd( bufferImageY, p, increment.y );
	imageAtomicAdd( bufferImageZ, p, increment.z );
	imageAtomicAdd( bufferImageCount, p, int( 256 * AAFactor ) );
}

void drawLine ( vec2 p0, vec2 p1, float energyTotal, float wavelength ) {
	// compute the color once for this line
	vec3 XYZColor = energyTotal * wavelengthColor( wavelength ) / 10.0f;

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

	for ( int i = 0; i < 3000 && accum < l; i++ ) {
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

void main () {
	seed = rngSeed + 42069 * gl_GlobalInvocationID.x + 6969 * gl_GlobalInvocationID.y + 619 * gl_GlobalInvocationID.z;
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// need to pick a light source, point on the light source, plus emission spectra, plus direction

	// hacky, but I want something to compare against the backwards impl at least temporarily
	vec2 rayOrigin, rayDirection; // emission spectra will not match, oh well, I can run it again with some tweaks
	/*
	switch ( clamp( int( NormalizedRandomFloat() * 2.99f ), 0, 2 ) ) {
		case 0: rayOrigin = vec2( -200.0f, -200.0f ) + 20.0f * CircleOffset(); rayDirection = normalize( CircleOffset() ); break;
		case 1: rayOrigin = vec2( -400.0f, 0.0f ) + 20.0f * CircleOffset(); rayDirection = normalize( CircleOffset() ); break;
		case 2: rayOrigin = vec2( -200.0f, 200.0f ) + 20.0f * CircleOffset(); rayDirection = normalize( CircleOffset() ); break;
		default: break;
	}
	*/

	// rayOrigin = vec2( -400.0f, 100.0f + 30.0f * ( NormalizedRandomFloat() - 0.5f ) );
	// rayDirection = normalize( vec2( 1.0f, 0.01f * ( NormalizedRandomFloat() - 0.5f ) ) );
//	 if ( NormalizedRandomFloat() < 0.3f ) { // beams
		  rayDirection = vec2( 0.0f, 1.0f );
		  if ( NormalizedRandomFloat() < 0.5f ) {
			  rayOrigin = vec2( -2000.0f + t + 50.0f * ( NormalizedRandomFloat() - 0.5f ), -1600.0f );
		  } else {
			  rayOrigin = vec2( -200.0f + t + 50.0f * ( NormalizedRandomFloat() - 0.5f ), -1600.0f );
		  }
	// } else if ( NormalizedRandomFloat() < 0.4f ) { // ambient omnidirectional point
//		 rayOrigin = vec2( 20.0f * ( NormalizedRandomFloat() - 0.5f ), -1800.0f );
//		 rayDirection = normalize( CircleOffset() * vec2( 1.0f, 3.0f ) );
//		 if (  rayDirection.y < 0.0f ) {
//			 rayDirection.y *= -1.0f;
//		 }
//	 } else { // overhead light
		// rayOrigin = vec2( 10.0f * ( NormalizedRandomFloat() - 0.5f ) + int( ( NormalizedRandomFloat() - 0.5f ) * 69.0f ) * 30.0f, -1800.0f );
//		 rayOrigin = vec2( 1400.0f * ( NormalizedRandomFloat() - 0.5f ), -1600.0f );
//		 rayDirection = vec2( 0.0f, 1.0f );
//	}

	// transmission and energy totals... energy starts at a maximum and attenuates, when we start from the light source
	float transmission = 1.0f;
	float energyTotal = 1.0f;

	// selected wavelength - using full range, we can revisit this later
	wavelength = RangeRemapValue( NormalizedRandomFloat(), 0.0f, 1.0f, 360.0f, 830.0f );

	// pathtracing loop
	const int maxBounces = 50;
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

		/*
		// russian roulette termination
		if ( NormalizedRandomFloat() > transmission ) break;
		transmission *= 1.0f / transmission; // compensation term
		*/

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
}