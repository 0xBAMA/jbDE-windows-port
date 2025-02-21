#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

#ifndef UINT_MAX
#define UINT_MAX (0xFFFFFFFF-1)
#endif

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// rasterizer result
uniform sampler2D depthTex;
uniform sampler2D depthTexBack;
uniform usampler2D primitiveID;
uniform usampler2D primitiveIDBack;

// rng seeding
#include "random.h"
uniform int inSeed;

#include "consistentPrimitives.glsl.h"
#include "mathUtils.h"

uniform vec3 eyePosition;	// location of the viewer

// SSAO Constants / Support Functions
// uniform int AONumSamples;
// uniform float AOIntensity;
// uniform float AOScale;
// uniform float AOBias;
// uniform float AOSampleRadius;
// uniform float AOMaxDistance;
const int AONumSamples = 16;
const float AOIntensity = 2.4f;
const float AOScale = 1.0f;
const float AOBias = 0.05f;
const float AOSampleRadius = 0.02f;
const float AOMaxDistance = 0.07f;

// tbd which are actually needed
// uniform mat4 projTransform;
// uniform mat4 projTransformInverse;
// uniform mat4 viewTransform;
// uniform mat4 viewTransformInverse;
// uniform mat4 combinedTransform;
uniform mat4 combinedTransformInverse;
vec3 posFromTexcoord ( vec2 uv ) { // get worldspace position, by way of the depth texture
	const float depthSample = texture( depthTex, uv ).r * 2.0f - 1.0f;
	const vec4 clipPos = vec4( uv * 2.0f - 1.0f, depthSample, 1.0f );
	// vec4 viewPos = projTransformInverse * clipPos;
	// viewPos /= viewPos.w;
	// return vec4( viewTransformInverse * viewPos ).xyz;
	const vec4 viewPos = combinedTransformInverse * clipPos;
	return viewPos.xyz / viewPos.w;
}

// Linearizes a Z buffer value
uniform float farZ;
uniform float nearZ;
// https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value
float GetLinearZ ( float depth ) {
	// bias it from 0..1 to -1..1
	float linear = nearZ / ( farZ - depth * ( farZ - nearZ ) ) * farZ;
	return ( linear * 2.0f ) - 1.0f;
}

float DoAO ( const vec2 texCoord, const vec2 uv, const vec3 p, const vec3 cNormal ) {
	vec3 diff = posFromTexcoord( texCoord + uv ) - p;
	const float l = length( diff );
	const vec3 v = diff / l;
	const float d = l * AOScale;
	float AO = max( 0.0f, dot( cNormal, v ) - AOBias ) * ( 1.0f / ( 1.0f + d ) );
	AO *= smoothstep( AOMaxDistance, AOMaxDistance * 0.5f, l );
	return AO;
}

// adapted from https://www.shadertoy.com/view/Ms33WB by Reinder Nijhoff 2016
float SpiralAO ( vec2 uv, vec3 p, vec3 n, float rad ) {
	float goldenAngle = 2.4f;
	float ao = 0.0f;
	float inv = 1.0f / float( AONumSamples );
	float radius = 0.0f;

	float rotatePhase = NormalizedRandomFloat() * 6.28f;
	float rStep = inv * rad;
	vec2 spiralUV = vec2( 0.0f );

	for ( int i = 0; i < AONumSamples; i++ ) {
		spiralUV.x = sin( rotatePhase );
		spiralUV.y = cos( rotatePhase );
		radius += rStep;
		ao += DoAO( uv, spiralUV * radius, p, n );
		rotatePhase += goldenAngle;
	}
	ao *= inv;
	return ao;
}

// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 1, std430 ) buffer parametersBuffer {
	parameters_t parametersList[];
};
// ===================================================================================================
struct pointSpriteParameters_t {
	float data[ 16 ];
};
layout( binding = 2, std430 ) buffer pointSpriteParametersBuffer {
	pointSpriteParameters_t pointSpriteParameters[];
};
// ===================================================================================================
uniform int numLights;
uniform vec3 ambientLighting;

