#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
//=============================================================================================================================
layout( location = 0, rgba8ui ) readonly uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;
//=============================================================================================================================
uniform ivec2 tileOffset;
uniform ivec2 noiseOffset;
uniform int wangSeed;
//=============================================================================================================================
#include "random.h"		// rng + remapping utilities
#include "twigl.glsl"	// noise, some basic math utils
#include "noise.h"		// more noise
#include "wood.h"		// DeanTheCoder's procedural wood texture
#include "hg_sdf.glsl"	// SDF modeling + booleans, etc
#include "mathUtils.h"	// couple random math utilities
#include "colorRamps.glsl.h" // 1d -> 3d color mappings
#include "colorspaceConversions.glsl" // various colorspace conversions
#include "glyphs.h"		// the uint encoded glyph masks
#include "consistentPrimitives.glsl.h" // primitives with the same handling of normal, etc
//=============================================================================================================================
bool BoundsCheck( in ivec2 loc ) {
	const ivec2 b = ivec2( imageSize( accumulatorColor ) ).xy;
	return ( loc.x < b.x && loc.y < b.y && loc.x >= 0 && loc.y >= 0 );
}
//=============================================================================================================================
vec4 Blue( in ivec2 loc ) {
	return imageLoad( blueNoise, ( loc + noiseOffset ) % imageSize( blueNoise ).xy );
}
//=============================================================================================================================
//=============================================================================================================================
#define NONE	0
#define BLUE	1
#define UNIFORM	2
#define WEYL	3
#define WEYLINT	4
//=============================================================================================================================
uniform int sampleNumber;
uniform int subpixelJitterMethod;
//=============================================================================================================================
vec2 SubpixelOffset () {
	vec2 offset = vec2( 0.0f );
	switch ( subpixelJitterMethod ) {
		case NONE:
			offset = vec2( 0.5f );
			break;

		case BLUE:
			bool oddFrame = ( sampleNumber % 2 == 0 );
			offset = ( oddFrame ?
				Blue( ivec2( gl_GlobalInvocationID.xy + tileOffset ) ).xy :
				Blue( ivec2( gl_GlobalInvocationID.xy + tileOffset ) ).zw ) / 255.0f;
			break;

		case UNIFORM:
			offset = vec2( NormalizedRandomFloat(), NormalizedRandomFloat() );
			break;

		case WEYL:
			offset = fract( float( sampleNumber ) * vec2( 0.754877669f, 0.569840296f ) );
			break;

		case WEYLINT:
			offset = fract( vec2( sampleNumber * 12664745, sampleNumber * 9560333 ) / exp2( 24.0f ) );	// integer mul to avoid round-off
			break;

		default:
			break;
	}
	return offset;
}
//=============================================================================================================================
//=============================================================================================================================
uniform vec3 viewerPosition;
uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;

uniform float uvScalar;
uniform int cameraType;
uniform bool cameraOriginJitter;
uniform float voraldoCameraScalar;
uniform int maxBounces;
uniform int bokehMode;
uniform float FoV;
uniform float maxDistance;
uniform float epsilon;
uniform float exposure;

uniform float marbleRadius;

uniform bool thinLensEnable;
uniform float thinLensFocusDistance;
uniform float thinLensJitterRadiusInner;
uniform float thinLensJitterRadiusOuter;
//=============================================================================================================================
// key structs
//=============================================================================================================================
//=============================================================================================================================
struct ray_t {
	vec3 origin;
	vec3 direction;
};
//=============================================================================================================================
struct intersection_t {
	float dTravel;
	vec3 albedo;
	vec3 normal;
	bool frontfaceHit;
	int materialID;
	float IoR;
	float roughness;

	// tbd:
	// material properties... IoR, roughness...
	// absorption state...
};
//=============================================================================================================================
// default "constructor"
//=============================================================================================================================
intersection_t DefaultIntersection() {
	intersection_t intersection;
	intersection.dTravel = maxDistance;
	intersection.albedo = vec3( 0.0f );
	intersection.normal = vec3( 0.0f );
	intersection.frontfaceHit = false;
	intersection.materialID = 0;
	intersection.IoR = 1.0f / 1.5f;
	intersection.roughness = 0.1f;
	return intersection;
}
//=============================================================================================================================
struct result_t {
	vec4 color;
	vec4 normalD;
};
//=============================================================================================================================

//=============================================================================================================================
// getting the camera ray for the current pixel
//=============================================================================================================================
#define NORMAL		0
#define SPHERICAL	1
#define SPHERICAL2	2
#define SPHEREBUG	3
#define SIMPLEORTHO	4
#define ORTHO		5
#define COMPOUND	6
#define VORALDO		7
//=============================================================================================================================
ray_t GetCameraRayForUV( in vec2 uv ) { // switchable cameras ( fisheye, etc ) - Assumes -1..1 range on x and y
	const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

	const vec2 uvCache = uv;

	ray_t r;
	r.origin	= vec3( 0.0f );
	r.direction	= vec3( 0.0f );

	// apply uvScalar here, probably

	switch ( cameraType ) {
	case NORMAL: {
		r.origin = viewerPosition;
		r.direction = normalize( aspectRatio * uv.x * basisX + uv.y * basisY + ( 1.0f / FoV ) * basisZ );
		break;
	}

	case SPHERICAL: {
		uv *= uvScalar;
			uv *= 0.4f;
		uv.y /= aspectRatio;
		uv.x -= 0.05f;
		uv = vec2( atan( uv.y, uv.x ) + 0.5f, ( length( uv ) + 0.5f ) * acos( -1.0f ) );
		vec3 baseVec = normalize( vec3( cos( uv.y ) * cos( uv.x ), sin( uv.y ), cos( uv.y ) * sin( uv.x ) ) );
		baseVec = Rotate3D( PI / 2.0f, vec3( 2.5f, 0.4f, 1.0f ) ) * baseVec; // this is to match the other camera
		r.origin = viewerPosition;
		r.direction = normalize( -baseVec.x * basisX + baseVec.y * basisY + ( 1.0f / FoV ) * baseVec.z * basisZ );
		break;
	}

	case SPHERICAL2: {
		uv *= uvScalar;
		mat3 rotY = Rotate3D( uv.y, basisX ); // rotate up/down by uv.y
		mat3 rotX = Rotate3D( uv.x, basisY ); // rotate left/right by uv.x
		r.origin = viewerPosition;
		r.direction = rotX * rotY * basisZ;
		break;
	}

	case SPHEREBUG: {
		uv *= uvScalar;
		uv.y /= aspectRatio;
		uv = vec2( atan( uv.y, uv.x ) + 0.5f, ( length( uv ) + 0.5f ) * acos( -1.0f ) );
		vec3 baseVec = normalize( vec3( cos( uv.y ) * cos( uv.x ), sin( uv.y ), cos( uv.y ) * sin( uv.x ) ) );
		r.origin = viewerPosition;
		r.direction = normalize( -baseVec.x * basisX + baseVec.y * basisY + ( 1.0f / FoV ) * baseVec.z * basisZ );
		break;
	}

	case SIMPLEORTHO: { // basically a long focal length, not quite orthographic
	// this isn't correct - need to adjust ray origin with basisX and basisY, and set ray direction equal to basisZ
		uv.y /= aspectRatio;
		vec3 baseVec = vec3( uv * FoV, 1.0f );
		r.origin = viewerPosition;
		r.direction = normalize( basisX * baseVec.x + basisY * baseVec.y + basisZ * baseVec.z );
		break;
	}

	case ORTHO: {
		uv.y /= aspectRatio;
		r.origin = viewerPosition + basisX * FoV * uv.x + basisY * FoV * uv.y;
		r.direction = basisZ;
		break;
	}

	case COMPOUND: {
		// compound eye hex vision by Samon
		// https://www.shadertoy.com/view/4lfcR7

		// strange, original one wanted uv in 1..2
		// uv.y /= aspectRatio;
		uv.x *= aspectRatio;
		uv.xy /= 2.0f;
		uv.xy += 1.0f;

		// Estimate hex coordinate
		float R = 0.075f;
		vec2 grid;
		grid.y = floor( uv.y / ( 1.5f * R ) );
		float odd = mod( grid.y, 2.0f );
		grid.x = floor( uv.x / ( 1.732050807 * R ) - odd * 0.5f );

		// Find possible centers of hexagons
		vec2 id = grid;
		float o = odd;
		vec2 h1 = vec2( 1.732050807f * R * ( id.x + 0.5f * o ), 1.5f * id.y * R );
		id = grid + vec2( 1.0f, 0.0f ); o = odd;
		vec2 h2 = vec2( 1.732050807f * R * ( id.x + 0.5f * o ), 1.5f * id.y * R );
		id = grid + vec2( odd, 1.0f ); o = 1.0f - odd;
		vec2 h3 = vec2( 1.732050807f * R * ( id.x + 0.5f * o ), 1.5f * id.y * R );

		// Find closest center
		float d1 = dot( h1 - uv, h1 - uv );
		float d2 = dot( h2 - uv, h2 - uv );
		float d3 = dot( h3 - uv, h3 - uv );
		if ( d2 < d1 ) {
			d1 = d2;
			h1 = h2;
		}
		if ( d3 < d1 ) {
			d1 = d3;
			h1 = h3;
		}

		// Hexagon UV
		vec2 uv2 = uv - h1;

		// Set Hexagon offset
		const float offset = 2.0f;
		uv = ( uv.xy - 1.0f ) + uv2 * offset;

		// Per Facet Fisheye effect (optional)
		vec2 coords = ( uv2 - 0.5f * R ) * 2.0f;
		vec2 fisheye;
		float intensity = 1.0f;
		fisheye.x = ( 1.0f - coords.y * coords.y ) * intensity * ( coords.x );
		fisheye.y = ( 1.0f - coords.x * coords.x ) * intensity * ( coords.y );
		uv -= fisheye * R;

		// and now get the parameters from the remapped uv
		r.direction = normalize( aspectRatio * uv.x * basisX + uv.y * basisY + ( 1.0f / FoV ) * basisZ );
		r.origin = viewerPosition;
		break;
	}

	case VORALDO:
	{
		uv.y /= aspectRatio;
		vec3 viewerOffset = basisX * FoV * uv.x + basisY * FoV * uv.y;
		r.origin = viewerPosition + viewerOffset;
		r.direction = normalize( 2.0f * basisZ + viewerOffset * voraldoCameraScalar );

		break;
	}

	default:
		break;
	}

	if ( cameraOriginJitter ) {
		r.origin += ( Blue( ivec2( gl_GlobalInvocationID.xy ) ).z / 255.0f ) * 0.1f * r.direction;
	}

	if ( thinLensEnable || cameraType == SPHEREBUG ) { // or we want that fucked up split sphere behavior... sphericalFucked, something
		// thin lens adjustment
		vec3 focuspoint = r.origin + ( ( r.direction * thinLensFocusDistance ) / dot( r.direction, basisZ ) );

		// maybe try some different mappings... something non-linear, ramping up more quickly at the edges
		const float dCenter = distance( uvCache, vec2( 0.0f ) );
		float lerpedRadius = mix( thinLensJitterRadiusInner, thinLensJitterRadiusOuter, dCenter * dCenter * dCenter );
		vec2 diskOffset = lerpedRadius * GetBokehOffset( bokehMode );
		r.origin += diskOffset.x * basisX + diskOffset.y * basisY;
		r.direction = normalize( focuspoint - r.origin );
	}

	return r;
}

//=============================================================================================================================
uniform sampler2D skyCache;
uniform bool skyInvert;
uniform float skyBrightnessScalar;
uniform float skyClamp;
//=============================================================================================================================
vec3 SkyColor( ray_t ray ) {
	ray.direction = -ray.direction.yzx;
	// sample the texture
	if ( skyInvert ) {
		ray.direction.y *= -1.0f;
	}

	float elevationFactor = dot( ray.direction, vec3( 0.0f, 1.0f, 0.0f ) );
	const float epsilonBump = 1.0f / 4096.0f;
	vec2 samplePoint = vec2(
		RangeRemapValue( atan( ray.direction.x, ray.direction.z ), -pi, pi, 0.0f + epsilonBump / 2.0f, 1.0f - epsilonBump / 2.0f ),
		// RangeRemapValue( atan( ray.direction.x, ray.direction.z ), -pi, pi, 0.0f, 1.0f ),
		RangeRemapValue( elevationFactor, -1.0f, 1.0f, 0.0f, 1.0f )
	);

	if ( skyClamp > 0.0f ) {
		return clamp( texture( skyCache, samplePoint ).rgb * skyBrightnessScalar, vec3( 0.0f ), vec3( skyClamp ) );
	} else {
		return texture( skyCache, samplePoint ).rgb * skyBrightnessScalar;
	}
}
//=============================================================================================================================
// evaluate the material's effect on this ray
//=============================================================================================================================
#define NOHIT						0
#define EMISSIVE					1
#define EMISSIVE_FRESNEL            2
#define DIFFUSE						3
#define METALLIC					4
#define MIRROR						5
#define PERFECTMIRROR				6
#define POLISHEDWOOD				7
#define WOOD						8
#define MALACHITE					9
#define LUMARBLE					10
#define LUMARBLE2					11
#define LUMARBLE3					12
#define LUMARBLECHECKER				13
#define CHECKER						14
#define REFRACTIVE					15
//=============================================================================================================================
// objects shouldn't have this material set directly, it is used for backface hits
#define REFRACTIVE_BACKFACE			100
//=============================================================================================================================
float Reflectance ( const float cosTheta, const float IoR ) {
#if 1
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
//=============================================================================================================================
vec3 anglePhong( float a, vec3 n ){
	float r1 = NormalizedRandomFloat();
	float r2 = NormalizedRandomFloat();
	float t = pow( r2, 2.0f / ( 1.0f + a ) );
	float x = cos( 2.0f * 3.14159f * r1 ) * sqrt( 1.0f - t );
	float y = sin( 2.0f * 3.14159f * r1 ) * sqrt( 1.0f - t );
	float z = sqrt( t );
	vec3 W = ( abs( n.x ) > 0.99f ) ? vec3( 0.0f, 1.0f, 0.0f ) : vec3( 1.0f, 0.0f, 0.0f );
	vec3 N = n;
	vec3 T = normalize( cross( N, W ) );
	vec3 B = cross( T, N );
	return normalize( x * T + y * B + z * N );
}
//=============================================================================================================================
bool EvaluateMaterial( inout vec3 finalColor, inout vec3 throughput, in intersection_t intersection, inout ray_t ray, in ray_t rayPrevious ) {
	// precalculate reflected vector, random diffuse vector, random specular vector
	const vec3 reflectedVector = reflect( rayPrevious.direction, intersection.normal );
	const vec3 randomVectorCosineWeighted = cosWeightedRandomHemisphereDirection( intersection.normal );
	// const vec3 randomVectorDiffuse = normalize( ( 1.0f + epsilon ) * intersection.normal + RandomUnitVector() );

	vec3 randomVectorDiffuse = RandomUnitVector();
	if ( dot( randomVectorDiffuse, intersection.normal ) <= 0.0f ) {
		randomVectorDiffuse = -randomVectorDiffuse;
	}

	const vec3 randomVectorSpecular = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), intersection.roughness ) );

	// if the ray escapes
	if ( intersection.dTravel >= maxDistance ) {

		// contribution from the skybox - dithered slightly to deal with banding
		finalColor += throughput * SkyColor( ray ) + ( RandomUnitVector() * 0.02f - 0.01f );
		return false;

	} else {

		// // handle volumetrics, here? something where you attenuate by a term, weighted by exp( -distance )
		// 	// with a chance to randomly scatter, which will give a random ray, and the position will be the point where it scatters
		// // from Nikolay
		// float distanceTerm = exp( -intersection.dTravel * 0.35f );
		// // float distanceTerm = exp( -intersection.dTravel * 1.0f );
		// if ( NormalizedRandomFloat() > distanceTerm && intersection.dTravel > 1.0f ) {
		// 	vec3 prevDIr = ray.direction;
		// 	vec3 other = vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() ) * 2.0f - 1.0f;
		// 	ray.direction = normalize( other + anglePhong( 1800.0f, ray.direction ) * 0.15f );
		// 	throughput *= vec3( 0.618f, 0.385f, 0.112f );
		// 	return true;
		// }

		// material specific behavior
		switch( intersection.materialID ) {
		case NOHIT: { // no op
			return false;
			break;
		}

		case EMISSIVE: { // light emitting material
			ray.direction = randomVectorDiffuse;
			finalColor += throughput * intersection.albedo;
			break;
		}

		case EMISSIVE_FRESNEL: {
		    ray.direction = randomVectorDiffuse;
            finalColor += throughput * intersection.albedo * clamp( Reflectance( min( dot( -normalize( ray.direction ), intersection.normal ), 1.0f ), 1.5f ), 0.0f, 10.0f );
		    break;
		}

		case DIFFUSE: {
			ray.direction = randomVectorCosineWeighted;
			throughput *= intersection.albedo;

			// ray.direction = randomVectorDiffuse;
			// throughput *= ( intersection.albedo * saturate( dot( intersection.normal, ray.direction ) ) ) / pi;
			break;
		}

		case METALLIC: {
			throughput *= intersection.albedo;
			ray.direction = randomVectorSpecular;
			break;
		}

		case MIRROR: {	// mirror surface ( some attenuation )
			// throughput *= 0.618f;
			throughput *= intersection.albedo;
			ray.direction = reflectedVector;
			break;
		}

		case PERFECTMIRROR: {	// perfect mirror
			ray.direction = reflectedVector;
			break;
		}

		case POLISHEDWOOD: {
			if ( NormalizedRandomFloat() < 0.01f ) { // mirror behavior
				ray.direction = reflectedVector;
			} else {
				throughput *= matWood( ray.origin / 10.0f );
				ray.direction = randomVectorDiffuse;
			}
			break;
		}

		case WOOD: {
			throughput *= matWood( ray.origin / 10.0f );
			ray.direction = randomVectorDiffuse;
			break;
		}

		case MALACHITE: {
			vec3 tex = matWood( ray.origin );
			const float lumaValue = GetLuma( tex ).r;
			if ( NormalizedRandomFloat() < 0.1f * lumaValue ) {
				ray.direction = reflectedVector;
			} else {
				throughput *= tex.brg;
				ray.direction = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), 1.0f - lumaValue ) );

				// throughput *= matWood( ray.origin ).brg; // this swizzle makes a nice light green / dark green mix
				// ray.direction = randomVectorDiffuse;
			}
			break;
		}

		case LUMARBLE: {
			if ( NormalizedRandomFloat() < 0.05f ) {
				ray.direction = reflectedVector;
			} else {
				throughput *= GetLuma( matWood( ray.origin ) ); // grayscale version of the wood material
				ray.direction = randomVectorDiffuse;
			}
			break;
		}

		case LUMARBLE2: {
			float lumaValue = GetLuma( matWood( ray.origin ) ).x;
			throughput *= lumaValue;
			ray.direction = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), 1.0f - lumaValue ) );
			break;
		}

		case LUMARBLE3: {
			float lumaValue = GetLuma( matWood( ray.origin ) ).x;
			throughput *= lumaValue;
			ray.direction = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), lumaValue ) );
			break;
		}

		case LUMARBLECHECKER: {
			// the checkerboard
			bool blackOrWhite = ( step( 0.0f,
				cos( PI * ray.origin.x + PI / 2.0f ) *
				cos( PI * ray.origin.y + PI / 2.0f ) *
				cos( PI * ray.origin.z + PI / 2.0f ) ) == 0 );

			float lumaValue = GetLuma( matWood( ray.origin ) ).x;
			throughput *= lumaValue;
			ray.direction = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), blackOrWhite ? lumaValue : 1.0f - lumaValue ) );
			break;
		}

		case CHECKER: {
			// diffuse material
			const float thresh = 0.01f;
			const float threshSmaller = 0.005f;
			vec3 p = ray.origin;

			bool redLines = (
				// thick lines at zero
				( abs( p.x ) < thresh ) ||
				( abs( p.y ) < thresh ) ||
				( abs( p.z ) < thresh ) ||
				// thinner lines every 5
				( abs( mod( p.x, 5.0f ) ) < threshSmaller ) ||
				( abs( mod( p.y, 5.0f ) ) < threshSmaller ) ||
				( abs( mod( p.z, 5.0f ) ) < threshSmaller ) );

			// the checkerboard
			bool blackOrWhite = ( step( 0.0f,
				cos( PI * p.x + PI / 2.0f ) *
				cos( PI * p.y + PI / 2.0f ) *
				cos( PI * p.z + PI / 2.0f ) ) == 0 );

			vec3 color = redLines ? vec3( 1.0f, 0.1f, 0.1f ) : ( blackOrWhite ? vec3( 0.618f ) : vec3( 0.1618f ) );
			throughput *= color;
			ray.direction = ( blackOrWhite || redLines ) ? randomVectorDiffuse : reflectedVector;
			break;
		}

		case REFRACTIVE: {
			throughput *= intersection.albedo; // temp

			ray.origin -= 2.0f * epsilon * intersection.normal;
			float cosTheta = min( dot( -normalize( ray.direction ), intersection.normal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( intersection.IoR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, intersection.IoR ) > NormalizedRandomFloat() ) {
				ray.direction = normalize( mix( reflect( normalize( ray.direction ), intersection.normal ), RandomUnitVector(), intersection.roughness ) );
			} else {
				ray.direction = normalize( mix( refract( normalize( ray.direction ), intersection.normal, intersection.IoR ), RandomUnitVector(), intersection.roughness ) );
			}
			break;
		}

		case REFRACTIVE_BACKFACE: {
			throughput *= intersection.albedo; // temp

			ray.origin += 2.0f * epsilon * intersection.normal;
			intersection.normal = -intersection.normal;
			float adjustedIOR = 1.0f / intersection.IoR;
			float cosTheta = min( dot( -normalize( ray.direction ), intersection.normal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( adjustedIOR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, adjustedIOR ) > NormalizedRandomFloat() ) {
				ray.direction = normalize( mix( reflect( normalize( ray.direction ), intersection.normal ), RandomUnitVector(), intersection.roughness ) );
			} else {
				ray.direction = normalize( mix( refract( normalize( ray.direction ), intersection.normal, adjustedIOR ), RandomUnitVector(), intersection.roughness ) );
			}
			break;
		}

		default:
			break;
		}
	}
	// some material was encountered
	return true;
}
//=============================================================================================================================
// SDF RAYMARCH GEOMETRY
//=============================================================================================================================
uniform bool raymarchEnable;
uniform float raymarchMaxDistance;
uniform float raymarchUnderstep;
uniform int raymarchMaxSteps;
//=============================================================================================================================
// global state tracking
vec3 hitColor;
int hitSurfaceType;
float hitRoughness;
vec3 orbitColor = vec3( 0.0f );
//=============================================================================================================================

