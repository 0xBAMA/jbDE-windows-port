#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// horizontal and vertical occur separately
uniform int separableBlurMode; // 0 is horizontal, 1 is vertical
uniform float radius;

// handle bindings CPU side, so we can treat them agnostically here
// source texture

// destination image

#include "mathUtils.h"

// worked out separable blur: https://www.shadertoy.com/view/33GXzh
float blurWeight ( const float pos ) {
	// divide by number of taps included here instead of in the loop... this drops a scalar, but brightness is reasonable
	float normalizationTerm = radius * sqrt( pi ); // integral of distribution is sqrt(pi/a), a=1 because we dropped it
	float gaussianWeight = exp( -( pos * pos ) / ( 2.0f * radius * radius ) );
	return gaussianWeight / normalizationTerm;
}

float blurResult ( vec2 uv ) {
	float val = 0.0f;
	// 3 * radius based on observation that it's within some reasonable threshold of zero by that point (3 stdev)
	for ( float offset = -3.0f * radius - 0.5f; offset < 3.0f * radius + 0.5f; offset++ ) {
		// https://www.shadertoy.com/view/Xd33Rf note use of texel border sampling to double effective bandwidth
		val += blurWeight( offset ) * texture( substrateTex, ( fragCoord + ( ( separableBlurMode == 1 ) ? vec2( offset, 0.0f ) : vec2( 0.0f, offset ) ) / textureSize( substrateTex, 0 ).xy ) ).r;

	return val;
}

void main () {
	// implementing horizontal and vertical modes here, need to control from CPU

	// only apply the "decay" during the second pass, since we want to do a mix with a clean blurred image result

}