struct lightParameters_t {
	vec4 position;
	vec4 color;
};
layout( binding = 3, std430 ) buffer lightParametersBuffer {
	lightParameters_t lightParameters[];
};
// ===================================================================================================

uniform float blendAmount;
uniform float volumetricStrength;
uniform vec3 volumetricColor;

#define SPHERE 0
#define CAPSULE 1
#define ROUNDEDBOX 2

void main () {
	// pixel location + rng seeding
	const ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const vec2 screenPos = vec2( writeLoc + 0.5f ) / textureSize( depthTex, 0 ).xy;
	seed = inSeed + 10612 * writeLoc.x + 385609 * writeLoc.y;

	// data from the framebuffer
	const uvec2 idSample = texelFetch( primitiveID, writeLoc, 0 ).rg;
	const float linearZ = GetLinearZ( texture( depthTex, screenPos ).r );

	// solved position
	const vec3 worldPos = posFromTexcoord( screenPos );
	vec3 color = vec3( 0.0f );

	vec3 rayOrigin = eyePosition;
	vec3 rayDirection = normalize( worldPos - eyePosition );

	vec3 normalSample = -rayDirection;

	bool drawing = false;
	bool emissive = false;
	float knownRadius = -1.0f;

	float roughness = 5.0f;

	if ( idSample.x != 0 ) { // these are texels which wrote out a fragment during the raster geo pass, bounding box impostors

		drawing = true;
		const parameters_t parameters = parametersList[ idSample.x - 1 ];

		float result = 0.0f;

	// we now have to solve for the normal, knowing the eye position, fragment position, and primitive ID, we can intersect and get the normal
		switch ( int( parameters.data[ 0 ] ) ) {
			// case SPHERE: // tbd
			// break;

		case CAPSULE:
			result = iCapsule( rayOrigin, rayDirection, normalSample,
				vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] ), // point A
				vec3( parameters.data[ 5 ], parameters.data[ 6 ], parameters.data[ 7 ] ), // point B
				parameters.data[ 4 ] ); // radius
			break;

		case ROUNDEDBOX:
			const vec3 centerPoint = vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] );
			const float packedEuler = parameters.data[ 4 ];
			const vec3 scaleFactors = vec3( parameters.data[ 5 ], parameters.data[ 6 ], parameters.data[ 7 ] );
			const float roundingFactor = parameters.data[ 8 ];

			const float theta = fract( packedEuler ) * 2.0f * pi;
			const float phi = ( floor( packedEuler ) / 255.0f ) * ( pi / 2.0f );

			const mat3 transform = ( Rotate3D( -phi, vec3( 1.0f, 0.0f, 0.0f ) ) * Rotate3D( -theta, vec3( 0.0f, 1.0f, 0.0f ) ) );
			const vec3 rayDirectionAdjusted = ( transform * rayDirection );
			const vec3 rayOriginAdjusted = transform * ( rayOrigin - centerPoint );

			// going to have to figure out what the transforms need to be, in order to intersect with the transformed primitve
			result = iRoundedBox( rayOriginAdjusted, rayDirectionAdjusted, normalSample, scaleFactors, roundingFactor );

			// is it faster to do this, or to do the euler angle stuff, in inverse? need to profile, at scale
			// const mat3 inverseTransform = ( Rotate3D( theta, vec3( 0.0f, 1.0f, 0.0f ) ) * Rotate3D( phi, vec3( 1.0f, 0.0f, 0.0f ) ) );
			const mat3 inverseTransform = inverse( transform );
			normalSample = inverseTransform * normalSample;
			break;

		default:
			break;
		}


		// roughness = abs( parameters.data[ 15 ] );
		if ( parameters.data[ 15 ] < 0.0f ) { // using the color out of the buffer
			color = vec3( parameters.data[ 12 ], parameters.data[ 13 ], parameters.data[ 14 ] );
		} else { // todo: should interpret the alpha channel as a palette key
			color = vec3( parameters.data[ 15 ] ) * vec3( 0.6f, 0.1f, 0.0f );
		}

	} else if ( idSample.y != 0 ) { // using the second channel to signal point sprites

		// this will need to be revisited... I don't know how they will get normals back out - not critical, for now

		drawing = true;
		pointSpriteParameters_t parameters = pointSpriteParameters[ idSample.y - 1 ];

		emissive = ( parameters.data[ 4 ] != parameters.data[ 5 ] );
		knownRadius = parameters.data[ 4 ];

		if ( parameters.data[ 15 ] < 0.0f ) { // using the color out of the buffer
			color = vec3( parameters.data[ 12 ], parameters.data[ 13 ], parameters.data[ 14 ] );
		} else { // todo: should interpret the alpha channel as a palette key
			color = vec3( parameters.data[ 15 ] ) * vec3( 0.6f, 0.1f, 0.0f );
		}

	} // else this is a background pixel

	if ( drawing ) {

		// lighting calculations
		vec3 lightContribution = ambientLighting;
		for ( int i = 0; i < numLights; i++ ) {

			// phong setup
			const vec3 lightLocation = lightParameters[ i ].position.xyz;
			const vec3 lightVector = normalize( lightLocation - worldPos.xyz );
			const vec3 reflectedVector = normalize( reflect( lightVector, normalSample ) );

			// lighting calculation
			const float lightDot = dot( normalSample, lightVector );
			const float dLight = distance( worldPos.xyz, lightLocation );
			const float distanceFactor = 1.0f / pow( dLight, 2.0f );
			const float diffuseContribution = distanceFactor * max( lightDot, 0.0f );
			const float specularContribution = distanceFactor * pow( max( dot( -reflectedVector, normalize( worldPos - eyePosition ) ), 0.0f ), roughness );

			lightContribution += lightParameters[ i ].color.rgb * ( diffuseContribution + ( ( lightDot > 0.0f ) ? specularContribution : 0.0f ) );
		}

	// need to evaluate which of these looks the best, eventually - also figure out what settings I was using because this is looking a bit rough
		// float AOFactor = 1.0f - SpiralAO( screenPos, worldPos, normalSample, AOSampleRadius ) * AOIntensity;
		// float AOFactor = 1.0f - SpiralAO( screenPos, worldPos, normalSample, AOSampleRadius / texture( depthTex, screenPos ).r ) * AOIntensity;
		float AOFactor = 1.0f - SpiralAO( screenPos, worldPos, normalSample, AOSampleRadius / linearZ ) * AOIntensity;
		// float AOFactor = 1.0f - SpiralAO( screenPos, worldPos, normalSample, AOSampleRadius / ( texture( depthTex, screenPos ).r * 2.0f - 1.0f ) ) * AOIntensity;

		// seed = idSample;
		// color = pow( AOFactor, 2.0f ) * vec3( 1.0f, 0.6f, 0.3f ) * NormalizedRandomFloat();
		// color = vec3( 1.0f, 0.3f, 0.1f ) * NormalizedRandomFloat();
		// color = vec3( 0.618f ) * pow( AOFactor, 2.0f );
		// color = vec3( GetLinearZ( texture( depthTex, screenPos ).r ) / 100.0f ) *  pow( AOFactor, 2.0f );
		// color = vec3( GetLinearZ( texture( depthTex, screenPos ).r ) / 100.0f ) * AOFactor;

		if ( emissive ) {
			color *= 1.0f / pow( knownRadius, 2.0f );
		} else {
			color = lightContribution * color * AOFactor;
		}

	}

	// atmospheric/volumetric term...
	color += ( 1.0f - exp( -linearZ * volumetricStrength ) ) * volumetricColor;

	vec3 existingColor = imageLoad( accumulatorTexture, writeLoc ).rgb;
	color = mix( existingColor, color, blendAmount );

	// write out the result
	imageStore( accumulatorTexture, writeLoc, vec4( color, 1.0f ) );
}
