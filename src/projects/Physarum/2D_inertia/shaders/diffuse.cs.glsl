#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// horizontal and vertical occur separately
uniform int separableBlurMode; // 0 is horizontal, 1 is vertical
uniform float radius;

// handle bindings CPU side, so we can treat them agnostically here
// source texture
layout ( binding = 0 ) uniform sampler2D sourceTex;

// destination image
layout ( binding = 1, r32f ) uniform image2D destTex;

#include "mathUtils.h"

// worked out separable blur: https://www.shadertoy.com/view/33GXzh
float blurWeight ( const float pos ) {
	// divide by number of taps included here instead of in the loop... proportional to the filter radius (ignores a scale factor... still ok?)
	float normalizationTerm = radius * sqrtpi; // integral of distribution is sqrt(pi/a), we precompute this constant
	float gaussianWeight = exp( -( pos * pos ) / ( 2.0f * radius * radius ) );
	return gaussianWeight / normalizationTerm;
}

float blurResult ( vec2 uv ) {
	float val = 0.0f;

	// 3 * radius based on observation that it's within some reasonable threshold of zero by that point (3 stdev)
		// also, 99.8% of the integral is inside of 3 standard deviations
	for ( float offset = -3.0f * radius - 0.5f; offset < 3.0f * radius + 0.5f; offset++ )
	// https://www.shadertoy.com/view/Xd33Rf note use of texel border sampling to double effective bandwidth
		val += blurWeight( offset ) * texture( sourceTex, ( uv + ( ( separableBlurMode == 0 ) ?
			vec2( offset, 0.0f ) : vec2( 0.0f, offset ) ) / textureSize( sourceTex, 0 ).xy ) ).r;

	// if ( separableBlurMode == 1 ) { val *= 0.5f; }
	if ( separableBlurMode == 1 ) { val *= 0.46f; }
	return val;
}

void main () {
	// implementing horizontal and vertical modes here, need to control from CPU
	// only apply the "decay" during the second pass, since we want to do a mix with a clean blurred image result

	vec2 p = vec2( gl_GlobalInvocationID.xy + 0.5f ) / imageSize( destTex ).xy;
	imageStore( destTex, ivec2( gl_GlobalInvocationID.xy ), vec4( blurResult( p ) ) );
}
