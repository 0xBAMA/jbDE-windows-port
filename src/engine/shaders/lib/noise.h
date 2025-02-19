
// ==============================================================================================
// Hash by David_Hoskins
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uvec2(UI0, UI1)
#define UI3 uvec3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))

vec3 hash33( vec3 p ) {
	uvec3 q = uvec3( ivec3( p ) ) * UI3;
	q = ( q.x ^ q.y ^ q.z )*UI3;
	return -1.0 + 2.0 * vec3( q ) * UIF;
}

// Gradient noise by iq (modified to be tileable)
float gradientNoise( vec3 x, float freq ) {
    // grid
    vec3 p = floor( x );
    vec3 w = fract( x );
    
    // quintic interpolant
    vec3 u = w * w * w * ( w * ( w * 6.0 - 15.0 ) + 10.0 );

    // gradients
    vec3 ga = hash33( mod( p + vec3( 0.0, 0.0, 0.0 ), freq ) );
    vec3 gb = hash33( mod( p + vec3( 1.0, 0.0, 0.0 ), freq ) );
    vec3 gc = hash33( mod( p + vec3( 0.0, 1.0, 0.0 ), freq ) );
    vec3 gd = hash33( mod( p + vec3( 1.0, 1.0, 0.0 ), freq ) );
    vec3 ge = hash33( mod( p + vec3( 0.0, 0.0, 1.0 ), freq ) );
    vec3 gf = hash33( mod( p + vec3( 1.0, 0.0, 1.0 ), freq ) );
    vec3 gg = hash33( mod( p + vec3( 0.0, 1.0, 1.0 ), freq ) );
    vec3 gh = hash33( mod( p + vec3( 1.0, 1.0, 1.0 ), freq ) );
    
    // projections
    float va = dot( ga, w - vec3( 0.0, 0.0, 0.0 ) );
    float vb = dot( gb, w - vec3( 1.0, 0.0, 0.0 ) );
    float vc = dot( gc, w - vec3( 0.0, 1.0, 0.0 ) );
    float vd = dot( gd, w - vec3( 1.0, 1.0, 0.0 ) );
    float ve = dot( ge, w - vec3( 0.0, 0.0, 1.0 ) );
    float vf = dot( gf, w - vec3( 1.0, 0.0, 1.0 ) );
    float vg = dot( gg, w - vec3( 0.0, 1.0, 1.0 ) );
    float vh = dot( gh, w - vec3( 1.0, 1.0, 1.0 ) );
	
    // interpolation
    return va + 
           u.x * ( vb - va ) + 
           u.y * ( vc - va ) + 
           u.z * ( ve - va ) + 
           u.x * u.y * ( va - vb - vc + vd ) + 
           u.y * u.z * ( va - vc - ve + vg ) + 
           u.z * u.x * ( va - vb - ve + vf ) + 
           u.x * u.y * u.z * ( -va + vb + vc - vd + ve - vf - vg + vh );
}

float perlinfbm( vec3 p, float freq, int octaves ) {
    float G = exp2( -0.85 );
    float amp = 1.0;
    float noise = 0.0;
    for ( int i = 0; i < octaves; ++i ) {
        noise += amp * gradientNoise( p * freq, freq );
        freq *= 2.0;
        amp *= G;
    }
    return noise;
}

float freq = 19.0;
const int octaves = 2;
float noise( vec3 p ) {
	return perlinfbm( p, freq, octaves );
}
vec3 curlNoise( vec3 p ) {
// my curl noise: https://www.shadertoy.com/view/ssKczW
	vec3 col;

	// general structure from: https://al-ro.github.io/projects/embers/
	float n1, n2, a, b;
	vec2 epsilon = vec2( 0.1, 0.0 );
	n1 = noise( p + epsilon.yxy );
	n2 = noise( p - epsilon.yxy );
	a = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	n1 = noise( p + epsilon.yyx );
	n2 = noise( p - epsilon.yyx );
	b = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	col.x = a - b;

	n1 = noise( p + epsilon.yyx );
	n2 = noise( p - epsilon.yyx );
	a = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	n1 = noise( p + epsilon.xyy );
	n2 = noise( p - epsilon.xyy );
	b = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	col.y = b - a;

	n1 = noise( p + epsilon.xyy );
	n2 = noise( p - epsilon.xyy );
	a = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	n1 = noise( p + epsilon.yxy );
	n2 = noise( p - epsilon.yxy );
	b = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	col.z = a - b;

	// Output to screen
	// return normalize( col );
	return col;
}