// float deFractal( vec3 p ) {
// 	float s=3.;
// 	for(int i = 0; i < 4; i++) {
// 		p=mod(p-1.,2.)-1.;
// 		float r=1.2/dot(p,p);
// 		p*=r; s*=r;
// 	}
// 	p = abs(p)-0.8;
// 	if (p.x < p.z) p.xz = p.zx;
// 	if (p.y < p.z) p.yz = p.zy;
// 	if (p.x < p.y) p.xy = p.yx;
// 	if (p.x < p.z) p.xz = p.zx;
// 	return length(cross(p,normalize(vec3(0,1,1))))/s-.001;
// }

float deFractal2 ( vec3 p ) {
  float d = 0.0f;
  float t = 0.3f;
  float s = 1.0f;
  vec4 r = vec4( 0.0f );
  vec4 q = vec4( p, 0.0f );
  for ( int j = 0; j < 4 ; j++ )
    r = max( r *= r *= r = mod( q * s + 1.0f, 2.0f ) - 1.0f, r.yzxw ),
    d = max( d, ( 0.27f - length( r ) * 0.3f ) / s ),
    s *= 3.1f;
  return d;
}

#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
float deJune ( vec3 p ) {
	p=abs(p)-3.;
	if(p.x < p.z)p.xz=p.zx;
	if(p.y < p.z)p.yz=p.zy;
	if(p.x < p.y)p.xy=p.yx;
	float s=2.; vec3 off=p*.5;
	for(int i=0;i<12;i++){
		p=1.-abs(p-1.);
		float k=-1.1*max(1.5/dot(p,p),1.5);
		s*=abs(k); p*=k; p+=off;
		p.zx*=rot(-1.2);
	}
	float a=2.5;
	p-=clamp(p,-a,a);
	return length(p)/s;
}

float deFractal( vec3 pos ) {
	vec3 tpos = pos;
	tpos.xz = abs( 0.5f - mod( tpos.xz, 1.0f ) );
	vec4 p = vec4( tpos, 1.0f );
	float y = max( 0.0f, 0.35f - abs( pos.y - 3.35f ) ) / 0.35f;
	for ( int i = 0; i < 7; i++ ) {
		p.xyz = abs( p.xyz ) - vec3( -0.02f, 1.98f, -0.02f );
		p = p * ( 2.0f + 0.0f * y ) / clamp( dot( p.xyz, p.xyz ), 0.4f, 1.0f ) - vec4( 0.5f, 1.0f, 0.4f, 0.0f );
		p.xz *= mat2( -0.416f, -0.91f, 0.91f, -0.416f );
	}
	return ( length( max( abs( p.xyz ) - vec3( 0.1f, 5.0f, 0.1f ), vec3( 0.0f ) ) ) - 0.05f ) / p.w;
}

float deLeaf( vec3 p ) {
	float S = 1.0f;
	float R, e;
	float time = 0.5f;
	p.y += p.z;
	p = vec3( log( R = length( p ) ) - time, asin( -p.z / R ), atan( p.x, p.y ) + time );
	for( e = p.y - 1.5f; S < 6e2; S += S ) {
		e += sqrt( abs( dot( sin( p.zxy * S ), cos( p * S ) ) ) ) / S;
		orbitColor = mix( orbitColor, refPalette( mod( 0.4f * S, 1.0f ), INFERNO2 ).xyz, 0.9f );
	}
	return e * R * 0.1f;
}

float deGazTemple1( vec3 p ){
	float s = 2.;
	float e = 0.;
	for(int j=0;++j<7;)
		p.xz=abs(p.xz)-2.3,
		p.z>p.x?p=p.zyx:p,
		p.z=1.5-abs(p.z-1.3+sin(p.z)*.2),
		p.y>p.x?p=p.yxz:p,
		p.x=3.-abs(p.x-5.+sin(p.x*3.)*.2),
		p.y>p.x?p=p.yxz:p,
		p.y=.9-abs(p.y-.4),
		e=12.*clamp(.3/min(dot(p,p),1.),.0,1.)+
		2.*clamp(.1/min(dot(p,p),1.),.0,1.),
		p=e*p-vec3(7,1,1),
		s*=e;
	return length(p)/s;
}

float deGazTemple2(vec3 p){
	for(int j=0;++j<8;)
		p.z-=.3,
		p.xz=abs(p.xz),
		p.xz=(p.z>p.x)?p.zx:p.xz,
		p.xy=(p.y>p.x)?p.yx:p.xy,
		p.z=1.-abs(p.z-1.),
		p=p*3.-vec3(10,4,2);
	return length(p)/6e3-.001;
}

float deGazTemple3( vec3 p ){
	p.z-=2.5;
	float s = 3.;
	float e = 0.;
	for(int j=0;j++<8;)
		s*=e=3.8/clamp(dot(p,p),0.,2.),
		p=abs(p)*e-vec3(1,15,1);
	return length(cross(p,vec3(1,1,-1)*.577))/s;
}

float deGazTemple4( vec3 p ){
	vec3 a=vec3(.5);
	p.z-=55.; p = abs(p);
	float s=2., l=0.;
	for(int j=0;j++<8;)
		p=-sign(p)*(abs(abs(abs(p)-2.)-1.)-1.),
		s*=l=-2.12/max(.2,dot(p,p)),
		p=p*l-.55;
	return dot(p,a)/s;
}

#define fold45(p)(p.y>p.x)?p.yx:p
float deGazTemple5( vec3 p ) {
	float scale = 2.1, off0 = .8, off1 = .3, off2 = .83;
	vec3 off =vec3(2.,.2,.1);
	float s=1.0;
	for(int i = 0;++i<20;) {
		p.xy = abs(p.xy);
		p.xy = fold45(p.xy);
		p.y -= off0;
		p.y = -abs(p.y);
		p.y += off0;
		p.x += off1;
		p.xz = fold45(p.xz);
		p.x -= off2;
		p.xz = fold45(p.xz);
		p.x += off1;
		p -= off;
		p *= scale;
		p += off;
		s *= scale;
	}
	return length(p)/s;
}

float deRingo ( vec3 p ) {
  float e, s, t=0.0; // time adjust term
  vec3 q=p;
  p.z+=7.;
  p=vec3(log(s=length(p)),atan(p.y,p.x),sin(t/4.+p.z/s));
  s=1.;
  for(int j=0;j++<6;)
    s*=e=PI/min(dot(p,p),.8),
    p=abs(p)*e-3.,
    p.y-=round(p.y);
  return e=length(p)/s;
}

float deWater( vec3 p ) {
	float e, i=0., j, f, a, w;
	// p.yz *= mat2(cos(0.7f),sin(0.7f),-sin(0.7f),cos(0.7f));
	f = 0.3; // wave amplitude
	i < 45. ? p : p -= .001;
	e = p.y + 5.;
	for( a = j = .9; j++ < 30.; a *= .8 ){
		vec2 m = vec2( 1. ) * mat2(cos(j),sin(j),-sin(j),cos(j));
		// float x = dot( p.xz, m ) * f + t + t; // time varying behavior
		float x = dot( p.xz, m ) * f + 0.;
		w = exp( sin( x ) - 1. );
		p.xz -= m * w * cos( x ) * a;
		e -= w * a;
		f *= 1.2;
	}
	return e;
}

float deSquiggles(vec3 p){
	float k = pi*2.;
	vec3 v = vec3(0.,3.,fract(k));
	return (length(cross(cos(p+v),p.zxy))-0.4)*0.2;
}

vec3 Rotate(vec3 z,float AngPFXY,float AngPFYZ,float AngPFXZ) {
        float sPFXY = sin(radians(AngPFXY)); float cPFXY = cos(radians(AngPFXY));
        float sPFYZ = sin(radians(AngPFYZ)); float cPFYZ = cos(radians(AngPFYZ));
        float sPFXZ = sin(radians(AngPFXZ)); float cPFXZ = cos(radians(AngPFXZ));

        float zx = z.x; float zy = z.y; float zz = z.z; float t;

        // rotate BACK
        t = zx; // XY
        zx = cPFXY * t - sPFXY * zy; zy = sPFXY * t + cPFXY * zy;
        t = zx; // XZ
        zx = cPFXZ * t + sPFXZ * zz; zz = -sPFXZ * t + cPFXZ * zz;
        t = zy; // YZ
        zy = cPFYZ * t - sPFYZ * zz; zz = sPFYZ * t + cPFYZ * zz;
        return vec3(zx,zy,zz);
}

float deTT( vec3 p ) {
    float Scale = 1.34f;
    float FoldY = 1.5709f;
    float FoldX = 1.425709f;
    float FoldZ = 0.035271f;
    float JuliaX = -1.763517f;
    float JuliaY = 1.92486f;
    float JuliaZ = -1.734913f;
    float AngX = -51.080209f;
    float AngY = 0.4f;
    float AngZ = -19.096322f;
    float Offset = -3.036726f;
    int EnableOffset = 1;
    int Iterations = 80;
    float Precision = 1.0f;
    // output _sdf c = _SDFDEF)

    vec4 OrbitTrap = vec4(1,1,1,1);
    float u2 = 1;
    float v2 = 1;
    if(EnableOffset==1)p = Offset+abs(vec3(p.x,p.y,p.z));

    vec3 p0 = vec3(JuliaX,JuliaY,JuliaZ);
    float l = 0.0;
    int i=0;
    for (i=0; i<Iterations; i++) {
        p = Rotate(p,AngX,AngY,AngZ);
        p.x=abs(p.x+FoldX)-FoldX;
        p.y=abs(p.y+FoldY)-FoldY;
        p.z=abs(p.z+FoldZ)-FoldZ;
        p=p*Scale+p0;
        l=length(p);
        float rr = dot(p,p);
    }
    return Precision*(l)*pow(Scale, -float(i));
}

float deGazTemple6(vec3 p){
	float s=2., l=0.;
	p=abs(p);
	for(int j=0;j++<8;)
		p=1.-abs(abs(p-2.)-1.),
		p*=l=1.2/dot(p,p), s*=l;
	return dot(p,normalize(vec3(3,-2,-1)))/s;
}

float deUnguate(vec3 p){
	float n=1.+snoise3D(p), s=4., e;
	for(int i=0;i++<7;p.y-=20.*n)
		p.xz=.8-abs(p.xz),
		p.x < p.z?p=p.zyx:p,
		s*=e=2.1/min(dot(p,p),1.),
		p=abs(p)*e-n;
	return length(p)/s+1e-4;
}

float deGazTemple8(vec3 p){
	p=mod(p,2.)-1.;
	p=abs(p)-1.;
	if(p.x < p.z)p.xz=p.zx;
	if(p.y < p.z)p.yz=p.zy;
	if(p.x < p.y)p.xy=p.yx;
	float s=1.;
	for(int i=0;i<10;i++){
		float r2=2./clamp(dot(p,p),.1,1.);
		p=abs(p)*r2-vec3(.6,.6,3.5);
		s*=r2;
	}
	return length(p)/s;
}

