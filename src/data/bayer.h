#include <vector>

// analytic evaluation of bayer matrices... interesting
// https://www.shadertoy.com/view/4ssfWM
// https://www.shadertoy.com/view/XtV3RG // this one looks really promising
// https://www.shadertoy.com/view/ltsyWl // usage for dithering

static inline const std::vector< uint8_t > BayerData ( const int dimension ) {
	ZoneScoped;
	if ( dimension == 2 ) {
		std::vector< uint8_t > pattern2 = {
			0, 128,
			192, 64
		};
		return pattern2;
	} else if ( dimension == 4 ) {
		std::vector< uint8_t > pattern4 = {
			0,  8,  2,  10,	/* values begin scaled to the range 0..15 */
			12, 4,  14, 6,	/* so they need to be rescaled by 16 */
			3,  11, 1,  9,
			15, 7,  13, 5 };

		for ( auto &x : pattern4 )
			x *= 16;

		return pattern4;
	} else if ( dimension == 8 ) {
		std::vector< uint8_t > pattern8 = {
			0, 32,  8, 40,  2, 34, 10, 42,   /* 8x8 Bayer ordered dithering  */
			48, 16, 56, 24, 50, 18, 58, 26,  /* pattern. Each input pixel */
			12, 44,  4, 36, 14, 46,  6, 38,  /* starts scaled to the 0..63 range */
			60, 28, 52, 20, 62, 30, 54, 22,  /* before looking in this table */
			3, 35, 11, 43,  1, 33,  9, 41,   /* to determine the action. */
			51, 19, 59, 27, 49, 17, 57, 25,
			15, 47,  7, 39, 13, 45,  5, 37,
			63, 31, 55, 23, 61, 29, 53, 21 };

		for ( auto &x : pattern8 )
			x *= 4;

		return pattern8;
	} else {
		return std::vector< uint8_t >{ 0 };
	}
}

static inline std::vector< uint8_t > Make4Channel( const std::vector< uint8_t > &input ) {
	std::vector< uint8_t > data;
	for ( unsigned int i = 0; i < input.size(); i++ ) {
		for ( unsigned int n = 0; n < 4; n++ ) {
			data.push_back( input[ i ] );
		}
	}
	return data;
}