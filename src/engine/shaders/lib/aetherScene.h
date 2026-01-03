// need to make sure that hg_sdf.glsl is included before this, because we don't have chained #includes

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
#include "mathUtils.h"

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

float getIORForMaterial ( int material, float wavelength ) {
	// There are a couple ways to get IoR from wavelength
	float wavelengthMicrons = wavelength / 1000.0f;
	// float wavelengthMicrons = wavelength / 100.0f;
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
const float maxDistance = 4000.0f;
const int maxSteps = 100;
const float epsilon = 0.01f;

// we will encode time jitter here
uniform float frame;

// global state tracking
int hitSurfaceType = 0;
float hitRoughness = 0.0f;
float hitAlbedo = 0.0f;
bool invert = false;

float de ( vec3 p, float wavelength ) {
	float sceneDist = 100000.0f;
	const vec3 pOriginal = p;

	hitAlbedo = 0.0f;
	hitSurfaceType = NOHIT;
	hitRoughness = 0.0f;

	float dBounds = fBox( p, vec3( imageSize( bufferImageX ) ) / 2.0f );

	{
		// const float scale = 0.01f;
		// pR( p.xy, 1.0f );
		int l = int( pModInterval1( p.x, 400.0f, -3.0f, 3.0f ) );
		// int l = 0;
		int k = int( pModInterval1( p.y, 400.0f, -1.0f, 1.0f ) );
		int m = int( pModInterval1( p.z, 400.0f, -1.0f, 1.0f ) );

		vec3 rngValue = vec3( pcg3d( uvec3( ivec3( l, m, k ) + 10000 ) ) ) / 4294967296.0f;;
		pR( p.xy, 1.4f + rngValue.z * 1.4f * rngValue.y * 33.0f + rngValue.x * 4.0f + 0.1f * frame );
		pR( p.yz, -10.1f * rngValue.x * rngValue.y * rngValue.x + 0.001f * rngValue.z * frame );

		// const float d = ( invert ? -1.0f : 1.0f ) * ( k % 2 == 0 ? ( distance( p, vec3( 0.0f ) ) - ( 8.0f ) ) : () );
		// const float d = ( invert ? -1.0f : 1.0f ) * ( fDodecahedron( p, 3.0f + 0.5f * ( sin( l ) * cos( k ) * sin( m ) + sin( l * k ) * sin( l * m ) * cos( m * k ) ) ) );
		// const float d = ( invert ? -1.0f : 1.0f ) * ( ( sin( l * m * k + l * m + l * k + m * k ) < 0.0f ) ? fBox( p, 5.1f * vec3( 5.0f, 7.0f, 3.0f ) ) : ( distance( p, vec3( 0.0f ) ) - 45.0f ) );
		const float d = ( invert ? -1.0f : 1.0f ) * ( rngValue.x < 0.5f ? ( fDodecahedron( p, 120.0f ) ) : ( ( fDisc( p, 120.6f ) - 30.0f ) ) );
		// const float d = ( invert ? -1.0f : 1.0f ) * ( fDisc( p, 69.0f ) - 12.0f );
		// const float d = ( invert ? -1.0f : 1.0f ) * ( fBox( p, vec3( 6.0f, 2.0f, 1.618f ) ) );
		// const float d = ( invert ? -1.0f : 1.0f ) * fDodecahedron( p, 7.0f );
		// const float d = ( invert ? -1.0f : 1.0f ) * ( distance( p, vec3( 0.0f ) ) - 10.4f );

		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = rngValue.y < 0.5f ? SELLMEIER_FUSEDSILICA : SELLMEIER_BOROSILICATE_BK7;
			// hitSurfaceType = MIRROR;
			// hitRoughness = 0.1f;
			// hitAlbedo = 0.999f;
			hitAlbedo = 1.0f;
			// hitAlbedo = RangeRemapValue( wavelength, 380.0f, 830.0f, 1.0f, 0.9f );
		}
	}

	// get back final result
	return sceneDist;
}

// function to get the normal
vec3 SDFNormal ( vec3 p, float wavelength ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( p, wavelength ) ) - vec3( de( p - e.xyy, wavelength ), de( p - e.yxy, wavelength ), de( p - e.yyx, wavelength ) ) );
}

// trace against the scene
intersectionResult sceneTrace ( vec3 rayOrigin, vec3 rayDirection, float wavelength ) {
	intersectionResult result = getDefaultIntersection();

	// is the initial sample point inside? -> toggle invert so we correctly handle refractive objects
	if ( de( rayOrigin, wavelength ) < 0.0f ) { // this is probably a solution for the same problem in Daedalus, too...
		invert = !invert;
	}

	// if, after managing potential inversion, we still get a negative result back... we are inside solid scene geometry
	if ( de( rayOrigin, wavelength ) < 0.0f ) {
		result.dist = -1.0f;
		result.materialType = NOHIT;
		result.albedo = hitAlbedo;
	} else {
		// we're in a valid location and clear to do a raymarch
		result.dist = 0.0f;
		for ( int i = 0; i < maxSteps; i++ ) {
			float d = de( rayOrigin + result.dist * rayDirection, wavelength );
			if ( d < epsilon ) {
				// we have a hit - gather intersection information
				result.materialType = hitSurfaceType;
				result.albedo = hitAlbedo;
				result.frontFacing = !invert; // for now, this will be sufficient to make decisions re: IoR
				result.IoR = getIORForMaterial( hitSurfaceType, wavelength );
				result.normal = SDFNormal( rayOrigin + result.dist * rayDirection, wavelength );
				// if ( dot( rayDirection, result.normal ) < 0.0f ) result.normal = -result.normal;
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