float deAnemone(vec3 p){
	#define V vec2(.7,-.7)
	#define G(p)dot(p,V)
	float i=0.,g=0.,e=1.;
	float t = 6.9; // change to see different behavior
	for(int j=0;j++<8;){
		p=abs(rotate3D(t,vec3(1,-3,5))*p*2.)-1.,
		p.xz-=(G(p.xz)-sqrt(G(p.xz)*G(p.xz)+.05))*V;
	}
	return length(p.xz)/3e2;
}
float escape;
  float deJuly(vec3 p0){
    vec4 p = vec4(p0, 1.);
    escape = 0.;
    p=abs(p);
    if(p.x < p.z)p.xz = p.zx;
    if(p.z < p.y)p.zy = p.yz;
    if(p.y < p.x)p.yx = p.xy;
    for(int i = 0; i < 20; i++){
      if(p.x < p.z)p.xz = p.zx;
      if(p.z < p.y)p.zy = p.yz;
      if(p.y < p.x)p.yx = p.xy;
      p = abs(p);
      p*=(1.9/clamp(dot(p.xyz,p.xyz),0.1,1.));
      p.xyz-=vec3(0.2,1.9,0.6);
    //   p.xyz-=vec3(1.7,1.6,0.2);
    //   p.xyz-=vec3(0.9,1.1,0.5);
      escape += exp(-0.2*dot(p.xyz,p.xyz));
    }
    float m = 1.2;
    p.xyz-=clamp(p.xyz,-m,m);
    return (length(p.xyz)/p.w);
  }

  mat2 rot2(in float a){ float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
  float deFEFE(vec3 p){
    float d = 1e5;
    const int n = 3;
    const float fn = float(n);
    for(int i = 0; i < n; i++){
      vec3 q = p;
      float a = float(i)*fn*2.422; //*6.283/fn
      a *= a;
      q.z += float(i)*float(i)*1.67; //*3./fn
      q.xy *= rot2(a);
      float b = (length(length(sin(q.xy) + cos(q.yz))) - .15);
      float f = max(0., 1. - abs(b - d));
      d = min(d, b) - .25*f*f;
    }
    return d;
  }

// please, do not use in real projects - replace this by something better
float hashiq(vec3 p) {
    p  = 17.0*fract( p*0.3183099+vec3(.11,.17,.13) );
    return fract( p.x*p.y*p.z*(p.x+p.y+p.z) );
}

// https://iquilezles.org/articles/distfunctions
float sdBoxiq( vec3 p, vec3 b ) {
    vec3 d = abs(p) - b;
    return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}

// https://iquilezles.org/articles/smin
float smaxiq( float a, float b, float k ) {
    float h = max(k-abs(a-b),0.0);
    return max(a, b) + h*h*0.25/k;
}

//---------------------------------------------------------------
// A random SDF - it places spheres of random sizes in a grid
//---------------------------------------------------------------
#define NOISE 0
float sdBaseiq( in vec3 p ) {
#if NOISE==0
    vec3 i = floor(p);
    vec3 f = fract(p);

	#define RAD(r) ((r)*(r)*0.7)
    #define SPH(i,f,c) length(f-c)-RAD(hashiq(i+c))
    
    return min(min(min(SPH(i,f,vec3(0,0,0)),
                       SPH(i,f,vec3(0,0,1))),
                   min(SPH(i,f,vec3(0,1,0)),
                       SPH(i,f,vec3(0,1,1)))),
               min(min(SPH(i,f,vec3(1,0,0)),
                       SPH(i,f,vec3(1,0,1))),
                   min(SPH(i,f,vec3(1,1,0)),
                       SPH(i,f,vec3(1,1,1)))));
#else
    const float K1 = 0.333333333;
    const float K2 = 0.166666667;
    
    vec3 i = floor(p + (p.x + p.y + p.z) * K1);
    vec3 d0 = p - (i - (i.x + i.y + i.z) * K2);
    
    vec3 e = step(d0.yzx, d0);
	vec3 i1 = e*(1.0-e.zxy);
	vec3 i2 = 1.0-e.zxy*(1.0-e);
    
    vec3 d1 = d0 - (i1  - 1.0*K2);
    vec3 d2 = d0 - (i2  - 2.0*K2);
    vec3 d3 = d0 - (1.0 - 3.0*K2);
    
    float r0 = hashiq( i+0.0 );
    float r1 = hashiq( i+i1 );
    float r2 = hashiq( i+i2 );
    float r3 = hashiq( i+1.0 );

    #define SPH(d,r) length(d)-r*r*0.55

    return min( min(SPH(d0,r0),
                    SPH(d1,r1)),
                min(SPH(d2,r2),
                    SPH(d3,r3)));
#endif
}

//---------------------------------------------------------------
// subtractive fbm
//---------------------------------------------------------------
vec2 sdFbmiq( in vec3 p, float d ) {
    const mat3 m = mat3( 0.00,  0.80,  0.60,
                        -0.80,  0.36, -0.48,
                        -0.60, -0.48,  0.64 );
    float t = 0.0;
	float s = 1.0;
    for( int i=0; i<10; i++ ) {
        float n = s*sdBaseiq(p);
    	d = smaxiq( d, -n, 0.15*s );
        t += d;
        p = 2.0*m*p;
        s = 0.55*s;
    }
    
    return vec2(d,t);
}

// vec2 deRode( in vec3 p ) {
float deRode( in vec3 p ) {
    // box
    float d = sdBoxiq( p, vec3( 0.5f, 10.0f, 5.0f ) );

    // fbm
    vec2 dt = sdFbmiq( p+0.5, d );

    dt.y = 1.0+dt.y*2.0; dt.y = dt.y*dt.y;

    return dt.x;
}

float deG(vec3 p){
	float d = 1e5;
	const int n = 3;
	const float fn = float(n);
	for(int i = 0; i < n; i++){
		vec3 q = p;
		float a = float(i)*fn*2.422; //*6.283/fn
		a *= a;
		q.z += float(i)*float(i)*1.67; //*3./fn
		q.xy *= rot2(a);
		float b = (length(length(sin(q.xy) + cos(q.yz))) - .15);
		float f = max(0., 1. - abs(b - d));
		d = min(d, b) - .25*f*f;
	}
	return d;
}

// Nikolay's water
float sumWaves(float x){
	float sumstuff = 0.;
	for(int i = 0; i < 10; i++){
		sumstuff += abs(sin(x*float(i)))/(float(i)+1.);
	}
	return (2.-sumstuff)*0.5;
}
vec2 rotXXX(vec2 a, float t){
	vec2 b = vec2(cos(t), sin(t));
	return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x );
}

float rotatewaves(vec2 py, float times){
	float addsm = 0.;
	times *= 0.004;
	for(int i = 0; i < 25; i++){
		float k = 0.73;
		// for(int i = 0; i < 150; i++){
		float m = sqrt(float(i) / 25.);
		float r = 2. * 3.14159 * k * float(i);
		// vec2 coords = vec2(m * cos(r), m * sin(r)) * CoCSize
		py = rotXXX(py, r);
		vec2 waterDir = normalize(vec2(1., 1.))*times*122.5;
		addsm += (sumWaves(py.x-2.*float(i)+waterDir.x) + sumWaves(py.y+3.*float(i)+waterDir.y));
	}
	return addsm / 15.;
}

float isWat = 0.;
float mapPl(vec3 p, float times) {
	p = p.xzy;
	p.x *= .2;
	p.y *= .3;
	// p.z += 4.;
	float plane = p.z - ((rotatewaves(p.xy, times))*1.7);
	return plane;
}

// tdhooper variant 1 - spherical inversion
vec2 wrap ( vec2 x, vec2 a, vec2 s ) {
  x -= s;
  return (x - a * floor(x / a)) + s;
}

void TransA ( inout vec3 z, inout float DF, float a, float b ) {
  float iR = 1. / dot( z, z );
  z *= -iR;
  z.x = -b - z.x;
  z.y = a + z.y;
  DF *= iR; // max( 1.0, iR );
}

float deFFFF( vec3 z ) {
  vec3 InvCenter = vec3( 0.0, 1.0, 1.0 );
  float rad = 0.8;
  float KleinR = 1.5 + 0.39;
  float KleinI = ( 0.55 * 2.0 - 1.0 );
  vec2 box_size = vec2( -0.40445, 0.34 ) * 2.0;
  vec3 lz = z + vec3( 1.0 ), llz = z + vec3( -1.0 );
  float d = 0.0; float d2 = 0.0;
  z = z - InvCenter;
  d = length( z );
  d2 = d * d;
  z = ( rad * rad / d2 ) * z + InvCenter;
  float DE = 1e12;
  float DF = 1.0;
  float a = KleinR;
  float b = KleinI;
  float f = sign( b ) * 0.45;
  for ( int i = 0; i < 80; i++ ) {
    z.x += b / a * z.y;
    z.xz = wrap( z.xz, box_size * 2.0, -box_size );
    z.x -= b / a * z.y;
    if ( z.y >= a * 0.5 + f * ( 2.0 * a - 1.95 ) / 4.0 * sign( z.x + b * 0.5 ) * 
     ( 1.0 - exp( -( 7.2 - ( 1.95 - a ) * 15.0 )* abs(z.x + b * 0.5 ) ) ) ) {
      z = vec3( -b, a, 0.0 ) - z;
    } //If above the separation line, rotate by 180° about (-b/2, a/2)
    TransA( z, DF, a, b ); //Apply transformation a
    if ( dot( z - llz, z - llz ) < 1e-5 ) {
      break;
    } //If the iterated points enters a 2-cycle, bail out
    llz = lz; lz = z; //Store previous iterates
  }
  float y =  min(z.y, a - z.y);
  DE = min( DE, min( y, 0.3 ) / max( DF, 2.0 ) );
  DE = DE * d2 / ( rad + d * DE );
  return DE;
}

float deASDF( vec3 p ) {
  float s = 4.0;
  p = abs( p );
  for( int i = 0; i < 10; i++ ) {
    p = 1.0 - abs( p - 1.0 );
    float r2 = 1.3 / dot( p, p );
    p *= r2;
    s *= r2;
  }
  return abs( p.y ) / s;
}


// from Nikolay
void sphere_fold ( inout vec3 z, inout float dz ) {
	float FixedRadius = 10.9;
	float MinRadius = 1.1;
	float r2 = dot( z, z );
	if ( r2 < MinRadius ) {
		float temp = ( FixedRadius / MinRadius );
		z *= temp; dz *= temp;
	} else if ( r2 < FixedRadius ) {
		float temp = ( FixedRadius / r2 );
		z *= temp; dz *= temp;
	}
}

void box_fold ( inout vec3 z ) {
	float folding_limit = 1.0;
	z = clamp(z, -folding_limit, folding_limit) * 2.0 - z;
}

float DEnew ( vec3 p ) {
	float s = 1.;
	float t = 9999.;
	for(int i = 0; i < 12; i++){
		float k =  .1/clamp(dot(p,p),0.01,1.);
		p *= k;
		s *= k;
		sphere_fold(p.xyz,s);
		box_fold(p.xyz);
		p=abs(p)-vec3(.2,5.,2.7);
		p=mod(p-1.,2.)-1.;
		t = min(t, length(p)/s);
	}
	return t;
}

float DEnew2 ( vec3 p ) {
	float s = 1.;
	float t = 9999.;

	for(int i = 0; i < 12; i++){
		//if(p.x > p.z)p.xz = p.zx;
		//if(p.z > p.y)p.zy = p.yz;
		//if(p.y > p.x)p.yx = p.xy;
		float k =  .12/clamp(dot(p,p),0.01,1.);
		p *= k;
		s *= k;
		sphere_fold(p.xyz,s);
		box_fold(p.xyz);
		p=abs(p)-vec3(5.2,2.,.7);
		p=mod(p-1.,2.)-1.;

		t = min(t, length(p)/s);
	}
	return t;
}

float deJAIL ( vec3 p ) {
  float d = 0.0f;
  float t = 0.3f;
  float s = 1.0f;
  vec4 r = vec4( 0.0f );
  vec4 q = vec4( p, 0.0f );
  for ( int j = 0; j < 4 ; j++ )
    r = max( r *= r *= r = mod( q * s + 1.0f, 2.0f ) - 1.0f, r.yzxw ),
    d = max( d, ( 0.27f - length( r ) * 0.3f ) / s ),
    s *= 3.1f;
  return d;
}

// // #define PI 3.14159265359

// void pR(inout vec2 p, float a) {
//     p = cos(a)*p + sin(a)*vec2(p.y, -p.x);
// }

vec2 pRi(vec2 p, float a) {
    pR(p, a);
    return p;
}

// // #define saturate(x) clamp(x, 0., 1.)

// float vmax(vec2 v) {
//     return max(v.x, v.y);
// }

// float vmax(vec3 v) {
//     return max(max(v.x, v.y), v.z);
// }

// float vmin(vec3 v) {
//     return min(min(v.x, v.y), v.z);
// }

// float vmin(vec2 v) {
//     return min(v.x, v.y);
// }

// float fBox(vec3 p, vec3 b) {
//     vec3 d = abs(p) - b;
//     return length(max(d, vec3(0))) + vmax(min(d, vec3(0)));
// }

float fCorner2(vec2 p) {
    return length(max(p, vec2(0))) + vmax(min(p, vec2(0)));
}

// float fDisc(vec3 p, float r) {
//     float l = length(p.xz) - r;
//     return l < 0. ? abs(p.y) : length(vec2(p.y, l));
// }

// IQ https://www.shadertoy.com/view/Xds3zN
float sdRoundCone( in vec3 p, in float r1, float r2, float h )
{
    vec2 q = vec2( length(p.xz), p.y );
    
    float b = (r1-r2)/h;
    float a = sqrt(1.0-b*b);
    float k = dot(q,vec2(-b,a));
    
    if( k < 0.0 ) return length(q) - r1;
    if( k > a*h ) return length(q-vec2(0.0,h)) - r2;
        
    return dot(q, vec2(a,b) ) - r1;
}

float smin2(float a, float b, float r) {
    vec2 u = max(vec2(r - a,r - b), vec2(0));
    return max(r, min (a, b)) - length(u);
}

float smax2(float a, float b, float r) {
    vec2 u = max(vec2(r + a,r + b), vec2(0));
    return min(-r, max (a, b)) + length(u);
}

float smin(float a, float b, float k){
    float f = clamp(0.5 + 0.5 * ((a - b) / k), 0., 1.);
    return (1. - f) * a + f  * b - f * (1. - f) * k;
}

float smax(float a, float b, float k) {
    return -smin(-a, -b, k);
}

float smin3(float a, float b, float k){
    return min(
        smin(a, b, k),
        smin2(a, b, k)
    );
}

float smax3(float a, float b, float k){
    return max(
        smax(a, b, k),
        smax2(a, b, k)
    );
}


float ellip(vec3 p, vec3 s) {
    float r = vmin(s);
    p *= r / s;
    return length(p) - r;
}

float ellip(vec2 p, vec2 s) {
    float r = vmin(s);
    p *= r / s;
    return length(p) - r;
}

bool isEye = false;


