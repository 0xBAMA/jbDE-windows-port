// https://blog.demofox.org/2012/09/24/bias-and-gain-are-your-friend/
float biasValue ( float value, float bias ) {
	return ( value / ( ( ( ( 1.0f / bias ) - 2.0f ) * ( 1.0f - value ) ) + 1.0f ) );
}

float gainValue ( float value, float gain ) {
	if ( value < 0.5f ) {
		return biasValue( value * 2.0f, gain ) / 2.0f;
	} else {
		return biasValue( value * 2.0f - 1.0f, 1.0f - gain ) / 2.0f + 0.5f;
	}
}

// Bias and Gain generalization
// https://arxiv.org/abs/2010.09714
// https://arxiv.org/pdf/2010.09714
float biasGain ( float value, float slope, float threshold ) {
	// epsilon value to prevent divide by zero
	const float eps = 1e-10f;
	if ( value < threshold ) {
		return ( value * threshold ) / ( value + slope * ( threshold - value ) + eps );
	} else {
		return 1.0f + ( ( 1.0f - threshold ) * ( value - 1.0f ) ) / ( 1.0f - value - slope * ( threshold - value ) + eps );
	}
}