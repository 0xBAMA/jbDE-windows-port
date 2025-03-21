#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, rgba8ui ) uniform uimage2D displayTexture;
#include "VoraldoCompatibility/tonemap.glsl" // tonemapping curves

uniform int tonemapMode;
uniform float gamma;
uniform vec3 colorTempAdjust;
uniform int frameNumber;

vec4 linearInterpolatedSample ( vec2 location ) {
	const vec2 fractionalPart = fract( location );
	const vec2 wholePart = floor( location );
	const vec4 sample0 = imageLoad( accumulatorTexture, ivec2( wholePart ) );
	const vec4 sample1 = imageLoad( accumulatorTexture, ivec2( wholePart ) + ivec2( 1, 0 ) );
	const vec4 sample2 = imageLoad( accumulatorTexture, ivec2( wholePart ) + ivec2( 0, 1 ) );
	const vec4 sample3 = imageLoad( accumulatorTexture, ivec2( wholePart ) + ivec2( 1, 1 ) );
	const vec4 xBlend0 = mix( sample0, sample1, fractionalPart.x );
	const vec4 xBlend1 = mix( sample2, sample3, fractionalPart.x );
	return mix( xBlend0, xBlend1, fractionalPart.y );
}

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// compute SSFACTOR
	vec2 scalar = vec2( imageSize( accumulatorTexture ) ) / vec2( imageSize( displayTexture ) );
	vec2 samplePosition = scalar * vec2( loc );

	// TODO: take a couple samples, jittered with blue noise
	vec4 jitter[ 4 ];
	ivec2 position = ivec2( gl_GlobalInvocationID.xy ) % ivec2( imageSize( blueNoise ) );
	jitter[ 0 ] = imageLoad( blueNoise, position ) / 255.0;
	position = ivec2( gl_GlobalInvocationID.xy + ivec2( 256 ) ) % ivec2( imageSize( blueNoise ) );
	jitter[ 1 ] = imageLoad( blueNoise, position ) / 255.0;
	position = ivec2( gl_GlobalInvocationID.xy + ivec2( 127 ) ) % ivec2( imageSize( blueNoise ) );
	jitter[ 2 ] = imageLoad( blueNoise, position ) / 255.0;
	position = ivec2( gl_GlobalInvocationID.xy + ivec2( 383 ) ) % ivec2( imageSize( blueNoise ) );
	jitter[ 3 ] = imageLoad( blueNoise, position ) / 255.0;

	vec4 originalValue = vec4( 0.0 );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 0 ].xy );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 0 ].zw );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 1 ].xy );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 1 ].zw );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 2 ].xy );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 2 ].zw );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 3 ].xy );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 3 ].zw );
	originalValue /= 8.0;

	// vec3 color = tonemap( tonemapMode, colorTempAdjust * originalValue.xyz * 255.0 );
	vec3 color = tonemap( tonemapMode, colorTempAdjust * originalValue.xyz );
	color = gammaCorrect( gamma, color );
	// uvec4 tonemappedValue = uvec4( uvec3( color * 255.0 ), originalValue.a * 255 );
	uvec4 tonemappedValue = uvec4( uvec3( color * 255.0 ), 255 );

	imageStore( displayTexture, loc, tonemappedValue );
}