float dHead( vec3 p ) {

    pR(p.yz, -.1);
    p.y -= .11;

    vec3 pa = p;
    vec3 ps = p;
    ps.x = sqrt(ps.x * ps.x + .0005);
    p.x = abs(p.x);
    vec3 pp = p;

    float d = 1e12;

    // skull back
    p += vec3(0,-.135,.09);
    d = ellip(p, vec3(.395, .385, .395));

    // skull base
    p = pp;
    p += vec3(0,-.135,.09) + vec3(0,.1,.07);
    d = smin(d, ellip(p, vec3(.38, .36, .35)), .05);

    // forehead
    p = pp;
    p += vec3(0,-.145,-.175);
    d = smin(d, ellip(p, vec3(.315, .3, .33)), .18);

    p = pp;
    pR(p.yz, -.5);
    float bb = fBox(p, vec3(.5,.67,.7));
    d = smax(d, bb, .2);

    // face base
    p = pp;
    p += vec3(0,.25,-.13);
    d = smin(d, length(p) - .28, .1);

    // behind ear
    p = ps;
    p += vec3(-.15,.13,.06);
    d = smin(d, ellip(p, vec3(.15,.15,.15)), .15);

    p = ps;
    p += vec3(-.07,.18,.1);
    d = smin(d, length(p) - .2, .18);

    // cheek base
    p = pp;
    p += vec3(-.2,.12,-.14);
    d = smin(d, ellip(p, vec3(.15,.22,.2) * .8), .15);

    // jaw base
    p = pp;
    p += vec3(0,.475,-.16);
    pR(p.yz, .8);
    d = smin(d, ellip(p, vec3(.19,.1,.2)), .1);
    
    // brow
    p = pp;
    p += vec3(0,-.0,-.18);
    vec3 bp = p;
    float brow = length(p) - .36;
    p.x -= .37;
    brow = smax(brow, dot(p, normalize(vec3(1,.2,-.2))), .2);
    p = bp;
    brow = smax(brow, dot(p, normalize(vec3(0,.6,1))) - .43, .25);
    p = bp;
    pR(p.yz, -.5);
    float peak = -p.y - .165;
    peak += smoothstep(.0, .2, p.x) * .01;
    peak -= smoothstep(.12, .29, p.x) * .025;
    brow = smax(brow, peak, .07);
    p = bp;
    pR(p.yz, .5);
    brow = smax(brow, -p.y - .06, .15);
    d = smin(d, brow, .06);

    // jaw

    vec3 jo = vec3(-.25,.4,-.07);
    p = ps + jo;
    float jaw = dot(p, normalize(vec3(1,-.2,-.05))) - .069;
    jaw = smax(jaw, dot(p, normalize(vec3(.5,-.25,.35))) - .13, .12);
    jaw = smax(jaw, dot(p, normalize(vec3(-.0,-1.,-.8))) - .12, .15);
    jaw = smax(jaw, dot(p, normalize(vec3(.98,-1.,.15))) - .13, .08);
    jaw = smax(jaw, dot(p, normalize(vec3(.6,-.2,-.45))) - .19, .15);
    jaw = smax(jaw, dot(p, normalize(vec3(.5,.1,-.5))) - .26, .15);
    jaw = smax(jaw, dot(p, normalize(vec3(1,.2,-.3))) - .22, .15);

    p = pp;
    p += vec3(0,.63,-.2);
    pR(p.yz, .15);
    float cr = .5;
    jaw = smax(jaw, length(p.xy - vec2(0,cr)) - cr, .05);

    p = pp + jo;
    jaw = smax(jaw, dot(p, normalize(vec3(0,-.4,1))) - .35, .1);
    jaw = smax(jaw, dot(p, normalize(vec3(0,1.5,2))) - .3, .2);
    jaw = max(jaw, length(pp + vec3(0,.6,-.3)) - .7);

    p = pa;
    p += vec3(.2,.5,-.1);
    float jb = length(p);
    jb = smoothstep(.0, .4, jb);
    float js = mix(0., -.005, jb);
    jb = mix(.01, .04, jb);

    d = smin(d, jaw - js, jb);

    // chin
    p = pp;
    p += vec3(0,.585,-.395);
    p.x *= .7;
    d = smin(d, ellip(p, vec3(.028,.028,.028)*1.2), .15);

    // nose
    p = pp;
    p += vec3(0,.03,-.45);
    pR(p.yz, 3.);
    d = smin(d, sdRoundCone(p, .008, .05, .18), .1);

    p = pp;
    p += vec3(0,.06,-.47);
    pR(p.yz, 2.77);
    d = smin(d, sdRoundCone(p, .005, .04, .225), .05);

    // cheek

    p = pp;
    p += vec3(-.2,.2,-.28);
    pR(p.xz, .5);
    pR(p.yz, .4);
    float ch = ellip(p, vec3(.1,.1,.12)*1.05);
    d = smin(d, ch, .1);

    p = pp;
    p += vec3(-.26,.02,-.1);
    pR(p.xz, .13);
    pR(p.yz, .5);
    float temple = ellip(p, vec3(.1,.1,.15));
    temple = smax(temple, p.x - .07, .1);
    d = smin(d, temple, .1);

    p = pp;
    p += vec3(.0,.2,-.32);
    ch = ellip(p, vec3(.1,.08,.1));
    d = smin(d, ch, .1);

    p = pp;
    p += vec3(-.17,.31,-.17);
    ch = ellip(p, vec3(.1));
    d = smin(d, ch, .1);

    // mouth base
    p = pp;
    p += vec3(-.0,.29,-.29);
    pR(p.yz, -.3);
    d = smin(d, ellip(p, vec3(.13,.15,.1)), .18);

    p = pp;
    p += vec3(0,.37,-.4);
    d = smin(d, ellip(p, vec3(.03,.03,.02) * .5), .1);

    p = pp;
    p += vec3(-.09,.37,-.31);
    d = smin(d, ellip(p, vec3(.04)), .18);

    // bottom lip
    p = pp;
    p += vec3(0,.455,-.455);
    p.z += smoothstep(.0, .2, p.x) * .05;
    float lb = mix(.035, .03, smoothstep(.05, .15, length(p)));
    vec3 ls = vec3(.055,.028,.022) * 1.25;
    float w = .192;
    vec2 pl2 = vec2(p.x, length(p.yz * vec2(.79,1)));
    float bottomlip = length(pl2 + vec2(0,w-ls.z)) - w;
    bottomlip = smax(bottomlip, length(pl2 - vec2(0,w-ls.z)) - w, .055);
    d = smin(d, bottomlip, lb);
    
    // top lip
    p = pp;
    p += vec3(0,.38,-.45);
    pR(p.xz, -.3);
    ls = vec3(.065,.03,.05);
    w = ls.x * (-log(ls.y/ls.x) + 1.);
    vec3 pl = p * vec3(.78,1,1);
    float toplip = length(pl + vec3(0,w-ls.y,0)) - w;
    toplip = smax(toplip, length(pl - vec3(0,w-ls.y,0)) - w, .065);
    p = pp;
    p += vec3(0,.33,-.45);
    pR(p.yz, .7);
    float cut;
    cut = dot(p, normalize(vec3(.5,.25,0))) - .056;
    float dip = smin(
        dot(p, normalize(vec3(-.5,.5,0))) + .005,
        dot(p, normalize(vec3(.5,.5,0))) + .005,
        .025
    );
    cut = smax(cut, dip, .04);
    cut = smax(cut, p.x - .1, .05);
    toplip = smax(toplip, cut, .02);

    d = smin(d, toplip, .07);

    // seam
    p = pp;
    p += vec3(0,.425,-.44);
    lb = length(p);
    float lr = mix(.04, .02, smoothstep(.05, .12, lb));
    pR(p.yz, .1);
    p.y -= smoothstep(0., .03, p.x) * .002;
    p.y += smoothstep(.03, .1, p.x) * .007;
    p.z -= .133;
    float seam = fDisc(p, .2);
    seam = smax(seam, -d - .015, .01); // fix inside shape
    d = mix(d, smax(d, -seam, lr), .65);

    // nostrils base
    p = pp;
    p += vec3(0,.3,-.43);
    d = smin(d, length(p) - .05, .07);

    // nostrils
    p = pp;
    p += vec3(0,.27,-.52);
    pR(p.yz, .2);
    float nostrils = ellip(p, vec3(.055,.05,.06));

    p = pp;
    p += vec3(-.043,.28,-.48);
    pR(p.xy, .15);
    p.z *= .8;
    nostrils = smin(nostrils, sdRoundCone(p, .042, .0, .12), .02);

    d = smin(d, nostrils, .02);

    p = pp;
    p += vec3(-.033,.3,-.515);
    pR(p.xz, .5);
    d = smax(d, -ellip(p, vec3(.011,.03,.025)), .015);

    //return d;

    // eyelids
    p = pp;
    p += vec3(-.16,.07,-.34);
    float eyelids = ellip(p, vec3(.08,.1,.1));

    p = pp;
    p += vec3(-.16,.09,-.35);
    float eyelids2 = ellip(p, vec3(.09,.1,.07));

    // edge top
    p = pp;
    p += vec3(-.173,.148,-.43);
    p.x *= .97;
    float et = length(p.xy) - .09;

    // edge bottom
    p = pp;
    p += vec3(-.168,.105,-.43);
    p.x *= .9;
    float eb = dot(p, normalize(vec3(-.1,-1,-.2))) + .001;
    eb = smin(eb, dot(p, normalize(vec3(-.3,-1,0))) - .006, .01);
    eb = smax(eb, dot(p, normalize(vec3(.5,-1,-.5))) - .018, .05);

    float edge = max(max(eb, et), -d);

    d = smin(d, eyelids, .01);
    d = smin(d, eyelids2, .03);
    d = smax(d, -edge, .005);

    // eyeball
    p = pp;
    p += vec3(-.165,.0715,-.346);
    float eyeball = length(p) - .088;
	isEye = eyeball < d;
    d = min(d, eyeball);

    // tear duct
    p = pp;
    p += vec3(-.075,.1,-.37);
    d = min(d, length(p) - .05);

    
 	// ear
    p = pp;
    p += vec3(-.405,.12,.10);
    pR(p.xy, -.12);
    pR(p.xz, .35);
    pR(p.yz, -.3);
    vec3 pe = p;

    // base
    float ear = p.s + smoothstep(-.05, .1, p.y) * .015 - .005;
    float earback = -ear - mix(.001, .025, smoothstep(.3, -.2, p.y));

    // inner
    pR(p.xz, -.5);
    float iear = ellip(p.zy - vec2(.01,-.03), vec2(.045,.05));
    iear = smin(iear, length(p.zy - vec2(.04,-.09)) - .02, .09);
    float ridge = iear;
    iear = smin(iear, length(p.zy - vec2(.1,-.03)) - .06, .07);
    ear = smax2(ear, -iear, .04);
    earback = smin(earback, iear - .04, .02);

    // ridge
    p = pe;
    pR(p.xz, .2);
    ridge = ellip(p.zy - vec2(.01,-.03), vec2(.045,.055));
    ridge = smin3(ridge, -pRi(p.zy, .2).x - .01, .015);
    ridge = smax3(ridge, -ellip(p.zy - vec2(-.01,.1), vec2(.12,.08)), .02);

    float ridger = .01;

    ridge = max(-ridge, ridge - ridger);

    ridge = smax2(ridge, abs(p.x) - ridger/2., ridger/2.);

    ear = smin(ear, ridge, .045);

    p = pe;

    // outline
    float outline = ellip(pRi(p.yz, .2), vec2(.12,.09));
    outline = smin(outline, ellip(p.yz + vec2(.155,-.02), vec2(.035, .03)), .14);

    // edge
    float eedge = p.x + smoothstep(.2, -.4, p.y) * .06 - .03;

    float edgeo = ellip(pRi(p.yz, .1), vec2(.095,.065));
    edgeo = smin(edgeo, length(p.zy - vec2(0,-.1)) - .03, .1);
    float edgeoin = smax(abs(pRi(p.zy, .15).y + .035) - .01, -p.z-.01, .01);
    edgeo = smax(edgeo, -edgeoin, .05);

    float eedent = smoothstep(-.05, .05, -p.z) * smoothstep(.06, 0., fCorner2(vec2(-p.z, p.y)));
    eedent += smoothstep(.1, -.1, -p.z) * .2;
    eedent += smoothstep(.1, -.1, p.y) * smoothstep(-.03, .0, p.z) * .3;
    eedent = min(eedent, 1.);

    eedge += eedent * .06;

    eedge = smax(eedge, -edgeo, .01);
    ear = smin(ear, eedge, .01);
    ear = max(ear, earback);

    ear = smax2(ear, outline, .015);

    d = smin(d, ear, .015);

    // targus
    p = pp;
    p += vec3(-.34,.2,.02);
    d = smin2(d, ellip(p, vec3(.015,.025,.015)), .035);
    p = pp;
    p += vec3(-.37,.18,.03);
    pR(p.xz, .5);
    pR(p.yz, -.4);
    d = smin(d, ellip(p, vec3(.01,.03,.015)), .015);
    
    return d;
}

float deSDFSDF(vec3 p){
    return min(.65-length(fract(p+.5)-.5),p.y+.2);
}


float sdBox ( vec3 p, vec3 s ) {
  p = abs( p ) - s;
  return max( p.x, max( p.y, p.z ) );
}
float sdBox2 ( vec3 p, vec3 s ) {
  p.y += 0.1f * s.x;
  p = abs( p ) - s;
  return max( p.x, max( p.y, p.z ) );
}
float deLipsoid(vec3 p, vec3 r) {
    // vec3 r = vec3(0.2, 0.25, 0.05); // the radii on each axis
    float k0 = length(p/r);
    float k1 = length(p/(r*r));
    return k0*(k0-1.0)/k1;
}
float deLipsoid2(vec3 p, vec3 r) {
    // vec3 r = vec3(0.2, 0.25, 0.05); // the radii on each axis
	p.y += 0.1f * r.x;
    float k0 = length(p/r);
    float k1 = length(p/(r*r));
    return k0*(k0-1.0)/k1;
}
float deRings( vec3 p ) {
  float w = 0.9f;
  float t = 0.03f;
  return length(vec2(p.x * p.y * p.z, length(p) - w)) - t;
}
float deRings2( vec3 p ) {
  float w = 0.875f;
  float t = 0.031f;
  return length(vec2(p.x * p.y * p.z, length(p) - w)) - t;
}
 float deStar(vec3 p){
    vec3 Q;
    float i,d=1.;
    Q=p;
    d=.4*pi;
    Q.yx*=rotate2D(floor(atan(Q.x,Q.y)/d+.5)*d);
    Q.zx=abs(Q.zx);
    return d=max(Q.z-.06,(Q.y*.325+Q.x+Q.z*1.5)/1.83-.05);
  }
#define rotGG(j) mat2(cos(j),-sin(j),sin(j),cos(j))

// const vec3 pivot = vec3( 0.5f, 0.4f, 0.0f );
// const vec3 pivot = vec3( 0.1f, 0.9f, 0.8f );
// const vec3 pivot = vec3( -0.33f, 1.3f, 0.8f );
// const vec3 pivot = vec3( -0.2f, 1.0f, 0.6f );
const vec3 pivot = vec3( 0.6f, 0.7f, 0.8f );
const int iterations = 22;

float deJeyko ( vec3 p ) {
  // vec3 n = vec3( 0.1f, 0.9f, 0.8f );
  // vec3 n = vec3( -0.33f, 1.3f, 0.8f );
  // vec3 n = vec3( -0.2f, 1.0f, 0.6f );
  // vec3 n = vec3( 0.6f, 0.7f, 0.8f );
  vec3 n = pivot;
  float d = 10e7;
  vec3 sz = vec3( 1.0, 0.5, 0.5 ) / 2.0;
  for( int i = 0; i < iterations; i++ ){
    // float b = sdBox( p, sz.xyx );
	// float b = deLipsoid( p, sz );
	// float b = distance( p, vec3( 0.0f ) ) - sz.x;
	float b = deRings( p / sz.x ) * sz.x;
	// float b = deStar( p / sz.x ) * sz.x;
    sz *= vec3( 0.74, 0.5, 0.74 );
    d = min( b, d );
    p = abs( p );
    p.xy *= rotGG( -0.9 + n.x );
    p.yz *= rotGG( 0.6 - n.y * 0.3 );
    p.xz *= rotGG( -0.2 + n.y * 0.1 );
    p.xy -= sz.xy * 2.0;
  }
  return d;
}

float deJeyko2 ( vec3 p ) {
  // vec3 n = vec3( 0.1f, 0.9f, 0.8f );
  // vec3 n = vec3( -0.33f, 1.3f, 0.8f );
  // vec3 n = vec3( -0.2f, 1.0f, 0.6f );
  // vec3 n = vec3( 0.6f, 0.7f, 0.8f );
  vec3 n = pivot;
  float d = 10e7;
  vec3 sz = vec3( 1.0, 0.5, 0.5 ) / 2.0;
  for( int i = 0; i < iterations; i++ ){
    // float b = sdBox( p, sz.xyx );
    // float b = sdBox2( p, sz.xyx );
	// float b = deLipsoid( p, sz );
	float b = deRings2( p / sz.x ) * sz.x;
	// float b = deRings( p / sz.x ) * sz.x;
    sz *= vec3( 0.74, 0.5, 0.74 );
    d = min( b, d );
    p = abs( p );
    p.xy *= rotGG( -0.9 + n.x );
    p.yz *= rotGG( 0.6 - n.y * 0.3 );
    p.xz *= rotGG( -0.2 + n.y * 0.1 );
    p.xy -= sz.xy * 2.0;
  }
  return d;
}

float deGateep(vec3 p, int iters){
	p=fract(p)-.5;
	vec3 O=vec3( 2.0f, 0.0f, 3.0f );
	// vec3 O=vec3( 2.5f, 0.0f, 3.0f );
	// vec3 O=vec3( 2.8f, 0.3f, 4.0f );
	for(int j=0;j++<iters;){
		p=abs(p);
		p=(p.x < p.y?p.zxy:p.zyx)*3.-O;
		if(p.z < -.5*O.z)
		p.z+=O.z;
	}
	return length(p.xy)/3e3;
}
float deGatee( vec3 p ) {
	float scale = 2.0f;
	return deGateep( p / scale, 7 ) * scale - 0.0006;
}
float deGatee2( vec3 p ) {
	float scale = 2.0f;
	return deGateep( p / scale, 6 ) * scale - 0.0006;
}
float deGatee3( vec3 p ) {
	float scale = 2.0f;
	return deGateep( p / scale, 5 ) * scale - 0.0006;
}

  #define fold45(p)(p.y>p.x)?p.yx:p
  float deGGED(vec3 p) {
    float scale = 2.1, off0 = .6, off1 = .3, off2 = .83;
    vec3 off =vec3(2.3,0.6,.1);
    float s=1.0;
    for(int i = 0;++i<20;) {
      p.xy = abs(p.xy);
      p.xy = fold45(p.xy);
      p.y -= off0;
      p.y = -abs(p.y);
      p.y += off0;
      p.x += off1;
      p.xz = fold45(p.xz);
      p.x -= off2;
      p.xz = fold45(p.xz);
      p.x += off1;
      p -= off;
      p *= scale;
      p += off;
      s *= scale;
    }
    return length(p)/s;
  }

#define TAUg atan(1.)*8.
vec2 pmodg(vec2 p, float n){
	float a=mod(atan(p.y, p.x),TAUg/n)-.5 *TAUg/n;
	return length(p)*vec2(sin(a),cos(a));
}

float detrimental( vec3 p ){
	for(int i=0;i<4;i++){
		p.xy = pmodg(p.xy,10.);  p.y-=2.;
		p.yz = pmodg(p.yz, 12.); p.z-=10.;
	}
	return dot(abs(p),normalize(vec3(13,1,7)))-.7;
}
// from loicvdb https://www.shadertoy.com/view/cdlGRl
float ot;
float dePortrait(vec3 c)
{
	vec3 z = c;
	float dr = 1.0;
	float r = length(z);
	ot = 1.0;

	for (int j = 0; j < 11 && r < 4.0; j++)
	{
		dr = dr * r * 2.0 + 1.0;
		float t = acos(z.z / r) * 2.0;
		float p = atan(z.y, z.x) * 2.0;
		z = r * r * vec3(sin(t) * vec2(cos(p), sin(p)), cos(t)) + c;
		r = length(z);
		ot = min(abs(z.z) * 9.0, ot);
	}

	return 0.5 * log(r) * r / dr;
}

float deHalls2(vec3 pos){
	vec3 tpos=pos;
	tpos.xz=abs(.5-mod(tpos.xz,1.));
	vec4 p=vec4(tpos,1.);
	float y=max(0.,.35-abs(pos.y-3.35))/.35;
	for (int i=0; i<7; i++) {
		p.xyz = abs(p.xyz)-vec3(-0.02,1.98,-0.02);
		p=p*(2.0+0.*y)/clamp(dot(p.xyz,p.xyz),.4,1.)-vec4(0.5,1.,0.4,0.);
		p.xz*=mat2(-0.416,-0.91,0.91,-0.416);
	}
	return (length(max(abs(p.xyz)-vec3(0.1,5.0,0.1),vec3(0.0)))-0.05)/p.w;
}

float deLaceHall( vec3 p ){
	float s=3.;
	for(int i = 0; i < 4; i++) {
		p=mod(p-1.,2.)-1.;
		float r=1.2/dot(p,p);
		p*=r; s*=r;
	}
	p = abs(p)-0.8;
	if (p.x < p.z) p.xz = p.zx;
	if (p.y < p.z) p.yz = p.zy;
	if (p.x < p.y) p.xy = p.yx;
	return length(cross(p,normalize(vec3(0,1,1))))/s-.001;
}

float deHalls(vec3 p){
	return min(.65-length(fract(p+.5)-.5),p.y+0.05f);
}


// below from https://www.shadertoy.com/view/WllfzB
float noisedean(vec3 p) {
	const vec3 s = vec3(7.0, 157.0, 113.0);
	vec3 ip = floor(p);
	vec4 h = vec4(0.0, s.yz, s.y + s.z) + dot(ip, s);
	p -= ip;

	h = mix(fract(sin(h) * 43758.5453), fract(sin(h + s.x) * 43758.5453), p.x);

	h.xy = mix(h.xz, h.yw, p.y);
	return mix(h.x, h.y, p.z);
}

vec2 opRep(vec2 p, vec2 c) {
	return mod(p + 0.5 * c, c) - 0.5 * c;
}

float opRep(inout float p, float size, float c1, float c2) {
	float w2 = size * 0.5;
	float pp = p - w2;
	float idx = clamp(floor(pp / size), c1, c2);
	p = pp - size * idx - w2;
	return fract(idx * 12532.56);
}

