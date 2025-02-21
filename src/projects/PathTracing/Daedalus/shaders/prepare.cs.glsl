#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;
layout( location = 3, rgba32f ) uniform image2D tonemappedResult;

uniform sampler2D accumulatorColorTex;

// general tonemapping/grading parameters
uniform int tonemapMode;
uniform float gamma;
uniform float postExposure;
uniform mat3 saturation;
uniform vec3 colorTempAdjust;

// lens distortion
uniform bool enableLensDistort;
uniform bool lensDistortNormalize;
uniform int lensDistortNumSamples;
uniform vec3 lensDistortParametersStart;
uniform vec3 lensDistortParametersEnd;
uniform bool lensDistortChromab;

// vignetting
uniform bool enableVignette;
uniform float vignettePower;

uniform float hueShift;

#include "tonemap.glsl"
#include "mathUtils.h"
#include "oklabConversions.h.glsl"

// buffer for the color histograms
layout( binding = 1, std430 ) buffer colorHistograms {
	uint valuesR[ 256 ];
	uint maxValueR;
	uint valuesG[ 256 ];
	uint maxValueG;
	uint valuesB[ 256 ];
	uint maxValueB;
	uint valuesL[ 256 ];
	uint maxValueL;
};
// updating the histogram buffer
uniform bool updateHistogram;
void UpdateHistograms( vec4 color ) {
	if ( updateHistogram ) {
		uvec4 binCrements = uvec4(
			uint( saturate( color.r ) * 255.0f ),
			uint( saturate( color.g ) * 255.0f ),
			uint( saturate( color.b ) * 255.0f ),
			uint( saturate( dot( color.rgb, vec3( 0.299f, 0.587f, 0.114f ) ) ) * 255.0f )
		);
		atomicMax( maxValueR, atomicAdd( valuesR[ binCrements.r ], 1 ) + 1 );
		atomicMax( maxValueG, atomicAdd( valuesG[ binCrements.g ], 1 ) + 1 );
		atomicMax( maxValueB, atomicAdd( valuesB[ binCrements.b ], 1 ) + 1 );
		atomicMax( maxValueL, atomicAdd( valuesL[ binCrements.a ], 1 ) + 1 );
	}
}

void main() { // This is where tonemapping etc will be happening on the accumulated image
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	vec4 color = vec4( 0.0f );

	if ( enableLensDistort == true ) { // lens distort + chromatic abberation
		for ( int i = 0; i < lensDistortNumSamples; i++ ) {
			float interpolationValue = ( i + 0.5f ) / float( lensDistortNumSamples );
			vec3 interpolatedParameters = mix( lensDistortParametersStart, lensDistortParametersEnd, interpolationValue );

			vec3 colorWeight = vec3( 1.0f );
			if ( lensDistortChromab ) {
				const float i = interpolationValue * ( 3.1415f / 2.0f );
				// colorWeight = vec3( sin( i ), sin( i * 2.0f ), cos( i ) );
				colorWeight = vec3( sin( i ), sin( i * 2.0f ), cos( i ) ) * 1.618f; // why is this compensation value needed?
			}

		// sample the colors out at points along the interpolated Brown-Conrady distort
			// pixel coordinate in UV space
			const vec2 normalizedPosition = vec2( float( location.x ) / imageSize( accumulatorColor ).x, float( location.y ) / imageSize( accumulatorColor ).y );
			vec2 remapped = ( normalizedPosition * 2.0f ) - vec2( 1.0f );
			const float r2 = remapped.x * remapped.x + remapped.y * remapped.y;

			// interpolatedParameters is k1, k2, t1
			remapped *= 1.0f + ( interpolatedParameters.x * r2 ) * ( interpolatedParameters.y * r2 * r2 );
			// remapped *= 1.0f + ( iterationK1 * r2 ) + ( iterationK2 * r2 * r2 ); // interesting

			if ( interpolatedParameters.z != 0.0f ) { // tangential distortion
				const float angle = r2 * interpolatedParameters.z;
				remapped = mat2( cos( angle ), -sin( angle ), sin( angle ), cos( angle ) ) * remapped;
			}

			remapped = remapped * 0.5f + vec2( 0.5f ); // restore back to the normalized space
			if ( lensDistortNormalize ) { // scale about the image center, to keep the image close to the same size
				const float normalizeFactor = ( abs( interpolatedParameters.x ) < 1.0f ) ? ( 1.0f - abs( interpolatedParameters.x ) ) : ( 1.0f / ( interpolatedParameters.x + 1.0f ) );
				remapped = remapped * normalizeFactor - ( normalizeFactor * 0.5f ) + vec2( 0.5f );
			}

			// color += imageLoad( accumulatorColor, ivec2( remapped * imageSize( accumulatorColor ).xy ) ) * vec4( colorWeight, 1.0f );
			color += texture( accumulatorColorTex, remapped ) * vec4( colorWeight, 1.0f );
		}
		color = color / lensDistortNumSamples;
	} else {
		color = imageLoad( accumulatorColor, location );
	}

	// hue shifting
	color.rgb = oklch_to_srgb( srgb_to_oklch( color.rgb ) + vec3( 0.0f, 0.0f, hueShift ) );

	// SSAO, since we have access to depth and normals

	// potentially somet kind of denoising, in the future

	// ...

	// exposure adjustment
	color.rgb *= postExposure;

	// vignetting
	if ( enableVignette ) {
		vec2 uv = ( location + vec2( 0.5f ) ) / vec2( imageSize( tonemappedResult ).xy );
		uv *= 1.0f - uv.yx;
		color.rgb *= pow( uv.x * uv.y, vignettePower );
	}

	// tonemapping
	color.rgb = Tonemap( tonemapMode, colorTempAdjust * ( saturation * color.rgb ) );

	// gamma
	color.rgb = GammaCorrect( gamma, color.rgb );

	// dithering

	// update the histograms, with this value - but only if this invocation is on the image
	if ( all( lessThan( gl_GlobalInvocationID.xy, imageSize( tonemappedResult ) ) ) ) {
		UpdateHistograms( color );
	}

	imageStore( tonemappedResult, location, color );
}