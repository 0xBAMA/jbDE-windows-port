#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "random.h"
#include "rayState2.h.glsl"
#include "biasGain.h"
#include "twigl.glsl"
//=============================================================================================================================
// pixel offset + ray state buffers (intersection scratch not used)
layout( binding = 0, std430 ) readonly buffer pixelOffsets { uvec2 offsets[]; };
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };
layout( binding = 2, std430 ) buffer intersectionBuffer { intersection_t intersectionScratch[]; };
//=============================================================================================================================
layout( location = 0, rgba8ui ) readonly uniform uimage2D blueNoise;
uniform ivec2 noiseOffset;
uniform int frameNumber;
vec4 Blue( in ivec2 loc ) { return imageLoad( blueNoise, ( loc + noiseOffset ) % imageSize( blueNoise ).xy ); }
//=============================================================================================================================
uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;
uniform vec3 viewerPosition;
uniform vec2 imageDimensions;
//=============================================================================================================================
uniform float FoV;
uniform int cameraMode;
uniform float uvScaleFactor;
uniform int DoFBokehMode;
uniform float DoFRadius;
uniform float DoFFocusDistance;
uniform float chromabScaleFactor;
// ...
//=============================================================================================================================
struct cameraRay {
	vec3 origin;		// initial ray source position (jittered)
	vec3 direction;		// initial ray direction
	vec3 transmission;	// for the chromatic aberration, if desired
};
//=============================================================================================================================
// camera types
#define DEFAULT		0
#define ORTHO		1
#define VORALDO		2
#define SPHERICAL	3
#define HDRI		4
#define COMPOUND	5
//=============================================================================================================================
cameraRay GetCameraRay ( vec2 uv ) {
	cameraRay temp;
	temp.origin = viewerPosition;
	temp.transmission = vec3( 1.0f );

	const float aspectRatio = imageDimensions.x / imageDimensions.y;
	uv *= uvScaleFactor;
	uv.x = uv.x * aspectRatio;

	float FoVLocal = FoV;

	// compute FoV offset - this also informs initial transmission values
	#if 1 // interesting motion blur ish thing from jittering FoV
		const float interpolationValue = NormalizedRandomFloat();
		const float i = interpolationValue * ( 3.1415f / 2.0f );
		// const float i = gainValue( interpolationValue, 0.7f ) * ( 3.1415f / 2.0f );  // parameterize bias/gain? tbd
		const vec3 colorWeight = vec3( sin( i ), sin( i * 2.0f ), cos( i ) ) * 1.618f;
		// convolving transmission with the offset... gives like chromatic aberration
		temp.transmission = pow( colorWeight, vec3( 1.0f / 2.2f ) );
		FoVLocal = FoV + chromabScaleFactor * gainValue( interpolationValue, 0.75f );
	#endif

	// use offset FoV to calculate camera ray direction...
	switch ( cameraMode ) {
		case DEFAULT: {
			temp.direction = normalize( uv.x * basisX + uv.y * basisY + ( 1.0f / FoVLocal ) * basisZ );
			break;
		}

		case ORTHO: {
			temp.origin = viewerPosition + basisX * FoVLocal * uv.x + basisY * FoVLocal * uv.y;
			temp.direction = basisZ;
			break;
		}

		case VORALDO: {
			// can combine with ortho... FoV = 0.0 gives you an ortho camera
				// use the FoV as the perspective factor
			temp.origin = viewerPosition + basisX * uv.x + basisY * uv.y;
			temp.direction = normalize( 2.0f * basisZ + temp.origin * FoVLocal );
			break;
		}

		case SPHERICAL: {
			uv.x -= 0.05f;
			uv = vec2( atan( uv.y, uv.x ) + 0.5f, ( length( uv ) + 0.5f ) * acos( -1.0f ) );
			vec3 baseVec = normalize( vec3( cos( uv.y ) * cos( uv.x ), sin( uv.y ), cos( uv.y ) * sin( uv.x ) ) );
			baseVec = rotate3D( pi / 2.0f, vec3( 2.5f, 0.4f, 1.0f ) ) * baseVec; // this is to match the other camera
			temp.origin = viewerPosition;
			// I'd like to avoid having the repeating rings... if the direction is set to vec3( 0.0f ), that's a dead ray, so ideally we can do that to kill
			temp.direction = normalize( -baseVec.x * basisX + baseVec.y * basisY + ( 1.0f / FoVLocal ) * baseVec.z * basisZ );
			break;
		}

		case HDRI: {

			break;
		}

		case COMPOUND: {

			break;
		}

		default: // shouldn't hit this
		break;
	}

	// jittering the ray origin helps with some aliasing issues...
	temp.origin -= ( Blue( ivec2( gl_GlobalInvocationID.x, gl_GlobalInvocationID.x % 512 ) ).z / 255.0f ) * 0.01f * temp.direction;

	// if the DoF is on, we do some additional calculation...
	if ( DoFRadius != 0.0f && temp.direction != vec3( 0.0f ) ) {
		// thin lens adjustment
		vec3 focuspoint = temp.origin + ( ( temp.direction * DoFFocusDistance ) / dot( temp.direction, basisZ ) );
		vec2 diskOffset = DoFRadius * GetBokehOffset( DoFBokehMode );
		temp.origin += diskOffset.x * basisX + diskOffset.y * basisY;
		temp.direction = normalize( focuspoint - temp.origin );
	}

	return temp;
}
//=============================================================================================================================
void main () {
	const int index = int( gl_GlobalInvocationID.x );
	ivec2 offset = ivec2( offsets[ index ] );
	seed = offset.x * 100625 + offset.y * 2324 + gl_GlobalInvocationID.x * 42069;

	// zero out the buffer entry
	StateReset( state[ index ] );

	// fill out the rayState based on the prepared cameraRay for this uv location... add weyl subpixel jitter
	// vec2 uv = ( ( vec2( offset ) + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) ) / imageDimensions ) * 2.0f - vec2( 1.0f );
	vec2 uv = ( ( vec2( offset ) + fract( vec2( ( frameNumber % 512 ) * 12664745, ( frameNumber % 512 ) * 9560333 ) / exp2( 24.0f ) ) ) / imageDimensions ) * 2.0f - vec2( 1.0f );

	cameraRay myRay = GetCameraRay( uv );
	SetTransmission( state[ index ], myRay.transmission );
	SetRayDirection( state[ index ], myRay.direction );
	SetRayOrigin( state[ index ], myRay.origin );
	SetPixelIndex( state[ index ], offset );
}