float opRep(inout vec2 p, vec2 size, vec2 c1, vec2 c2) {
	vec2 w2 = size * 0.5;
	vec2 pp = p - w2;
	vec2 idx = clamp(floor(pp / size), c1, c2);
	p = pp - size * idx - w2;
	return dot(fract(idx * 12532.56), vec2(421.0, 965.0));
}

vec4 min4(vec4 a, vec4 b) { return a.x < b.x ? a : b; }

/*
float sdBox(vec3 p, vec3 b) {
	vec3 q = abs(p) - b;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
*/

float sdCappedCylinder(vec3 p, float h, float r) {
	vec2 d = abs(vec2(length(p.xy), p.z)) - vec2(h, r);
	return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

vec3 getRayDir(vec3 ro, vec3 lookAt, vec2 uv) {
	vec3 forward = normalize(lookAt - ro);
	vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
	vec3 up = cross(forward, right);
	return normalize(forward + right * uv.x + up * uv.y);
}

// Call to create a concrete-like 3D texture.
//
// p:   [in]  point in 3D space.
// mat: [out] material rgb
// id:  [in]  arbitary ID used as a rnd seed.
// dist:[in]  distance from ray origin (Used to fade out surface displacement, avoiding rendering artifacts).
// returns: surface noise value to apply to a SDF function.
float getConcreteMaterial(vec3 p, out vec3 mat, float id, float dist) {
	// Set rnd seed from id.
	vec3 tp = p + vec3(0.32, 0.40, 1.2) * mod(id, 10.0);

	// Mix a couple of shades of grey.
	float baseColor = smoothstep(0.0, 0.5, noisedean(tp));
	mat = mix(vec3(0.18, 0.17, 0.17), vec3(0.20, 0.19, 0.19), baseColor);

	// Surface roughness.
	dist = 1.0 - smoothstep(0.0, 1.0, dist / 14.0);
	float rough = noisedean(tp * 60.0) * 0.005 // Base
	+ step(0.2, noisedean(tp * 26.666)) * 0.0033; // Pits/dents.
	mat += rough * 24.0;

	// Fade surface roughness(/deflection) out with distance to prevent screen noise.
	return rough * dist;
}

// A rectangular concrete 'wall panel', with a small hole in each corner.
//
// p: point in 3D space.
// r: dimensions (halved)
// id: arbitary ID used as a rnd seed.
// dist: distance from ray origin (Used to fade out surface displacement, avoiding rendering artifacts).
// returns (distance, material rgb)
vec4 sdConcretePanel(vec3 p, vec3 r, float id, float dist) {
	// Tilt each panel a small amount.
	mat2 tilt = rot((fract(sin(id) * 43758.5453) - 0.5) * 0.03);
	p.xz *= tilt;
	p.yz *= tilt;

	float d = sdBox(p, r - 0.04) - 0.04;

	// Mounting holes.
	vec3 pp = p;
	pp.xy = abs(p.xy) - r.xy * 0.8;
	float hole = sdCappedCylinder(pp + vec3(0.0, 0.0, r.z * 1.5), r.x * 0.03, r.z * 1.0);
	d = max(d, -hole);

	// Apply material.
	vec3 mat;
	d -= getConcreteMaterial(p, mat, id, dist);
	return vec4(d, mat);
}

// Concrete paving slab.
//
// p: point in 3D space.
// r: dimensions (halved)
// id: arbitary ID used as a rnd seed.
// dist: distance from ray origin (Used to fade out surface displacement, avoiding rendering artifacts).
// returns (distance, material rgb)
vec4 sdSlab(vec3 p, vec3 r, float id, float dist) {
	// Tilt each slab a small amount.
	mat2 tilt = rot((fract(sin(id) * 43758.5453) - 0.5) * 0.1);
	p.xz *= tilt;
	p.yz *= tilt;

	float d = sdBox(p, r - 0.03) - 0.03;

	// Apply material.
	vec3 mat;
	d -= getConcreteMaterial(p, mat, id, dist);
	return vec4(d, mat);
}

// Map the scene using SDF functions.
vec4 concretemap(vec3 p) {
	vec3 r = vec3(1.0, 2.0, 0.05);

	vec4 d = vec4(1e10);

	float dist = 1.0f;

	// Left wall.
	vec3 pp = p;
	pp.xz *= rot(3.141 / 2.0);
	pp.z -= 5.0;
	float id = opRep(pp.xy, r.xy * 2.0, vec2(-15.0, -2.0), vec2(10.0, 1.0));
	d = min4(d, sdConcretePanel(pp, r, id, dist));

	// Far wall.
	pp = p;
	id = opRep(pp.xy, r.xy * 2.0, vec2(-5.0, -2.0), vec2(5.0, 1.0));
	d = min4(d, sdConcretePanel(pp - vec3(0.0, 0.0, 33.0), r, id, dist));

	// Columns.
	pp = p - vec3(8.0, 0.0, 0.0);
	id = opRep(pp.z, 5.0, -4.0, 3.0);
	d = min4(d, sdSlab(pp, vec3(2.0, 8.0, 1.0), 1.0, dist));

	// Floor.
	pp = p;
	pp.yz *= rot(3.141 / 2.0);
	pp.z -= 5.0;
	r = vec3(1.0, 1.0, 0.2);
	id = opRep(pp.xy, r.xy * 2.0, vec2(-15.0, -15.0), vec2(16.0, 16.0));
	d = min4(d, sdSlab(pp, r, id, dist));

	// Ceiling.
	d = min4(d, sdSlab(p - vec3(1.0, 10.0, 0.0), vec3(10.0, 3.0, 50.0), 1.0, dist));

	return d;
}


// below from https://www.shadertoy.com/view/ltSfRW
vec2 hash2( vec2 p )
{
	// texture based white noise
	//return textureLod( iChannel0, (p+0.5)/256.0, 0.0 ).xy;

	// procedural white noise
	return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}

// from https://www.shadertoy.com/view/ldl3W8 (bit modified to return desired values)
// returns,
//   x: distance to nearest neighbor
//   y: cell id
//   z: distance to border

vec3 voronoi( in vec2 x )
{
	vec2 n = floor(x);
	vec2 f = fract(x);

	//----------------------------------
	// first pass: regular voronoi
	//----------------------------------
	vec2 mg, mr;
	float id;

	float md = 8.0;
	for( int j=-1; j<=1; j++ )
	for( int i=-1; i<=1; i++ )
	{
		vec2 g = vec2(float(i),float(j));
		vec2 o = hash2( n + g );
		vec2 ao = o;
		vec2 r = g + ao - f;
		float d = dot(r,r);

		if( d<md )
		{
			md = d;
			mr = r;
			mg = g;
			id = o.x+o.y;
		}
	}

	//----------------------------------
	// second pass: distance to borders
	//----------------------------------
	md = 8.0;
	for( int j=-2; j<=2; j++ )
	for( int i=-2; i<=2; i++ )
	{
		vec2 g = mg + vec2(float(i),float(j));
		vec2 o = hash2( n + g );
		vec2 r = g + o - f;

		if( dot(mr-r,mr-r)>0.00001 )
		md = min( md, dot( 0.5*(mr+r), normalize(r-mr) ) );
	}

	return vec3( length(mr), id, md );
}


// textures for the material definition
uniform sampler2D diffuseMaterial;
uniform sampler2D displacementMaterial;
uniform sampler2D roughnessMaterial;

float deWHIRP ( vec3 p ) {
	p = p.xzy;
	vec3 cSize = vec3(1., 1., 1.3);
	float scale = 1.;
	for( int i=0; i < 12; i++ ){
		p = 2.0*clamp(p, -cSize, cSize) - p;
		float r2 = dot(p,p);
		float k = max((2.)/(r2), .027);
		p *= k; scale *= k;
	}
	float l = length(p.xy);
	float rxy = l - 4.0;
	float n = l * p.z;
	rxy = max(rxy, -(n) / 4.);
	return (rxy) / abs(scale);
}


// https://www.shadertoy.com/view/tcjfRz
float fractalD(vec3 p) {
	float s,w,l;
	float a = 0.5f * dot(sin( p * 4.0f ), vec3( 0.1f ) );
	p *= vec3( 0.6f, 0.6f, 0.5f );
	p.xy -= 1.5f;
	p += sin( p + p.zxy * 2.0f ) * 0.13f + cos( p.yzx * 3.0f ) * 0.28f;
	for ( s = 0.0f, w = 0.6f; s++ < 8.0f; p *= l, w *= l )
		p  = abs( sin( p ) ) - 1.0f,
		l = exp( a ) / dot( p, p ) - a;
	return length( p ) / w;
}




float deCage(vec3 p){
	return (length(vec2((length(vec2(length(p.xy)-1.3,
	length(p.zy)-1.3))-.5), dot(cos(p*12.),sin(p.zxy*12.))*.1))-.02)*.3;
}

float deBasket(vec3 p){
	float i,g,e,s,q;
	q=length(p)-1.;
	p.y++;
	s=3.;
	for(int i=0;i++<7;p=vec3(0,5,0)-abs(abs(p)*e-3.))
	s*=e=max(1.,14./dot(p,p));
	return max(q,min(1.,length(p.xz)-.3))/s;
}
float deBBB( vec3 p0 ){
	vec4 p = vec4(p0, 1.);
	for(int i = 0; i < 8; i++){
		if(p.x > p.z)p.xz = p.zx;
		if(p.z > p.y)p.zy = p.yz;
		p = abs(p);
		p.xyz = mod(p.xyz-1., 2.)-1.;
		p*=1.23;
	}
	p/=p.w;
	return abs(p.y)*0.25;
}

float deGGG ( vec3 p ) {
	p = mod( p, 2.0f ) - 1.0f;
	p = abs( p ) - 1.0f;
	if ( p.x < p.z ) p.xz = p.zx;
	if ( p.y < p.z ) p.yz = p.zy;
	if ( p.x < p.y ) p.xy = p.yx;
	float s = 1.0f;
	for ( int i = 0; i < 10; i++ ) {
		float r2 = 2.0f / clamp( dot( p, p ), 0.1f, 1.0f );
		p = abs( p ) * r2 - vec3( 0.6f, 0.6f, 3.5f );
		s *= r2;
	}
	return length( p ) / s;
}

//=============================================================================================================================
#include "oldTestChamber.h.glsl"
#include "pbrConstants.glsl"
//=============================================================================================================================
uniform mat4 transform_imguizmo;
float de( in vec3 p ) {
	const vec3 pOriginal = p;
	float sceneDist = 1000.0f;
	hitColor = vec3( 0.0f );
	hitSurfaceType = NOHIT;
	hitRoughness = 0.0f;

	// form for the following:
	// {
		// const float d = blah whatever SDF
		// sceneDist = min( sceneDist, d );
		// if ( sceneDist == d && d < epsilon ) {
			// set material specifics - hitColor, hitSurfaceType, hitRoughness
		// }
	// }

	const vec3 bboxDim = vec3( 25.0f, 20.0f, 20.0f );
//	const float dBounds = distance( p, vec3( 0.0f ) ) - marbleRadius - 0.001f;
	 const float dBounds = sdBox( p, bboxDim );
	// const float dBounds = sdBox( p, vec3( marbleRadius ) );
	p = ( transform_imguizmo * vec4( p, 1.0f ) ).xyz;
	const vec3 displacement = sqrt( matWood( p * 1.8f ) );
	const vec3 displacement2 = matWood( p * 3.8f );
	const float noise1 = perlinfbm( p, 10.0f, 5 ) - 0.3f;
	const float noise2 = sqrt( saturate( perlinfbm( p, 1.5f, 5 ) + 0.4f ) );

	if ( false ) {
		const float scale = 66.4f;
		p = p / scale;
		const vec2 uv = 3.1f * rot( 0.3f ) * ( p.xz + vec2( 0.5f ) );
		const float d = max( ( p.y - texture( displacementMaterial, uv ).r * 0.05f ) * scale, dBounds );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			float rough =  texture( roughnessMaterial, uv ).r;
			hitSurfaceType = NormalizedRandomFloat() > pow( rough, 0.5f ) ? MIRROR : DIFFUSE;
			hitRoughness = rough;
			hitColor = texture( diffuseMaterial, uv ).rgb;
			//			hitColor.rg = p.xy;
		}
	}

	if ( false ) {
		p = pOriginal.xzy;
		const float scale = 20.0f;
		const float d = max( deWHIRP( p / scale ) * scale, dBounds );

		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = NormalizedRandomFloat() > 0.1f ? METALLIC : MIRROR;
			 hitColor = hitSurfaceType == MIRROR ? vec3( 0.99f ) : vec3( 0.01f );
			hitRoughness = 0.2f;
		}

	}

	if ( false ) {

		// apply transform
//		p = ( transform_imguizmo * vec4( p, 1.0f ) ).xyz;

		const float scale = 10.5f;

		const float dBase = dePortrait( p / scale ) * scale;
		const float d = max( max( dBase + GetLuma( displacement2 ).r * 0.04f * ( 1.0f - ot ), dBounds ), dBounds );
//		const float d = max( max( deBasket( p / scale ) * scale - GetLuma( displacement2 ).r * 0.02f, dBounds ), dBounds );

		// const float d = deJeyko( p );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {

			// float noiseValue = ( snoise3D( p * 6.18f ) + 1.0f ) / 2.0f;
			// hitColor = mix( tire, nvidia, 0.1f );
			// hitColor = vec3( 0.99f );
			// hitColor = tire * 3.0f;
			// hitColor = mix( sapphire, nickel, noiseValue );
			// hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
			// hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : METALLIC;
			// hitRoughness = noiseValue;
			// hitSurfaceType = LUMARBLECHECKER;
			
			/*
			hitColor = mix( gold, vec3( 0.99f ), -0.5f );
			hitRoughness = 0.1f;
			hitSurfaceType = METALLIC;
			*/

			// hitSurfaceType = MIRROR;

			// hitColor = mix( mix( gold, vec3( 0.99f ), -0.3f ), blood / 5.0f, ot );

			if ( NormalizedRandomFloat() < ot ) {
				hitRoughness = 0.3f;
				hitSurfaceType = NormalizedRandomFloat() < 0.9f ? METALLIC : MIRROR;
				// hitColor = ( hitSurfaceType == MIRROR ) ? mix( vec3( 0.99f ), honey, NormalizedRandomFloat() ) : ( mix( vec3( 0.0f ), blood * displacement2.r, displacement.r ) * 3.0f * displacement.r );
				hitColor = ( hitSurfaceType == MIRROR ) ? mix( vec3( 0.99f ), honey, NormalizedRandomFloat() ) : ( vec3( 0.01f ) );
			} else {
				hitSurfaceType = MIRROR;
				// hitColor = vec3( 0.99f );
				hitColor = displacement2.rrr * blood;
			}


//			ot = 1.0f - ot;
			// hitColor = mix( mix( nvidia, vec3( 0.1f, 0.3f, 0.04f ), perlinfbm( p, 3.0f, 5 ) / 2.0f + 0.5f ), nickel, ot );
			// hitColor = mix( mix( vec3( 0.0f, 0.15f, 0.0f ), nvidia / 4.0f, perlinfbm( p, 50.0f, 5 ) ), verdigris, ot ).ggr;
			// hitColor = mix( gold, blood, ot );
			// hitColor = mix( verdigris, nickel, saturate( ot * 1.3f - 0.3f ) );
			// hitColor = ot < 0.618f ? verdigris : nickel;
			// hitColor = mix( vec3( 1.0f, 1.0f, 0.9f ), vec3( 0.7f, 0.2f, 0.3f ), ot );
			// hitColor = mix( gold, vec3( 0.99f ), -0.3f ); // hypergold
			// hitColor = nvidia * GetLuma( displacement2 );
			// hitSurfaceType = NormalizedRandomFloat() < ( ot ) ? MIRROR : ( NormalizedRandomFloat() > ot ) ? DIFFUSE : METALLIC;
			// hitSurfaceType = NormalizedRandomFloat() < ( ot + perlinfbm( p, 150.0f, 5 ) * 0.5f ) ? METALLIC : DIFFUSE;
//			hitSurfaceType = NormalizedRandomFloat() > ( noise1 + noise2 * displacement2.r ) ? METALLIC : MIRROR;
//			if ( hitSurfaceType == METALLIC ) {
//				hitColor = 2.0f * displacement.rrr;
//				if ( GetLuma( displacement2 ).r > 0.35f )
//					hitColor = vec3( gold ), hitSurfaceType = MIRROR;
				// hitColor = nickel;
				// hitColor = mix( carrot, GetLuma( displacement2 ), ot );
				// hitColor = mix( vec3( 0.0f ), mix( vec3( 1.0f ), carrot, GetLuma( displacement ) * 2.0f ), pow( ot, 1.5f ) );
//			}
//			hitSurfaceType = NormalizedRandomFloat() < ( ot ) ? WOOD : DIFFUSE;


			// hitColor = vec3( 0.99f );
			// hitColor = mix( gold, vec3( 0.99f ), -0.3f ); // hypergold
			// hitColor = vec3( ( hitSurfaceType != MIRROR ) ? gold : vec3( 0.99f ) );
			// hitColor = vec3( ( hitSurfaceType != MIRROR ) ? ( vec3( blood.grb / 3.0f ) ) : vec3( 0.9f ) );
			// hitRoughness = 0.1f;



			// hitColor = 0.3f * nvidia;
			// hitColor = mix( vec3( 0.25f, 0.0f, 0.0f ), nvidia, 0.75f );
			// hitColor = vec3( 0.25f, 0.0f, 0.0f );
			// hitColor = mix( nickel, vec3( 1.0f ), -0.3f );

		}
	}

	p = pOriginal;

	if ( false ) {
		// const float d = max( max( deGatee2( p ), dBounds ), dBounds );
		const float scale = 14.8f;
		const float d = max( max( deCage( p / scale ) * scale - displacement.r * 0.4f * displacement2.r - 0.1f * GetLuma( displacement2 ).r, dBounds ), dBounds );
//		const float d = max( max( deLaceHall( p / scale ) * scale, dBounds ), dBounds );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {

			hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
			hitColor = ( hitSurfaceType == DIFFUSE ) ? vec3( nvidia * displacement2.r ) : vec3( 0.99f );

//			hitSurfaceType = MIRROR;
//			hitColor = blood * 0.1f;

			// hitColor = 0.3f * nvidia * displacement.rrr;
			// hitColor = displacement.rrr * nvidia;

//			hitColor = mix( nvidia, blood, displacement.r * 3.0f ) * 0.2f;
//			hitColor = displacement.r < 0.01f ? honey : vec3( 0.99f );

			// hitColor = mix( nickel, brass, 0.2f );
			// hitColor = brass;


			/*
			const float noiseValue = saturate( pow( perlinfbm( p, 30.0f, 4 ), 4 ) * 2.0f );
			if ( noiseValue > 0.05f ) {
				const float remappedN = RangeRemapValue( noiseValue, 0.05f, 0.1f, 0.0f, 1.0f );
				hitColor = mix( brick, vec3( iron ), saturate( 1.0f - remappedN ) );
				hitSurfaceType = ( NormalizedRandomFloat() < remappedN ) ? DIFFUSE : METALLIC;
				hitRoughness = remappedN;
			}
			*/
		}
	}

	if ( false ) {
		// const vec4 d = concretemap( p ) + vec4( 0.05f * GetLuma( displacement2 ).rrrr );
		const vec4 d = vec4( deCage( p ) );
//		const vec4 d = concretemap( p ) + vec4( displacement2.x * 0.05f, 0.0f, 0.0f, 0.0f );
		sceneDist = min( sceneDist, d.x );
		if ( sceneDist == d.x && d.x < epsilon ) {
			hitSurfaceType = NormalizedRandomFloat() < 0.9f ? METALLIC : MIRROR;
			hitRoughness = 0.3f;
//			 hitColor = hitSurfaceType == MIRROR ? vec3( 0.99f ) : mix( d.yzw, nickel, displacement2.rgb );
			hitColor = hitSurfaceType == MIRROR ? vec3( 0.99f ) : mix( vec3( 1.0f ), nvidia, saturate( GetLuma( displacement2.xyz ).r + 0.1f ) );
		}
	}

	p *= Rotate3D( 0.3f, normalize( vec3( 1.0f ) ) );
	if ( true ) {
		vec3 pCache = p;
		float idx = pModInterval1( p.x, 0.3f, 0.0f, 7.0f );
		// const float d = fBox( p - vec3( 0.0f, 0.0f, -5.0f ), vec3( 0.8f, 0.1f, 30.0f ) );
		const float d = fBox( p, vec3( 0.01618f, 0.01f, 5.0f ) );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = EMISSIVE_FRESNEL;
			hitColor = vec3( 0.1618f * sapphire );
//			hitColor = blood;
		}
		p = pCache;
	}

	if ( false ) {
		const float d = fBox( p - vec3( 0.0f, 20.0f, 0.0f ), vec3( 30.0f, 0.1f, 6.0f ) );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = EMISSIVE_FRESNEL;
			// hitColor = vec3( 1.0f ) * carrot;
			hitColor = vec3( 1.0f ) * mix( blood, aqua, displacement.xyz ) * displacement2.xyz;
		}
	}

	p *= Rotate3D( 0.3f, normalize( vec3( 1.0f, 0.1, 0.2f ) ) );
	if ( true ) {
		float idx = pModInterval1( p.x, 0.3f, 0.0f, 7.0f );
		const float d = fBox( p - vec3( 0.0f, 6.5f, 0.0f ), vec3( 6.9f, 0.05f, 0.01f ).yxz );

	 	// const float d = deJeyko( p );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = EMISSIVE_FRESNEL;
			hitColor = vec3( oklab_mix( blood, nvidia, pow( idx / 7.0f, 2.0f ) ) );

//			vec3 c0 = voronoi( pOriginal.xz * 5.0f );
//			vec3 c1 = voronoi( pOriginal.yz * 5.0f );
//			vec3 c;
//			if ( c0.z > 0.1f )
//				c = c0;
//			else
//				c = c1;
//			hitColor = pow( saturate( smoothstep( c.z, 0.0f, 0.1f ) ), 0.25f ) * ( 1.8f * saturate( 0.5f + 0.5f * cos( c.y * 6.2831f + vec3( 0.0f, 1.0f, 2.0f ) ) ) );
//			hitColor = ( blood + honey ) * hitColor * smoothstep( c1.z, 0.0f, 0.01f );
//
//			if ( c == c1 ) {
//				hitSurfaceType = NormalizedRandomFloat() < 0.9f ? METALLIC : MIRROR;
//				hitRoughness = displacement.r;
//				hitColor = hitSurfaceType == MIRROR ? vec3( 0.99f ) : vec3( smoothstep( c1.z, 0.0f, 0.01f ) * 0.01f );
//			}

//			 hitColor = 3.0f * ( mod( i, 2 ) == 0 ? vec3( 0.99f ) : mix( blood, honey, 0.3f ) );
//			hitColor = 2.0f * sapphire;
	 	}
	 }

	p = pOriginal;
	if ( true ) {
		const float scale = 2.0f;
		const float d = max( dBounds, deGGG( p / scale ) * scale );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = NormalizedRandomFloat() < 0.9f ? METALLIC : MIRROR;
			hitRoughness = 0.1f;
			hitColor = vec3( hitSurfaceType == MIRROR ? 0.99f : 0.4f );
		}
	}

	return sceneDist;

	// pModInterval1( p.x, 1.0f, -5.0f, 5.0f );
	// pModInterval1( p.y, 4.0f, -2.0f, 2.0f );

	// {
	// 	// const float d = max( deRingo( p ), distance( p, vec3( 0.0f ) ) - marbleRadius - 0.001f );
	// 	// const float d = deFFFF( p );
	// 	const float d = max( deASDF( p ), fBox( p, vec3( 3.0f ) ) );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		// hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 		hitSurfaceType = METALLIC;
	// 		// hitColor = vec3( 0.713f, 0.170f, 0.026f ); // carrot
	// 		// hitColor = vec3( 0.831f, 0.397f, 0.038f ); // honey
	// 		// hitColor = vec3( 0.887f, 0.789f, 0.434f ); // bone
	// 		// hitColor = vec3( 0.023f, 0.023f, 0.023f ); // tire
	// 		// hitColor = vec3( 0.670f, 0.764f, 0.855f ); // sapphire blue
	// 		hitColor = vec3( 0.649f, 0.610f, 0.541f ); // nickel
	// 		hitRoughness = 0.3f;
	// 	}
	// }

	// {
	// 	const float d = deOldTestChamber( p );
	// 	sceneDist = min( sceneDist, d );
	// }

	// p.y = -p.y;

	// {
	// // 	// const float d = max( mapPl( p - vec3( -5.0f, -5.0f, 5.0f ), 0.0f ), fBox( p - vec3( -18.0f, -3.0f, 0.0f ), vec3( 8.0f, 3.0f, 10.5f ) ) );

	// // 	// pModPolar( p.xy, 7.0f );

	// // 	// ivec3 idx = ivec3( pMod3( p, vec3( 1.0f ) ) );
	// // 	// uint seedCache = seed;
	// // 	// seed = idx.x * 1000 + idx.y * 2040 + idx.z * 3002;

	// // 	// seed = seedCache;

	// // 	// const float d = max( dHead( p * scale ) / scale, fBox( pOriginal, vec3( 8.0f, 3.0f, 10.5f ) ) );

	// // 	uint seedCache = seed;
	// // 	int xIdx = int( pModInterval1( p.x, 1.2f, -10.0f, 10.0f ) );
	// // 	int yIdx = int( pModInterval1( p.y, 1.2f, -10.0f, 10.0f ) );
	// // 	seed = xIdx * 1000 + yIdx * 2040;
	// // 	vec3 col = viridis( RangeRemapValue( NormalizedRandomFloat(), 0.0f, 1.0f, 0.5f, 0.7f ) );
	// // 	pR( p.xy, 35.0f * NormalizedRandomFloat() );
	// // 	seed = seedCache;

	// // 	float scale = 1.5f;
	// // 	p *= scale;
	// // 	float d = fOpUnionSoft( dHead( p ) / scale, fPlane( pOriginal, vec3( 0.0f, 0.0f, 1.0f ), 0.0f ), 0.2f );

	// // 	bool isEye1 = isEye;

	// // 	// second octave
	// // 	p = pOriginal;
	// // 	seedCache = seed;
	// // 	xIdx = int( pModInterval1( p.x, 2.6f, -10.0f, 10.0f ) );
	// // 	yIdx = int( pModInterval1( p.y, 2.1f, -10.0f, 10.0f ) );
	// // 	seed = xIdx * 1000 + yIdx * 2040;
	// // 	vec3 col2 = viridis( RangeRemapValue( NormalizedRandomFloat(), 0.0f, 1.0f, 0.5f, 0.7f ) );
	// // 	pR( p.xy, 35.0f * NormalizedRandomFloat() );
	// // 	seed = seedCache;
	// // 	scale = 0.75f;

	// // 	d = fOpUnionSoft( d, fOpUnionSoft( dHead( p ) / scale, fPlane( pOriginal, vec3( 0.0f, 0.0f, 1.0f ), 0.0f ), 0.2f ), 0.1f );

	// 	const float scale = 0.1f;
	// 	const float d = dHead( p * scale ) / scale;
	// 	sceneDist = min( sceneDist, d );

	// 	if ( sceneDist == d && d < epsilon ) {
	// 		// hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 		if ( isEye ) {
	// 			hitSurfaceType = EMISSIVE;
	// 			// hitColor = vec3( 0.670f, 0.764f, 0.855f ); // sapphire blue
	// 			hitColor = vec3( 3.0f );
	// 			// hitColor = col;
	// 		} else {
	// 			// hitSurfaceType = LUMARBLECHECKER;
	// 			hitSurfaceType = DIFFUSE;
	// 			// hitSurfaceType = METALLIC;
	// 			// hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 			// hitSurfaceType = EMISSIVE;
	// 			// hitColor = vec3( 0.887f, 0.789f, 0.434f ).rrg; // bone
	// 			// hitRoughness = 0.6f;
	// 			// hitColor = vec3( 0.3f );
	// 			hitColor = vec3( 0.99f );
	// 		}
	// 	}
	// }

	// {
	// 	p = pOriginal;
	// 	p.y = -p.y;

	// 	// ivec3 idx = ivec3( pMod3( p, vec3( 5.0f ) ) );

	// 	const float scale = 0.1f;
	// 	const float d = max( DEnew( p * scale ) / scale, fBox( pOriginal, vec3( 10.0f ) ) );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		// hitSurfaceType = DIFFUSE;
	// 		hitSurfaceType = MIRROR;
	// 		hitColor = vec3( 0.618f );
	// 	}
	// }

	// {
	// 	p = pOriginal;
	// 	// const float d = max( deWater( p ), fBox( p - vec3( 0.0f, -3.0f, 0.0f ), vec3( 8.0f, 3.0f, 10.5f ) ) );
	// 	const float scale = 16.0f;
	// 	const float d = max( max( deJAIL( p * scale ) / scale, deSDFSDF( p ) ), fBox( p, vec3( 10.0f ) ) );
	// 	// const float d = fCylinder( p  - vec3( 1.3f, 0.0f, 0.0f ), 0.05f, 3.0f );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		// hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 		// hitSurfaceType = EMISSIVE;
	// 		// hitSurfaceType = DIFFUSE;
	// 		// hitSurfaceType = MIRROR;
	// 		hitSurfaceType = METALLIC;
	// 		hitRoughness = 0.2f;
	// 		hitColor = vec3( 0.618f );
	// 	}
	// }





	// {
	// 	// const float d = max( deFEFE( p ), distance( p, vec3( 0.0f ) ) - 20.0f );
	// 	const float d = max( deFEFE( p ), fBox( p - vec3( 0.0f, -3.0f, 0.0f ), vec3( 8.0f, 3.0f, 10.5f ) ) );
	// 	// const float d = deRode( p );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		// hitSurfaceType = DIFFUSE;
	// 		hitColor = vec3( 0.618f );
	// 		hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 		// hitColor = vec3( 0.887f, 0.789f, 0.434f ); // bone
	// 	}
	// }

	// // pModPolar( p.xy, 7.0f );
	// {
	// 	// const float d = max( deGazTemple8( p ), distance( p, vec3( 0.0f ) ) - marbleRadius - 0.4f );
	// 	// const float d = max( deJuly( p ), fBox( p, vec3( 1.0f, 1.0f, 1.5f ) ) );
	// 	const float d = deJuly( p / 3.0f ) * 3.0f;
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		if ( abs( escape ) < 5.1f ) {
	// 			hitSurfaceType = EMISSIVE;
	// 			if ( abs( escape ) < 5.0f ) {
	// 				hitColor = vec3( 0.713f, 0.170f, 0.026f ); // carrot
	// 			} else {
	// 				hitColor = vec3( 0.831f, 0.397f, 0.038f ); // honey
	// 			}
	// 			// hitColor = viridis( RangeRemapValue( abs( escape ), 4.5f, 5.2f, 0.0f, 1.0f ) );
	// 		} else {
	// 			// hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 			hitSurfaceType = METALLIC;
	// 			hitRoughness = 0.1f;
	// 			hitColor = vec3( 0.618f );
	// 		}
	// 	}
	// }

	return sceneDist;
}
//=============================================================================================================================
vec3 SDFNormal( in vec3 position ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );

