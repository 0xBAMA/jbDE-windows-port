#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// texture for the atomic writes
layout( r32ui ) uniform uimage2D vectorscopeImage;

// texture for the composited output
layout( rgba8 ) uniform image2D compositedResult;

// buffer for max pixel count
layout( binding = 5, std430 ) buffer perColumnMins { uint maxCount; };

// #include "hsvConversions.h"
#include "yuvConversions.h"
#include "mathUtils.h"

vec3 saturationBoost( vec3 inputColor, float boost ) {
	const float s = boost;
	const float oms = 1.0f - s;
	vec3 weights = vec3( 0.3086f, 0.6094f, 0.0820f );	// "improved" luminance vector
	mat3 saturationMatrix = mat3(
		oms * weights.r + s,	oms * weights.r,		oms * weights.r,
		oms * weights.g,		oms * weights.g + s,	oms * weights.g,
		oms * weights.b,		oms * weights.b,		oms * weights.b + s
	);
	return saturationMatrix * inputColor;
}

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	const vec2 normalizedSpace = ( vec2( loc ) + vec2( 0.5f ) ) / imageSize( compositedResult ) - vec2( 0.5f );

	vec4 color = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	const float radius = sqrt( dot( normalizedSpace, normalizedSpace ) );
	if ( radius < 0.5f ) {
		// todo: gridlines, background
		// smoothstep for edge falloff
		color.rgb = vec3( 0.01618f * smoothstep( 0.01f, 0.0f, smoothstep( 0.01f, 0.02f, mod( radius, 0.1f ) ) ) );

		// find the pixel count value, normalize by dividing by maxCount
		const float value = float( imageLoad( vectorscopeImage, loc ).r ) / float( maxCount );

		// find the color for this pixel, from position -> hsv color, set l=value -> rgb
		// color.rgb = hsv2rgb( vec3( atan( normalizedSpace.x, normalizedSpace.y ) / 6.28f, radius * 2.0f, value ) );

		// from https://www.shadertoy.com/view/4dcSRN
		// vec3 yuv = vec3( RangeRemapValue( value, 0.0f, 1.0f, 0.5f, 1.0f ), -normalizedSpace );
		vec3 yuv = vec3( 0.5, -normalizedSpace );
		const float r = ( 1.0f - abs( yuv.x - 0.5f ) * 2.0f ) * 0.7071f;
		yuv.yz *= -r;
		yuv = clamp( yuv, vec3( 0.0f, -0.5f, -0.5f ), vec3( 1.0f, 0.5f, 0.5f ) );
		color.rgb += pow( saturationBoost( ycocg_rgb( yuv ) * value, 3.5f ), vec3( 0.618f ) ) * 10.0f;

		// oklab is probably another one that will be worth trying

	} else {
		color.a = smoothstep( 0.5f, 0.4f, radius );
	}

	imageStore( compositedResult, loc, color );
}