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