// another formulation, source unknown
//	vec3 calcNormal( in vec3 pos ){
//		const float h = 0.0001;
//		vec3 n = vec3(0.0);
//		for( int i=ZERO; i<4; i++ )
//		{
//			vec3 e = 0.5773*(2.0*vec3((((i+3)>>1)&1),((i>>1)&1),(i&1))-1.0);
//			n += e*scene(pos+e*h).dist;
//		}
//		return normalize(n);
//	}
}
//=============================================================================================================================
intersection_t raymarch( in ray_t ray ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	vec3 pQuery = ray.origin;
	for ( int steps = 0; steps < raymarchMaxSteps; steps++ ) {
		pQuery = ray.origin + dTotal * ray.direction;
		dQuery = de( pQuery );
		dTotal += dQuery * raymarchUnderstep;
		if ( dTotal > raymarchMaxDistance || abs( dQuery ) < epsilon ) {
			break;
		}
	}
	// fill out the intersection struct
	intersection_t intersection;
	intersection.dTravel = ( dTotal > raymarchMaxDistance ) ? maxDistance : dTotal; // mitigating issues with expensive raymarch
	intersection.albedo = hitColor;
	intersection.materialID = hitSurfaceType;
	intersection.roughness = hitRoughness;
	intersection.normal = SDFNormal( ray.origin + dTotal * ray.direction );
	intersection.frontfaceHit = ( dot( intersection.normal, ray.direction ) <= 0.0f );
	return intersection;
}

//=============================================================================================================================
// plane-based convex polyhedra intersector, from Eric Haines' "Fast Ray-Convex Polyhedron Intersection"
intersection_t convexPolyhedraIntersect ( in vec3 rayOrigin, in vec3 rayDirection, uint polyhedraSeed, float tMax ) {
	// the rng is deterministic, so we evaluate these in order and get the same results, for a given seed
	uint rngSeedCache = seed;
	seed = polyhedraSeed;

	float tNear = -1e30f;
	float tFar = tMax;

	vec3 frontNormalCantidate;
	vec3 backNormalCantidate;

	for ( int i = 0; i < 6; i++ ) {
		// deterministic rng for generating normals
		vec3 planeNormal = RandomUnitVector();
		float planeOffset = -1.5f + NormalizedRandomFloat();

		// precompute terms
		float vd = rayDirection.x * planeNormal.x + rayDirection.y * planeNormal.y + rayDirection.z * planeNormal.z;
		float vn = rayOrigin.x * planeNormal.x + rayOrigin.y * planeNormal.y + rayOrigin.z * planeNormal.z + planeOffset;

		if ( vd == 0.0f ) { // parallel ray
			if ( vn > 0.0f ) { // half space partition, ray is outside
				return DefaultIntersection();
			}
		} else {
			float t = -vn / vd;
			if ( vd < 0.0f ) { // we're considering a frontface hit
				if ( t > tFar ) { // spatial partitioning has collapsed
					return DefaultIntersection();
				}
				if ( t > tNear ) { // we are considering a new nearest hit ( frontface )
					frontNormalCantidate = planeNormal;
					tNear = t;
				}
			} else { // we're considering a backface hit
				if ( t < tNear ) { // backface closer than nearest collapses partition
					return DefaultIntersection();
				}
				if ( t < tFar ) { // we're considering a new nearest hit ( backface )
					backNormalCantidate = planeNormal;
					tFar = t;
				}
			}
		}
	}

	// restore rng state
	seed = rngSeedCache;

	// evaluate results
	intersection_t result = DefaultIntersection();

	// result.albedo = vec3( 1.0f, 0.0f, 0.0f );
	result.albedo = vec3( 0.99f );
	result.IoR = 1.0f / 1.6f;
	// result.IoR = 1.6f;
	result.roughness = 0.0f;

	if ( tNear >= 0.0f ) { // outside, hitting front face
		result.dTravel = tNear;
		result.normal = frontNormalCantidate;
		result.frontfaceHit = true;
		// result.materialID = REFRACTIVE;
		result.materialID = MIRROR;
		return result;
	} else {
		if ( tFar < tMax ) { // inside, hitting back face
			result.dTravel = tFar;
			result.normal = -backNormalCantidate;
			result.frontfaceHit = true;
			// result.materialID = REFRACTIVE_BACKFACE;
			result.materialID = MIRROR;
			return result;
		} else { // inside but far face beyond tMax, miss
			return DefaultIntersection();
		}
	}
}


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

