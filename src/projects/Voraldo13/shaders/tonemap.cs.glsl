#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba16f ) uniform image2D source;
layout( binding = 1, rgba8ui ) uniform uimage2D displayTexture;
layout( binding = 2, rgba8ui ) uniform uimage2D blueNoise;
#include "VoraldoCompatibility/tonemap.glsl" // tonemapping curves

uniform int tonemapMode;
uniform float gamma;
uniform float postExposure;
uniform mat3 saturation;
uniform vec3 colorTempAdjust;
uniform ivec2 blueNoiseOffset;

// vignetting
uniform bool enableVignette;
uniform float vignettePower;

vec4 blueNoiseRef( ivec2 pos ) {
	pos += blueNoiseOffset;
	pos.x = pos.x % imageSize( blueNoise ).x;
	pos.y = pos.y % imageSize( blueNoise ).y;
	return imageLoad( blueNoise, pos ) / 255.0f;
}

vec4 linearInterpolatedSample ( vec2 location ) {
	const vec2 fractionalPart = fract( location );
	const vec2 wholePart = floor( location );
	const vec4 sample0 = imageLoad( source, ivec2( wholePart ) );
	const vec4 sample1 = imageLoad( source, ivec2( wholePart ) + ivec2( 1, 0 ) );
	const vec4 sample2 = imageLoad( source, ivec2( wholePart ) + ivec2( 0, 1 ) );
	const vec4 sample3 = imageLoad( source, ivec2( wholePart ) + ivec2( 1, 1 ) );
	const vec4 xBlend0 = mix( sample0, sample1, fractionalPart.x );
	const vec4 xBlend1 = mix( sample2, sample3, fractionalPart.x );
	return mix( xBlend0, xBlend1, fractionalPart.y );
}

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// compute SSFACTOR - todo: this can just be passed in
	vec2 scalar = vec2( imageSize( source ) ) / vec2( imageSize( displayTexture ) );
	vec2 samplePosition = scalar * vec2( loc );

	// take a couple samples, jittered with blue noise
	vec4 jitter[ 4 ];
	jitter[ 0 ] = blueNoiseRef( loc + 64  );
	jitter[ 1 ] = blueNoiseRef( loc + 128 );
	jitter[ 2 ] = blueNoiseRef( loc + 256 );
	jitter[ 3 ] = blueNoiseRef( loc + 383 );

	vec4 originalValue = vec4( 0.0f );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 0 ].xy );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 0 ].zw );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 1 ].xy );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 1 ].zw );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 2 ].xy );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 2 ].zw );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 3 ].xy );
	originalValue += linearInterpolatedSample( samplePosition + jitter[ 3 ].zw );
	originalValue *= postExposure;
	originalValue /= 8.0f;

	// vignetting
	if ( enableVignette ) {
		vec2 uv = ( vec2( loc ) + vec2( 0.5f ) ) / vec2( imageSize( displayTexture ) );
		uv *= 1.0f - uv.yx;
		originalValue.rgb *= pow( uv.x * uv.y, vignettePower );
	}

	// small amount of functional ( not aesthetic ) dither, for banding issues incurred from the vignette
		// this maybe needs a toggle
	originalValue.rgb = originalValue.rgb + blueNoiseRef( loc ).rgb * 0.005f;

	vec3 color = tonemap( tonemapMode, colorTempAdjust * ( saturation * originalValue.xyz ) );
	color = gammaCorrect( gamma, color );
	uvec4 tonemappedValue = uvec4( uvec3( color * 255.0 ), 255 );

	imageStore( displayTexture, loc, tonemappedValue );
}
