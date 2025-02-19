// ============================================================================
// from iq: https://www.shadertoy.com/view/llGcDm

// adapted from https://en.wikipedia.org/wiki/Hilbert_curve
int hilbert( ivec2 p, int level ) {
	int d = 0;
	for ( int k = 0; k < level; k++ ) {
		int n = level - k - 1;
		ivec2 r = ( p >> n ) & 1;
		d += ( ( 3 * r.x) ^ r.y ) << ( 2 * n );
		if ( r.y == 0 ) {
			if ( r.x == 1 ) {
				p = ( 1 << n ) - 1 - p;
			}
			p = p.yx;
		}
	}
	return d;
}

// adapted from  https://en.wikipedia.org/wiki/Hilbert_curve
ivec2 ihilbert ( int i, int level ) {
	ivec2 p = ivec2( 0, 0 );
	for( int k = 0; k < level; k++ ) {
		ivec2 r = ivec2( i >> 1, i ^ ( i >> 1 ) ) & 1;
		if ( r.y == 0 ) {
			if ( r.x== 1 ) {
				p = ( 1 << k ) - 1 - p;
			}
			p = p.yx;
		}
		p += r << k;
		i >>= 2;
	}
	return p;
}

// ============================================================================
// Z-curve / Morton code from Fabrice https://www.shadertoy.com/view/4sscDn
// refs : https://en.wikipedia.org/wiki/Z-order_curve
//        https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
int MASKS[] = int[] ( 0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF );

int xy2z ( vec2 U ) {	// --- grid location to curve index
	ivec2 I = ivec2( U );
	int n = 8;
	for ( int i = 3; i >= 0; i-- )
		I = ( I | ( I << n ) ) & MASKS[ i ],
		n /= 2;
	return I.x | ( I.y << 1 );
}

ivec2 z2xy ( int z ) {	// --- curve index to grid location 
	int n = 1;
	ivec2 I = ivec2( z, z >> 1 ) & MASKS[ 0 ];
	for ( int i = 1; i <= 4; i++ )
		I = ( I | ( I >>  n ) ) & MASKS[ i ],
		n *= 2;
	return I;
}