iqIntersect IntersectSphere ( in ray_t ray, in vec3 center, float radius ) {
	// https://iquilezles.org/articles/intersectors/
	vec3 oc = ray.origin - center;
	float b = dot( oc, ray.direction );
	float c = dot( oc, oc ) - radius * radius;
	float h = b * b - c;
	if ( h < 0.0f ) return kEmpty; // no intersection
	h = sqrt( h );
	// h is known to be positive at this point, b+h > b-h
	float nearHit = -b - h; vec3 nearNormal = normalize( ( ray.origin + ray.direction * nearHit ) - center );
	float farHit  = -b + h; vec3 farNormal  = normalize( ( ray.origin + ray.direction * farHit ) - center );
	return iqIntersect( vec4( nearHit, nearNormal ), vec4( farHit, farNormal ) );
}

iqIntersect IntersectBox( in ray_t ray, in vec3 center, in vec3 size ) {
	vec3 oc = ray.origin - center;
	vec3 m = 1.0f / ray.direction;
	vec3 k = vec3( ray.direction.x >= 0.0f ? size.x : -size.x, ray.direction.y >= 0.0f ? size.y : -size.y, ray.direction.z >= 0.0f ? size.z : -size.z );
	vec3 t1 = ( -oc - k ) * m;
	vec3 t2 = ( -oc + k ) * m;
	float tN = max( max( t1.x, t1.y ), t1.z );
	float tF = min( min( t2.x, t2.y ), t2.z );
	if ( tN > tF || tF < 0.0f ) return kEmpty;
	return iqIntersect(
		vec4( tN, normalize( -sign( ray.direction ) * step( vec3( tN ), t1 ) ) ),
		vec4( tF, normalize( -sign( ray.direction ) * step( t2, vec3( tF ) ) ) ) );
}

//=============================================================================================================================
uniform bool ddaSpheresEnable;
uniform vec3 ddaSpheresBoundSize;
uniform int ddaSpheresResolution;
layout( rgba8ui ) readonly uniform uimage3D DDATex;
layout( r32f ) readonly uniform image2D HeightmapTex;
//=============================================================================================================================
vec3 GetPositionForIdx( ivec3 idx ) {
	return ( vec3( idx ) - vec3( ddaSpheresResolution / 2.0f ) ) * 0.009f + vec3( 6.5f );
}

float ddaDiskSDF( vec3 p ) {
	// return deBB( p );
	return deLeaf( p );
	// return max( deFractal2( p ), ( distance( p, vec3( 0.0f ) ) - ( ddaSpheresResolution / 200.0f ) ) );
}

bool CheckValidityOfIdx( ivec3 idx ) {
	// return true;

	// return snoise3D( idx * 0.01f ) < 0.0f;

	// bool mask = deStairs( idx * 0.01f - vec3( 2.9f ) ) < 0.001f;
	// bool mask = deWater( idx * 0.02f - vec3( 3.5f ) ) < 0.01f;
	// bool mask = ddaDiskSDF( GetPositionForIdx( idx ) ) < 0.01f;

	bool mask = ( imageLoad( DDATex, idx ).a != 0 );

	// const float heightSample = imageLoad( HeightmapTex, idx.xy ).r * 512;
	// const float scaledZAxis = float( idx.z );
	// bool mask = heightSample < scaledZAxis;

	// bool blackOrWhite = ( step( -0.0f,
	// 	cos( PI * 0.01f * float( idx.x ) + PI / 2.0f ) *
	// 	cos( PI * 0.01f * float( idx.y ) + PI / 2.0f ) *
	// 	cos( PI * 0.01f * float( idx.z ) + PI / 2.0f ) ) == 0 );

	// return true;
	return mask;
	// return blackOrWhite && mask;
	// return blackOrWhite;
}
//=============================================================================================================================
vec3 GetOffsetForIdx( ivec3 idx ) {
	return vec3( 0.5f );

	// jitter... not liking the look of this, much
	// return vec3( 0.26f ) + 0.45f * ( pcg3d( uvec3( idx ) ) / 4294967296.0f );
	// return vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() );
}
//=============================================================================================================================
float GetRadiusForIdx( ivec3 idx ) {
	return 0.47f;
	// return 0.37f;
	// return 0.4f;
	// return 0.2f;

	// return 0.45f * ( pcg3d( uvec3( idx ) ) / 4294967296.0f ).x;
	// return NormalizedRandomFloat() / 2.0f;

	// return saturate( ( snoise3D( idx * 0.01f ) / 2.0f ) + 1.0f ) / 2.0f;
	// return saturate( snoise3D( idx * 0.04f ) / 4.0f );
}
//=============================================================================================================================
uint GetAlphaValueForIdx( ivec3 idx ) {
	return imageLoad( DDATex, idx ).a;
}
//=============================================================================================================================
vec3 GetColorForIdx( ivec3 idx ) {
	// return vec3( 0.23f, 0.618f, 0.123f ).ggg;
//	return vec3( 0.99f );
	// return viridis( ( pcg3d( uvec3( idx ) ).x / 4294967296.0f ) / 5.0f + 0.6f );
	// return vec3( pcg3d( uvec3( idx ) ) / 4294967296.0f ) + vec3( 0.8f );
	// return mix( vec3( 0.99f ), vec3( 0.99, 0.6f, 0.1f ), pcg3d( uvec3( idx ) ).x / 4294967296.0f );
	// return mix( vec3( 0.618f ), vec3( 0.0, 0.0f, 0.0f ), saturate( pcg3d( uvec3( idx ) ) / 4294967296.0f ) );
	 return GetLuma( imageLoad( DDATex, idx ).rgb / 255.0f ) * 3.0f * nvidia;
	// return magma( imageLoad( HeightmapTex, idx.xy ).r );
}
//=============================================================================================================================
int GetMaterialIDForIdx( ivec3 idx ) {
	// return DIFFUSE;
	// return MIRROR;
	// return REFRACTIVE;
	// return NormalizedRandomFloat() < 0.9f ? MIRROR : DIFFUSE;
	return int( imageLoad( DDATex, idx ).a );
}
//=============================================================================================================================
intersection_t DDATraversal( in ray_t ray, in float distanceToBounds ) {
	ray_t rayCache = ray;

	// accounting for the bounding box
	if ( distanceToBounds > 0.0f ) {
		ray.origin += ray.direction * distanceToBounds;
	}

	// map the ray into the integer grid space
	vec3 scales = ddaSpheresBoundSize / max( ddaSpheresBoundSize.x, max( ddaSpheresBoundSize.y, ddaSpheresBoundSize.z ) );
	const vec3 res = scales * vec3( ddaSpheresResolution );

	const float epsilon = 0.001f;
	ray.origin = vec3(
		RangeRemapValue( ray.origin.x, -ddaSpheresBoundSize.x / 2.0f, ddaSpheresBoundSize.x / 2.0f, epsilon, res.x - epsilon ),
		RangeRemapValue( ray.origin.y, -ddaSpheresBoundSize.y / 2.0f, ddaSpheresBoundSize.y / 2.0f, epsilon, res.y - epsilon ),
		RangeRemapValue( ray.origin.z, -ddaSpheresBoundSize.z / 2.0f, ddaSpheresBoundSize.z / 2.0f, epsilon, res.z - epsilon )
	);

	// prep for traversal
	bool hitFound = false;
	intersection_t intersection = DefaultIntersection();

	// DDA traversal
	// from https://www.shadertoy.com/view/7sdSzH
	vec3 deltaDist = 1.0f / abs( ray.direction );
	ivec3 rayStep = ivec3( sign( ray.direction ) );
	bvec3 mask0 = bvec3( false );
	ivec3 mapPos0 = ivec3( floor( ray.origin ) );
	vec3 sideDist0 = ( sign( ray.direction ) * ( vec3( mapPos0 ) - ray.origin ) + ( sign( ray.direction ) * 0.5f ) + 0.5f ) * deltaDist;

	#define MAX_RAY_STEPS 2000
	for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, ivec3( res ) ) ) ); i++ ) {
	// for ( int i = 0; i < MAX_RAY_STEPS; i++ ) {

		// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
		bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
		vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
		ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

		// checking mapPos0 for hit
		if ( CheckValidityOfIdx( mapPos0 ) ) {

// changing behavior of the traversal - the disks are not super compatible with the sphere/box intersection code
#define SPHEREORBOX
// #define DISKS

			#ifdef SPHEREORBOX
				// see if we found an intersection - ball will almost fill the grid cell
				// iqIntersect test = IntersectBox( ray, vec3( mapPos0 ) + vec3( 0.5f ), vec3( 0.5f ) );
				iqIntersect test = IntersectSphere( ray, vec3( mapPos0 ) + GetOffsetForIdx( mapPos0 ), GetRadiusForIdx( mapPos0 ) );
				const bool behindOrigin = ( test.a.x < 0.0f && test.b.x < 0.0f );

				// update ray, to indicate hit location
				if ( !IsEmpty( test ) && !behindOrigin ) {
					intersection.frontfaceHit = !( test.a.x < 0.0f && test.b.x >= 0.0f );

					// get the location of the intersection
					ray.origin = ray.origin + ray.direction * ( intersection.frontfaceHit ? test.a.x : test.b.x );

					ray.origin = vec3( // map the ray back into the world space
						RangeRemapValue( ray.origin.x, epsilon, res.x - epsilon, -ddaSpheresBoundSize.x / 2.0f, ddaSpheresBoundSize.x / 2.0f ),
						RangeRemapValue( ray.origin.y, epsilon, res.y - epsilon, -ddaSpheresBoundSize.y / 2.0f, ddaSpheresBoundSize.y / 2.0f ),
						RangeRemapValue( ray.origin.z, epsilon, res.z - epsilon, -ddaSpheresBoundSize.z / 2.0f, ddaSpheresBoundSize.z / 2.0f )
					);

					intersection.dTravel = distance( ray.origin, rayCache.origin );
					intersection.normal = intersection.frontfaceHit ? test.a.yzw : test.b.yzw;
					// intersection.normal *= -1.0f;
					// intersection.materialID = GetMaterialIDForIdx( mapPos0 );

					// vec3 testVal = ( pcg3d( uvec3( mapPos0 ) ) / 4294967296.0f );
					// vec3 testVal = vec3( 1.0f );
					// if ( testVal.x < 0.05f ) {
					// // 	intersection.albedo = 3.0f * ( vec3( 0.618f ) + inferno( testVal.g ) );
					// // } else if ( testVal.x < 0.1f ) {
					// 	// intersection.materialID = METALLIC;
					// 	intersection.materialID = EMISSIVE;
					// 	// intersection.albedo = vec3( 0.831f, 0.397f, 0.038f ); // honey
					// 	ray_t ray;
					// 	ray.origin = vec3( 0.0f );
					// 	ray.direction = normalize( testVal );
					// 	intersection.albedo = SkyColor( ray );
					// 	// intersection.roughness = 0.9f;
					// } else {
						// intersection.materialID = MIRROR;
						intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
						intersection.albedo = GetColorForIdx( mapPos0 );
						// intersection.albedo = vec3( 0.831f, 0.397f, 0.038f ).grb; // honey
						// intersection.albedo = vec3( 0.9f );
						intersection.roughness = 0.2f;
						// intersection.roughness = 0.0f;
						// intersection.roughness = 0.9f;
						// intersection.materialID = DIFFUSE;
					// }

					// intersection.materialID = NormalizedRandomFloat() < 0.9f ? MIRROR : DIFFUSE;
					// if ( GetAlphaValueForIdx( mapPos0 ) < 35 ) {
					// 	// intersection.albedo *= 3.0f;
					// 	// intersection.materialID = EMISSIVE;
					// 	// intersection.materialID = DIFFUSE;
					// 	intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
					// } else if ( GetAlphaValueForIdx( mapPos0 ) < 130 ) {
					// 	intersection.materialID = MIRROR;
					// 	// intersection.materialID = NormalizedRandomFloat() > 0.9f ? MIRROR : DIFFUSE;
					// 	// intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
					// } else {
					// 	intersection.materialID = DIFFUSE;
					// }
					break;
				}
			#endif

			#ifdef DISKS
				// disk test
				const vec3 planePt = vec3( mapPos0 ) + vec3( 0.5f );
				vec2 e = vec2( epsilon, 0.0f );
				vec3 p = GetPositionForIdx( mapPos0 );
				const vec3 normal = normalize(
					vec3( ddaDiskSDF( p ) ) - vec3(
						vec3( ddaDiskSDF( p - e.xyy ), ddaDiskSDF( p - e.yxy ), ddaDiskSDF( p - e.yyx ) )
					)
				);

				const float planeDistance = -( dot( ray.origin - planePt, normal ) ) / dot( ray.direction, normal );
				const vec3 hitPointInPlane = ray.origin + ray.direction * planeDistance;
				const float radius = 0.5f;

				if ( distance( planePt, hitPointInPlane ) < radius ) {

					// get the location of the intersection
					ray.origin = hitPointInPlane;

					ray.origin = vec3( // map the ray back into the world space
						RangeRemapValue( ray.origin.x, epsilon, res.x - epsilon, -ddaSpheresBoundSize.x / 2.0f, ddaSpheresBoundSize.x / 2.0f ),
						RangeRemapValue( ray.origin.y, epsilon, res.y - epsilon, -ddaSpheresBoundSize.y / 2.0f, ddaSpheresBoundSize.y / 2.0f ),
						RangeRemapValue( ray.origin.z, epsilon, res.z - epsilon, -ddaSpheresBoundSize.z / 2.0f, ddaSpheresBoundSize.z / 2.0f )
					);

					intersection.dTravel = distance( ray.origin, rayCache.origin );
					intersection.frontfaceHit = dot( normal, ray.direction ) < 0.0f;
					// intersection.normal = intersection.frontfaceHit ? normal : -normal;
					intersection.normal = intersection.frontfaceHit ? -normal : normal;
					intersection.roughness = 0.01f;
					intersection.IoR = 1.0f / 1.5f;
					// intersection.materialID = GetMaterialIDForIdx( mapPos0 );
					// intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
					// intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : MIRROR;
					intersection.materialID = DIFFUSE;
					// intersection.materialID = NormalizedRandomFloat() > 0.9f ? MIRROR : DIFFUSE;
					intersection.albedo = GetColorForIdx( mapPos0 );
					break;
				}
			#endif
		}
		sideDist0 = sideDist1;
		mapPos0 = mapPos1;
	}

	// return the state, when we drop out of the loop
	return intersection;
}


