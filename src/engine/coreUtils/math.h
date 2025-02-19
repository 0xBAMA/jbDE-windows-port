#pragma once
#ifndef MATH_H
#define MATH_H

// for formatted output
inline std::string fixedWidthNumberString ( int32_t value, int32_t width = 5, const char fill = '0' ) {
	return string( width - std::min( width, ( int ) to_string( value ).length() ), fill ) + to_string( value );
}

inline std::string fixedWidthNumberStringF ( float value, int32_t width = 5, const char fill = ' ' ) {
	return string( width - std::min( width, ( int ) to_string( value ).length() ), fill ) + to_string( value );
}

// remap the value from [inLow..inHigh] to [outLow..outHigh]
inline float RangeRemap ( float value, float inLow, float inHigh, float outLow, float outHigh ) {
	return outLow + ( value - inLow ) * ( outHigh - outLow ) / ( inHigh - inLow );
}

inline std::string GetWithThousandsSeparator ( size_t value ) {
	std::string s = std::to_string( value );
	int n = s.length() - 3;
	int end = ( value >= 0 ) ? 0 : 1; // Support for negative numbers
	while ( n > end ) {
		s.insert( n, "," );
		n -= 3;
	}
	return s;
}

// fast integer power using a LUT - from https://gist.github.com/orlp/3551590
inline int64_t intPow ( int64_t base, uint8_t exp ) {
	static const uint8_t highest_bit_set[] = {
		0, 1, 2, 2, 3, 3, 3, 3,
		4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5,
		5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 255, // anything past 63 is a guaranteed overflow with base > 1
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255,
	};

	int64_t result = 1;
	switch ( highest_bit_set[ exp ] ) {
	case 255: // we use 255 as an overflow marker and return 0 on overflow/underflow
		if ( base == 1 ) {
			return 1;
		}
		if ( base == -1 ) {
			return 1 - 2 * ( exp & 1 );
		}
		return 0;
	case 6:
		if ( exp & 1 ) result *= base;
		exp >>= 1;
		base *= base;
	case 5:
		if ( exp & 1 ) result *= base;
		exp >>= 1;
		base *= base;
	case 4:
		if ( exp & 1 ) result *= base;
		exp >>= 1;
		base *= base;
	case 3:
		if ( exp & 1 ) result *= base;
		exp >>= 1;
		base *= base;
	case 2:
		if ( exp & 1 ) result *= base;
		exp >>= 1;
		base *= base;
	case 1:
		if ( exp & 1 ) result *= base;
	default:
		return result;
	}
}

inline uint32_t nextPowerOfTwo ( uint32_t n ) {
	if ( n == 0 )
		return 1;

	// Find the position of the most significant bit
	int msb = 0;
	while ( n >>= 1 ) { msb++; }

	// Return the next power of 2
	return 1 << ( msb + 1 );
}


// // from fadaaszhi on GP discord 11/8/2023
// vec2 randHex() {
// #ifdef ANALYTIC
// 	float x = rand1f() * 2.0 - 1.0;
// 	float a = sqrt(3.0) - sqrt(3.0 - 2.25 * abs(x));
// 	return vec2(sign(x) * a, (rand1f() * 2.0 - 1.0) * (1.0 - a / sqrt(3.0)));
// #else
// 	while (true) {
// 		float x = (rand1f() - 0.5) * sqrt(3.0);
// 		float y = rand1f() * 2.0 - 1.0;
		
// 		if (abs(x) < min(sqrt(0.75), sqrt(3.0) * (1.0 - abs(y)))) {
// 			return vec2(x, y);
// 		}
// 	}
// #endif
// }

// other curves? there were some things
	// Bias and Gain Functions - https://arxiv.org/abs/2010.09714
	// iq's Usful Functions - https://iquilezles.org/articles/functions/
	// Easing Functions - https://easings.net/

#endif // MATH_H