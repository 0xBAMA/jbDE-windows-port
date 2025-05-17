#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2 ) uniform sampler2D selectedTexture;

uniform float angle;
uniform float scale;
uniform vec2 offset;

const float SRGB_GAMMA = 1.0f / 2.2f;
const float SRGB_INVERSE_GAMMA = 2.2f;
const float SRGB_ALPHA = 0.055f;

// Converts a single linear channel to srgb
float linear_to_srgb ( float channel ) {
	if ( channel <= 0.0031308f )
		return 12.92f * channel;
	else
		return ( 1.0f + SRGB_ALPHA ) * pow( channel, 1.0f / 2.4f ) - SRGB_ALPHA;
}

// Converts a single srgb channel to rgb
float srgb_to_linear ( float channel ) {
	if ( channel <= 0.04045f )
		return channel / 12.92f;
	else
		return pow( ( channel + SRGB_ALPHA ) / ( 1.0f + SRGB_ALPHA ), 2.4f );
}

// Converts a linear rgb color to a srgb color (exact, not approximated)
vec3 rgb_to_srgb ( vec3 rgb ) {
	return vec3(
		linear_to_srgb( rgb.r ),
		linear_to_srgb( rgb.g ),
		linear_to_srgb( rgb.b ) );
}

// Converts a srgb color to a linear rgb color (exact, not approximated)
vec3 srgb_to_rgb ( vec3 srgb ) {
	return vec3(
		srgb_to_linear( srgb.r ),
		srgb_to_linear( srgb.g ),
		srgb_to_linear( srgb.b ) );
}

void main () {
	// screen UV
	vec2 iS = imageSize( accumulatorTexture ).xy;
	vec2 centeredUV = gl_GlobalInvocationID.xy / iS - vec2( 0.5f );
	centeredUV.x *= ( iS.x / iS.y );
	centeredUV.y *= -1.0f;

	// rotate and scale, then position
	mat2 rot = mat2(
		cos( -angle ), -sin( -angle ),
		sin( -angle ), cos( -angle )
	);

	float ratio = float( textureSize( selectedTexture, 0 ).x ) / float( textureSize( selectedTexture, 0 ).y );
	mat2 scaleMat = mat2( scale, 0.0f, 0.0f, scale * ratio );

	vec2 texUV = scaleMat * centeredUV;
	texUV -= offset;
	texUV = rot * texUV;

	// sample the texture
	vec4 color = texture( selectedTexture, texUV + vec2( 0.5f ) );

	/*
	if ( texUV.x > 0.0f && texUV.y > 0.0f && texUV.x < 1.0f && texUV.y < 1.0f ) {
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( texUV.xy, 0.0f, 1.0f ) );
	}
	*/

	if ( color.a != 0.0f ) {
		// blend with the accumulator contents...
		vec4 previousColor = imageLoad( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ) );
		previousColor.rgb = rgb_to_srgb( previousColor.rgb );

		// alpha blending, new sample over running color
		float alphaSquared = pow( color.a, 2.0f );
		previousColor.a = max( alphaSquared + previousColor.a * ( 1.0f - alphaSquared ), 0.001f );
		previousColor.rgb = color.rgb * alphaSquared + previousColor.rgb * previousColor.a * ( 1.0f - alphaSquared );
		previousColor.rgb /= previousColor.a;

		color.rgb = srgb_to_rgb( previousColor.rgb );
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( color.rgb, 1.0f ) );
	}
}