//=============================================================================================================================
intersection_t VoxelIntersection( in ray_t ray ) {
	intersection_t intersection = DefaultIntersection();

	iqIntersect boundingBoxIntersection = IntersectBox( ray, vec3( 0.0f ), vec3( ddaSpheresBoundSize / 2.0f ) );
	const bool boxBehindOrigin = ( boundingBoxIntersection.a.x < 0.0f && boundingBoxIntersection.b.x < 0.0f );
	const bool backfaceHit = ( boundingBoxIntersection.a.x < 0.0f && boundingBoxIntersection.b.x >= 0.0f );

	if ( !IsEmpty( boundingBoxIntersection ) && !boxBehindOrigin ) {
		intersection = DDATraversal( ray, boundingBoxIntersection.a.x );
	}

	return intersection;
}
//=============================================================================================================================
layout( binding = 4, rg32ui ) uniform uimage3D SpherePack;
//=============================================================================================================================
#include "intersect.h"
intersection_t SpherePackDDA( in ray_t ray ) {
	ray_t rayCache = ray;
	intersection_t intersection = DefaultIntersection();

	// box intersection
	float tMin, tMax;
	const ivec3 iS = imageSize( SpherePack ).xyz;
	const float scale = 0.125f;
	const vec3 blockSize = vec3( iS ) * scale;
	const vec3 blockSizeHalf = vec3( blockSize ) / 2.0f;

	// then intersect with the AABB
	const bool hit = IntersectAABB( ray.origin, ray.direction, -blockSizeHalf, blockSizeHalf, tMin, tMax );
	const bool behindOrigin = ( tMin < 0.0f && tMax < 0.0f );
	const bool backface = ( tMin < 0.0f && tMax >= 0.0f );
	const float epsilon = 0.001f;

	if ( hit && !behindOrigin ) { // texture sample
		// get a sample point in grid space... start at ray origin if you see a backface, or at the closest positive hit
		vec3 p = backface ? ray.origin : ( ray.origin + tMin * ray.direction );
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
		vec3 deltaDist = 1.0f / abs( ray.direction );
		ivec3 rayStep = ivec3( sign( ray.direction ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( p + 0.0f ) );
		vec3 sideDist0 = ( sign( ray.direction ) * ( vec3( mapPos0 ) - p ) + ( sign( ray.direction ) * 0.5f ) + 0.5f ) * deltaDist;

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
				vec3 colorCache = vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() );
				seed = seedCache;

				iqIntersect test = IntersectSphere( ray, center - blockSizeHalf, radius * scale );
				const bool behindOrigin = ( test.a.x < 0.0f && test.b.x < 0.0f );

				if ( !IsEmpty( test ) && !behindOrigin ) {
					// if you get a hit, fill out the struct
					intersection.frontfaceHit = !( test.a.x < 0.0f && test.b.x >= 0.0f );

					// get the location of the intersection
					ray.origin = ray.origin + ray.direction * ( intersection.frontfaceHit ? test.a.x : test.b.x );

					intersection.dTravel = distance( ray.origin, rayCache.origin );
					intersection.normal = normalize( intersection.frontfaceHit ? test.a.yzw : test.b.yzw );
					 intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
//					intersection.materialID = DIFFUSE;
//					intersection.materialID = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
					// intersection.IoR = 1.0f / ( 1.5f + 0.1f * colorCache.r );
					intersection.IoR = 1.0f / ( 1.5f + 0.1f * colorCache.r );
//					intersection.materialID = NormalizedRandomFloat() > ( Reflectance(  min( dot( -normalize( ray.direction ), intersection.normal ), 1.0f ), intersection.IoR ) ) ? DIFFUSE : MIRROR;
					// intersection.albedo = mix( honey, vec3( 1.0f ), pow( saturate( RangeRemapValue( radius, 0.0f, 10.0f, 0.0f, 1.0f ) ), 1.0f ) );
//					intersection.albedo = intersection.materialID == MIRROR ? vec3( 0.99f ) : mix( nvidia, vec3( 0.0f ), colorCache.r );
					// intersection.albedo = vec3( 0.618f );
					intersection.albedo = vec3( 0.8f ) * mix( nvidia, honey, colorCache.r );
					 if ( colorCache.g < 0.1f ) {
						intersection.roughness = colorCache.b;
//						 intersection.albedo = vec3( colorCache.b );
					 }

//					if ( intersection.materialID == MIRROR ) {
//						intersection.albedo = mix( blood, vec3( 0.0f ), mix( colorCache.r, radius / 15.0f, 0.8f ) );
//					}
//
//					if ( radius > 10.0f ) {
//						intersection.materialID = EMISSIVE_FRESNEL;
//						intersection.albedo *= 0.1f;
//					}
					/*
					if ( colorCache.r < 0.07f && radius < 4.0f ) {
						intersection.materialID = EMISSIVE_FRESNEL;
						intersection.albedo = vec3( ( 0.3f + colorCache.b * 0.4f ) * mix( honey, carrot, colorCache.g / 3.0f ) );
					} else {
						if ( colorCache.b < 0.3f ) {
							intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
							// intersection.albedo = intersection.materialID == MIRROR ? vec3( 0.99f ) : ( ( colorCache.b < 0.3f ) ? ( honey * colorCache.r ) : ( ( colorCache.b < 0.6f ) ? blood * colorCache.r : vec3( 0.01f ) ) );
							intersection.roughness = colorCache.b;
							intersection.albedo = vec3( 0.99f - 0.8f * colorCache.g );
						} else {
							// intersection.albedo = intersection.materialID == MIRROR ? vec3( 0.99f ) : vec3( 0.01f );
							intersection.albedo = intersection.materialID == MIRROR ? vec3( 0.99f ) : nvidia * colorCache.g;
						}
					}
					*/
					break;
				}
			}

			sideDist0 = sideDist1;
			mapPos0 = mapPos1;
		}

		// otherwise, fall through with a default intersection result
	}

	return intersection;
}
//=============================================================================================================================
uniform bool maskedPlaneEnable;
layout( rgba8ui ) readonly uniform uimage3D textBuffer;
//=============================================================================================================================
bool maskedPlaneMaskEval( in vec3 location, out vec3 color ) {
	// bool mask = ( step( -0.0f, // placeholder, glyph mapping + texture stuff next
	// 	cos( PI * float( location.x ) + PI / 2.0f ) *
	// 	cos( PI * float( location.y ) + PI / 2.0f ) ) == 0 );

	ivec2 pixelIndex = ivec2( floor( location.xy ) );
	ivec2 bin = ivec2( floor( pixelIndex.xy / vec2( 8.0f, 16.0f ) ) );
	ivec2 offset = ivec2( pixelIndex.xy ) % ivec2( 8, 16 );

	// return ( step( -0.0f, // placeholder, glyph mapping + texture stuff next
	// 	cos( PI * float( pixelIndex.x ) + PI / 2.0f ) *
	// 	cos( PI * float( pixelIndex.y ) + PI / 2.0f ) ) == 0 );

	const uvec3 texDims = uvec3( imageSize( textBuffer ) );
	// uvec4 sampleValue = imageLoad( textBuffer, ivec3( bin.x % texDims.x, bin.y % texDims.y, location.z ) ); // repeated
	uvec4 sampleValue = imageLoad( textBuffer, ivec3( bin.x, bin.y % texDims.y, location.z ) );

	color = vec3( sampleValue.xyz ) / 255.0f;

	// int onGlyph = fontRef( int( abs( snoise3D( vec3( bin.xy, location.z ) ) ) * 255 ), offset );
	int onGlyph = fontRef( int( sampleValue.a ), offset );
	return onGlyph > 0;
}
//=============================================================================================================================
intersection_t MaskedPlaneIntersect( in ray_t ray, in mat3 transform, in vec3 basePoint, in ivec3 dims, in vec3 scale ) {
	intersection_t intersection = DefaultIntersection();

	// determine the direction of travel, so that we know what order to step through the planes in
	vec3 normal = transform * vec3( 0.0f, 0.0f, 1.0f );
	const float rayDot = dot( ray.direction, normal );

	// if the ray is parallel, we can early out with a nohit default intersection
	if ( rayDot == 0.0f ) {
		return intersection;
	}

	// for x,y mapping on the plane
	const vec3 xVec = transform * vec3( 1.0f, 0.0f, 0.0f );
	const vec3 yVec = transform * vec3( 0.0f, 1.0f, 0.0f );

	// is this ray going in the same direction as the normal vector
	const bool nAlign = ( rayDot < 0.0f );
	const int start  = nAlign ? dims.z - 1 : 0;
	const int thresh = nAlign ? 0 : dims.z - 1;
	// iterating through the planes - top to bottom, or bottom to top
	for ( int i = start; nAlign ? ( i >= thresh ) : ( i <= thresh ); nAlign ? ( i-- ) : ( i++ ) ) {
		// do the plane test
		const vec3 planePt = i * scale.z * normal + basePoint;
		const float planeDistance = -( dot( ray.origin - planePt, normal ) ) / dot( ray.direction, normal );
		if ( planeDistance > 0.0f ) {

			// get the 2d location on the plane
			const vec3 planeIntersection = ray.origin + ray.direction * planeDistance;
			const vec3 intersectionPointInPlane = planeIntersection - planePt;
			const vec2 projectedCoords = vec2( // vector projected onto vector ( +x,y scale factors )
				scale.x * dot( intersectionPointInPlane, xVec ) + imageSize( textBuffer ).x / 2,
				scale.y * dot( intersectionPointInPlane, yVec ) + imageSize( textBuffer ).y / 2
			);

			// check the projected coordinates against the input boundary size
			if ( abs( projectedCoords.x ) <= float( dims.x * scale.x ) / 2.0f && abs( projectedCoords.y ) <= float( dims.y * scale.y ) / 2.0f ) {
				// now we have a 2d point to work with
				vec3 color;
				if ( maskedPlaneMaskEval( vec3( projectedCoords, i ), color ) ) {
					intersection.dTravel = planeDistance;
					intersection.albedo = color;
					intersection.frontfaceHit = nAlign;
					intersection.normal = nAlign ? normal : -normal;
					intersection.frontfaceHit = true;
					intersection.materialID = DIFFUSE;
					break; // we can actually early out the first time we get a good hit, passing the mask test, since we are going in order
				}
			}
		}
	}
	// fall out of the loop and return
	return intersection;
}
//=============================================================================================================================
intersection_t MaskedPlaneIntersect( in ray_t ray ) {
	mat3 basis = mat3( 1.0f ); // identity matrix - should be able to support rotation
	const vec3 point = vec3( 0.0f ); // base point for the plane
	const ivec3 dims = ivec3( imageSize( textBuffer ) ); // "voxel" dimensions - x,y glyph res + z number of planes
	const vec3 scale = vec3( 1000.0f, 1000.0f, 0.003f ); // scaling x,y, and z is spacing between the planes
	// return MaskedPlaneIntersect( ray, basis, point, dims, scale );

	intersection_t intersect1 = MaskedPlaneIntersect( ray, basis, point, dims, scale );

	basis = Rotate3D( 1.5f, vec3( 0.0f, 1.0f, 0.0f ) ) * Rotate3D( 0.5f, vec3( 1.0f, 1.0f, 0.0f ) ) * basis;
	intersection_t intersect2 = MaskedPlaneIntersect( ray, basis, point, dims, scale );

	basis = Rotate3D( 1.5f, vec3( 0.0f, 1.0f, 0.0f ) ) * Rotate3D( 2.5f, vec3( 1.0f, 1.0f, 0.0f ) ) * basis;
	intersection_t intersect3 = MaskedPlaneIntersect( ray, basis, point, dims, scale );

	intersection_t result = DefaultIntersection();
	float minDistance = vmin( vec3( intersect1.dTravel, intersect2.dTravel, intersect3.dTravel ) );
	if ( minDistance == intersect1.dTravel ) {
		result = intersect1;
	} else if ( minDistance == intersect2.dTravel ) {
		result = intersect2;
	} else {
		result = intersect3;
	}
	return result;
}
//=============================================================================================================================
// 
//=============================================================================================================================
// CPU-generated sphere array
	// I want to generalize this:
		// primitive type
		// primitive parameters
		// material details
uniform int numExplicitPrimitives;
uniform bool explicitListEnable;
struct sphere_t {
	vec4 positionRadius;
	vec4 colorMaterial;
	vec4 materialProps;
};
layout( binding = 0, std430 ) readonly buffer sphereData {
	sphere_t spheres[];
};
//=============================================================================================================================
intersection_t ExplicitListIntersect( in ray_t ray ) {
	int indexOfHit = -1;
	float nearestOverallHit = 1e20f;
	vec3 currentNormal = vec3( 0.0f );

	for ( int i = 0; i < numExplicitPrimitives; i++ ) {
		vec3 tempNormal = vec3( 0.0f );

		// this will need to be handled in a way that can do multiple types of parameters - distance and normal are the outputs
		const float currentNearestPositive =
			iSphereOffset( ray.origin, ray.direction, tempNormal, spheres[ i ].positionRadius.w, spheres[ i ].positionRadius.xyz );
			// iBoxOffset( ray.origin, ray.direction, tempNormal, vec3( spheres[ i ].positionRadius.w ), spheres[ i ].positionRadius.xyz );
			// iTorusOffset( ray.origin, ray.direction, tempNormal, vec2( spheres[ i ].positionRadius.w, spheres[ i ].positionRadius.w / 5.0f ), spheres[ i ].positionRadius.xyz );
			// iEllipsoidOffset( ray.origin, ray.direction, tempNormal, vec3( spheres[ i ].positionRadius.w, spheres[ i ].positionRadius.w, spheres[ i ].positionRadius.w * 3.0f ), spheres[ i ].positionRadius.xyz );
			// iGoursat( ray.origin - spheres[ i ].positionRadius.xyz, ray.direction, tempNormal, 5.0f * 0.16f * spheres[ i ].positionRadius.w, spheres[ i ].positionRadius.w );

		nearestOverallHit = min( currentNearestPositive, nearestOverallHit );
		if ( currentNearestPositive == nearestOverallHit ) {
			currentNormal = tempNormal;
			indexOfHit = i;
		}
	}

	// perlinfbm( vec3 p, float freq, int octaves)

	intersection_t result = DefaultIntersection();
	if ( indexOfHit != -1 ) { // at least one primitive got a valid hit
		result.dTravel = nearestOverallHit;
		result.frontfaceHit = ( dot( ray.direction, currentNormal ) < 0.0f );
		result.normal = currentNormal;
		result.materialID = int( spheres[ indexOfHit ].colorMaterial.w );
		result.roughness = spheres[ indexOfHit ].materialProps.g;
		if ( result.materialID == EMISSIVE_FRESNEL || result.materialID == EMISSIVE ) {
			result.albedo /= 100.0f;

			uint rngCache = seed;
			seed = indexOfHit * 42069;
			if ( NormalizedRandomFloat() < 0.75f ) {
				result.materialID = REFRACTIVE;
			}
			seed = rngCache;
		}
		if ( result.materialID == CHECKER ) {
			result.materialID = ( NormalizedRandomFloat() > 0.9f ) ? MIRROR : DIFFUSE;
			if ( result.materialID == DIFFUSE ) {
				result.albedo = vec3( nickel );
			} else {
				result.albedo = vec3( 0.99f );
			}
		}
		if ( result.materialID == REFRACTIVE && result.frontfaceHit == false ) {
			result.materialID = REFRACTIVE_BACKFACE; // this needs to be more generalized
			// result.normal = -result.normal; // this is already handled, for the sphere, at least...
		}
		result.albedo = spheres[ indexOfHit ].colorMaterial.xyz;
//		result.IoR = spheres[ indexOfHit ].materialProps.r;

		 result.roughness = spheres[ indexOfHit ].materialProps.g;
		// result.roughness = saturate( pow( perlinfbm( ray.origin + ray.direction * result.dTravel, 20.0f, 4 ), 3.0f ) * 0.2f );
	}
	return result;
}
//=============================================================================================================================
intersection_t GetNearestSceneIntersection( in ray_t ray ) {
	// return a single intersection_t representing the closest ray intersection
	intersection_t SDFResult		= raymarchEnable		? raymarch( ray )				: DefaultIntersection();
	 intersection_t VoxelResult		= ddaSpheresEnable		? VoxelIntersection( ray )		: DefaultIntersection();
//	intersection_t VoxelResult		= SpherePackDDA( ray );
	intersection_t MaskedPlane		= maskedPlaneEnable		? MaskedPlaneIntersect( ray )	: DefaultIntersection();
	intersection_t ExplicitList		= explicitListEnable	? ExplicitListIntersect( ray )	: DefaultIntersection();
	intersection_t convexPolyhedra	= DefaultIntersection();
	// intersection_t convexPolyhedra	= convexPolyhedraIntersect( ray.origin, ray.direction, 4200690926, DefaultIntersection().dTravel );

	intersection_t result = DefaultIntersection();
	float minDistance = min( vmin( vec4( SDFResult.dTravel, VoxelResult.dTravel, MaskedPlane.dTravel, ExplicitList.dTravel ) ), convexPolyhedra.dTravel );
	if ( minDistance == SDFResult.dTravel ) {
		result = SDFResult;
	} else if ( minDistance == VoxelResult.dTravel ) {
		result = VoxelResult;
	} else if ( minDistance == MaskedPlane.dTravel ) {
		result = MaskedPlane;
	} else if ( minDistance == ExplicitList.dTravel ) {
		result = ExplicitList;
		// result.IoR = 1.0f / 1.7f; // hack to make marble IoR constant
	} else if ( minDistance == convexPolyhedra.dTravel ) {
		result = convexPolyhedra;
	}
	return result;
}

//=============================================================================================================================
//=============================================================================================================================

result_t GetNewSample( in vec2 uv ) {
	// get the camera ray for this uv
	const ray_t rayInitial = GetCameraRayForUV( uv );

	// initialize state for the ray
	result_t result;

	// initialize the "previous", "current" values
	ray_t ray, rayPrevious;
	ray = rayPrevious = rayInitial;

	// pathtracing accumulators
	vec3 finalColor = vec3( 0.0f );
	vec3 throughput = vec3( 1.0f );

	for ( int bounce = 0; bounce < maxBounces; bounce++ ) {
		// get the scene intersection for this bounce
		intersection_t intersection = GetNearestSceneIntersection( ray );

		if ( bounce == 0 ) // first hit populates the normal/depth buffer result
			result.normalD = vec4( intersection.normal, intersection.dTravel );

		// update the ray values
		rayPrevious = ray;
		ray.origin = rayPrevious.origin + intersection.dTravel * rayPrevious.direction;

		// attenuate by some amount times the distance traveled... tbd.. this kind of sucks
		// throughput = throughput * exp( -0.03f * intersection.dTravel );

		// epsilon bump, along the normal vector, for non-refractive surfaces
		if ( intersection.materialID != REFRACTIVE && intersection.materialID != REFRACTIVE_BACKFACE )
			ray.origin += 2.0f * epsilon * intersection.normal;

		// evaluate the material - updates finalColor, throughput, ray - return false on ray escape
		if ( !EvaluateMaterial( finalColor, throughput, intersection, ray, rayPrevious ) ) break;

		// russian roulette termination
		float maxChannel = vmax( throughput );
		if ( NormalizedRandomFloat() > maxChannel ) break;
		throughput *= 1.0f / maxChannel; // compensation term
	}

	// I want to figure out doing a lit object on a black background, for aesthetic purposes
	// if ( abs( float( GetLuma( throughput - vec3( 1.0f ) ) ) ) > 0.001f ) {
	// 	finalColor = vec3( 0.0f );
	// }

	// apply final exposure value and return
	result.color.rgb = finalColor * exposure;
	return result;
}

//=============================================================================================================================
//=============================================================================================================================

void ProcessTileInvocation( in ivec2 pixel ) {
	// calculate the UV from the pixel location
	vec2 uv = ( vec2( pixel ) + SubpixelOffset() ) / imageSize( accumulatorColor ).xy;
	uv = uv * 2.0f - vec2( 1.0f );

	// get the new color sample
	result_t newData = GetNewSample( uv );

	// load the previous color, normal, depth values
	vec4 previousColor = imageLoad( accumulatorColor, pixel );
	vec4 previousNormalD = imageLoad( accumulatorNormalsAndDepth, pixel );

	// mix the new and old values based on the current sampleCount
	float sampleCount = previousColor.a + 1.0f;
	const float mixFactor = 1.0f / sampleCount;
	const vec4 mixedColor = vec4( mix( previousColor.rgb, newData.color.rgb, mixFactor ), sampleCount );
	const vec4 mixedNormalD = vec4(
		clamp( mix( previousNormalD.xyz, newData.normalD.xyz, mixFactor ), -1.0f, 1.0f ),
		mix( previousNormalD.w, newData.normalD.w, mixFactor )
	);

	// store the values back
	imageStore( accumulatorColor, pixel, clamp( mixedColor, 0.0f, 10000.0f ) );
	imageStore( accumulatorNormalsAndDepth, pixel, mixedNormalD );
}

//=============================================================================================================================
//=============================================================================================================================

void main() {
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy ) + tileOffset.xy;
	if ( BoundsCheck( location ) ) {
		// seed defined in random.h, value used for the wang hash
		seed = wangSeed + 42069 * location.x + 451 * location.y;
		ProcessTileInvocation( location );
	}
}