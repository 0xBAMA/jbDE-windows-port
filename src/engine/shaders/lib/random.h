// random utilites
uint seed = 0;
uint wangHash () {
	seed = uint( seed ^ uint( 61 ) ) ^ uint( seed >> uint( 16 ) );
	seed *= uint( 9 );
	seed = seed ^ ( seed >> 4 );
	seed *= uint( 0x27d4eb2d );
	seed = seed ^ ( seed >> 15 );
	return seed;
}

float NormalizedRandomFloat () {
	return float( wangHash() ) / 4294967296.0f;
}

uint rng_state;
uint PCGHash() {
    rng_state = rng_state * 747796405u + 2891336453u;
    uint state = rng_state;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}


#ifndef PI_DEFINED
#define PI_DEFINED
const float pi = 3.14159265358979323846f;
const float tau = 2.0f * pi;
const float sqrtpi = 1.7724538509f;
#endif

#ifndef saturate
#define saturate(x) clamp(x, 0, 1)
#endif

vec3 RandomUnitVector () {
	float z = NormalizedRandomFloat() * 2.0f - 1.0f;
	float a = NormalizedRandomFloat() * 2.0f * pi;
	float r = sqrt( 1.0f - z * z );
	float x = r * cos( a );
	float y = r * sin( a );
	return vec3( x, y, z );
}

// note that this is not uniformly distributed, ring shaped
vec2 RandomInUnitDisk () {
	return RandomUnitVector().xy;
}

float NormalDistributionRand() {
	// https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
	float U1 = NormalizedRandomFloat();
	float U2 = NormalizedRandomFloat();
	return sqrt( -2.0f * log( U1 ) ) * cos( tau * U2 );
}

vec3 cosWeightedRandomHemisphereDirection( const vec3 n ) {
	vec2 rv2 = vec2( NormalizedRandomFloat(), NormalizedRandomFloat() );

	vec3  uu = normalize( cross( n, vec3( 0.0f, 1.0f, 1.0f ) ) );
	vec3  vv = normalize( cross( uu, n ) );

	float ra = sqrt( rv2.y );
	float rx = ra * cos( 6.2831f * rv2.x );
	float ry = ra * sin( 6.2831f * rv2.x );
	float rz = sqrt( 1.0f - rv2.y );
	vec3  rr = vec3( rx * uu + ry * vv + rz * n );

	//return normalize(n + (hash3()*vec3(2.0) - vec3(1.0)));
	return normalize( rr );
}


// unit circle, no edge bias
vec2 randCircle ( float rand1, float rand2 ) {
	float u = 2.0 * pi * rand1;  // θ
	float v = sqrt( rand2 );  // r
	return v * vec2( cos( u ), sin( u ) );
}

vec2 CircleOffset () {
	return randCircle( NormalizedRandomFloat(), NormalizedRandomFloat() );
}

// size, corner rounding radius
float sdHexagon ( vec2 p, float s, float r ) {
	const vec3 k = vec3( -0.866025404f, 0.5f, 0.577350269f );
	p = abs( p );
	p -= 2.0f * min( dot( k.xy, p ), 0.0f ) * k.xy;
	p -= vec2( clamp( p.x, -k.z * s, k.z * s ), s );
	return length( p ) * sign( p.y ) - r;
}

vec2 RejectionSampleHexOffset () {
	vec2 cantidate;
	// generate points in a unit square, till one is inside a hexagon
	while ( sdHexagon( cantidate = vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) - vec2( 0.5f ), 0.3f, 0.0f ) > 0.0f );
	return 2.0f * cantidate;
}

// uniform sampled hex from fadaaszhi on GP discord
vec2 UniformSampleHexagon ( vec2 u ) {
	u = 2.0f * u - 1.0f;
	float a = sqrt( 3.0f ) - sqrt( 3.0f - 2.25f * abs( u.x ) );
	return vec2( sign( u.x ) * a, u.y * ( 1.0f - a / sqrt( 3.0f ) ) );
}

vec2 UniformSampleHexagon () {
	return UniformSampleHexagon( vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) );
}

// more of these uniform remapping functions
// https://www.shadertoy.com/view/7lGXWK

// x²-|x|y+y² < 1
vec2 randHeart( float rand1, float rand2 ) {
	float u = 2.0f * pi * rand1;  // θ
	float v = sqrt( rand2 );  // r
	vec2 c = v * vec2( cos( u ), sin( u ) );  // unit circle
	c = mat2( 1.0f, 1.0f, -0.577f, 0.577f ) * c;  // ellipse
	if ( c.x < 0.0f ) c.y = -c.y;  // mirror
	return c;
}

vec2 HeartOffset () {
	return randHeart( NormalizedRandomFloat(), NormalizedRandomFloat() );
}

// rosette shape
// r(θ) = cos(nθ)
vec2 randRose( float n, float rand1, float rand2 ) {
	// integrate cos(nθ)², split into n intervals and inverse in [-π/2n,π/2n)
	float u = pi * rand1;  // integral in [0,2π) is π
	float v = sqrt( rand2 );  // r
	float ui = pi / n * floor( 2.0f * n / pi * u );  // center of each "petal"
	float uf = mod( u, pi / ( 2.0f * n ) ) - pi / ( 4.0f * n );  // parameter of each "petal", integralOfCosNThetaSquared(θ)
	float theta = 0.0f;  // iteration starting point
	for ( int iter = 0; iter < 9; iter++ ) {
		float cdf = 0.5f * theta + sin( 2.0f * n * theta ) / ( 4.0f * n );  // integral of cos(nθ)²
		float pdf = 0.5f + 0.5f * cos( 2.0f * n * theta );  // cos(nθ)²
		theta -= ( cdf - uf ) / pdf;  // Newton-Raphson
	}
	theta = ui + theta;  // move to petal
	float r = cos( n * theta );  // polar equation
	return v * r * vec2( cos( theta ), sin( theta ) );  // polar coordinate
}

vec2 TriRose () {
	return randRose( 3.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

vec2 QuintRose () {
	return randRose( 5.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

// ring formed by two concentric circles with radius r0 and r1
vec2 randConcentric ( float r0, float r1, float rand1, float rand2 ) {
	float u = 2.0f * pi * rand1; // θ
	float v = sqrt( mix( r0 * r0, r1 * r1, rand2 ) ); // r
	return v * vec2( cos( u ), sin( u ) ); // polar to Cartesian
}

vec2 RingBokeh () {
	return randConcentric( 0.6f, 1.1f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

/*
// n-blade aperture, from loicvdb, https://www.shadertoy.com/view/WsBfWD
vec2 sampleAperture ( int nbBlades, float rotation, float rand1, float rand2, float rand3 ) {
	const float side = sin( pi / float( nbBlades ) );
	vec2 tri = vec2( rand1, rand2 );
	if ( tri.x - tri.y > 0.0f ) tri = vec2( tri.x - 1.0f, 1.0f - tri.y );
	tri *= vec2( side, -sqrt( 1.0f - side * side ) );
	const float angle = rotation + 2.0f * pi * floor( rand3 * float( nbBlades ) ) / float( nbBlades );
	return vec2( tri.x * cos( angle ) + tri.y * sin( angle ),
				tri.y * cos( angle ) - tri.x * sin( angle ) );
}
*/

// regular n-gon with radius 1
vec2 randPolygon ( float n, float rand1, float rand2 ) {
	float u = n * rand1;
	float v = rand2;
	float ui = floor( u );  // index of triangle
	float uf = fract( u );  // interpolating in triangle
	vec2 v0 = vec2( cos( 2.0f * pi * ui / n ), sin( 2.0f * pi * ui / n ) );  // triangle edge #1
	vec2 v1 = vec2( cos( 2.0f * pi * ( ui + 1.0f ) / n ), sin( 2.0f * pi * ( ui + 1.0f ) / n ) );  // triangle edge #2
	return sqrt( v ) * mix( v0, v1, uf );  // sample inside triangle
}

vec2 Pentagon () {
	return randPolygon( 5.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

vec2 Septagon () {
	return randPolygon( 7.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

vec2 Octagon () {
	return randPolygon( 8.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

vec2 Nonagon () {
	return randPolygon( 9.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

vec2 Decagon () {
	return randPolygon( 10.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

vec2 Eleven_gon () {
	return randPolygon( 11.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

// regular n-star with normalized size
vec2 randStar ( float n, float rand1, float rand2 ) {
	float u = n * rand1;
	float v = rand2;
	float ui = floor( u );  // index of triangle
	float uf = fract( u );  // interpolating in rhombus
	vec2 v0 = vec2( cos( 2.0f * pi * ui / n ), sin( 2.0f * pi * ui / n ) );  // rhombus edge #1
	vec2 v1 = vec2( cos( 2.0f * pi * ( ui + 1.0f ) / n ), sin( 2.0f * pi * ( ui + 1.0f ) / n ) );  // rhombus edge #2
	vec2 p = v0 * v + v1 * uf;  // sample rhombus
	return p / ( n * sin( 2.0f * pi / n ) / pi );  // normalize size
}

vec2 Star5 () {
	return randStar( 5.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

vec2 Star6 () {
	return randStar( 6.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

vec2 Star7 () {
	return randStar( 7.0f, NormalizedRandomFloat(), NormalizedRandomFloat() );
}

// from https://www.shadertoy.com/view/3fSyWG
vec2 rnd_disc_cauchy() {
	vec2 h = vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) * vec2( 6.28318530718f, pi / 2.0f );
	// note ( re: below ): this clamp is actually significant, at a value of 10.0f... depending on application it may be an issue that it will be clipping frequently
	// h.y = clamp( tan( h.y ), 0.0f, 10.0f ); // this distribution has a very long (read: infinite) tail, and it is creating noise
	// h.y = tan( h.y * 0.95f );
	h.y = tan( h.y );
	return h.y * vec2( sin( h.x ), cos( h.x ) );
}

vec3 RandomCauchyVector () {
	float z = NormalizedRandomFloat() * 2.0f - 1.0f;
	float a = NormalizedRandomFloat() * 2.0f * pi;
	// float r = sqrt( 1.0f - z * z );
	float r = tan( NormalizedRandomFloat() * 0.9f * ( pi / 2.0f ) );
	float x = r * cos( a );
	float y = r * sin( a );
	return vec3( x, y, z * r );
}

// ======================================================================================================================
#define BOKEHSHAPE_NONE				0
#define BOKEHSHAPE_EDGEBIAS_DISK	1
#define BOKEHSHAPE_UNIFORM_DISK		2
#define BOKEHSHAPE_REJECTION_HEX	3
#define BOKEHSHAPE_UNIFORM_HEX		4
#define BOKEHSHAPE_UNIFORM_HEART	5
#define BOKEHSHAPE_UNIFORM_ROSE3	6
#define BOKEHSHAPE_UNIFORM_ROSE5	7
#define BOKEHSHAPE_UNIFORM_RING		8
#define BOKEHSHAPE_UNIFORM_PENTAGON	9
#define BOKEHSHAPE_UNIFORM_SEPTAGON	10
#define BOKEHSHAPE_UNIFORM_OCTAGON	11
#define BOKEHSHAPE_UNIFORM_NONAGON	12
#define BOKEHSHAPE_UNIFORM_DECAGON	13
#define BOKEHSHAPE_UNIFORM_11GON	14
#define BOKEHSHAPE_UNIFORM_5STAR	15
#define BOKEHSHAPE_UNIFORM_6STAR	16
#define BOKEHSHAPE_UNIFORM_7STAR	17
#define BOKEHSHAPE_CAUCHY_DISK		18
// ======================================================================================================================

vec2 GetBokehOffset ( int mode ) {
	switch ( mode ) {

		case BOKEHSHAPE_NONE:				return vec2( 0.5f ); break;
		case BOKEHSHAPE_EDGEBIAS_DISK:		return RandomInUnitDisk(); break;			// edge-biased disk
		case BOKEHSHAPE_UNIFORM_DISK:		return CircleOffset(); break;				// not-edge-biased disk
		case BOKEHSHAPE_REJECTION_HEX:		return RejectionSampleHexOffset(); break;	// rejection sample hex
		case BOKEHSHAPE_UNIFORM_HEX:		return UniformSampleHexagon(); break;		// uniform sample hex
		case BOKEHSHAPE_UNIFORM_HEART:		return HeartOffset(); break;				// uniform sample heart shape
		case BOKEHSHAPE_UNIFORM_ROSE3:		return TriRose(); break;					// three bladed rosette shape
		case BOKEHSHAPE_UNIFORM_ROSE5:		return QuintRose(); break;					// five  "  "
		case BOKEHSHAPE_UNIFORM_RING:		return RingBokeh(); break;					// ring shape
		case BOKEHSHAPE_UNIFORM_PENTAGON:	return Pentagon(); break;					// pentagon shape
		case BOKEHSHAPE_UNIFORM_SEPTAGON:	return Septagon(); break;					// 7 sided shape
		case BOKEHSHAPE_UNIFORM_OCTAGON:	return Octagon(); break;					// octagon shape
		case BOKEHSHAPE_UNIFORM_NONAGON:	return Nonagon(); break;					// 9 sided
		case BOKEHSHAPE_UNIFORM_DECAGON:	return Decagon(); break;					// 10 sided
		case BOKEHSHAPE_UNIFORM_11GON:		return Eleven_gon(); break;					// 11 sided
		case BOKEHSHAPE_UNIFORM_5STAR:		return Star5(); break;						// star patterns
		case BOKEHSHAPE_UNIFORM_6STAR:		return Star6(); break;
		case BOKEHSHAPE_UNIFORM_7STAR:		return Star7(); break;
		case BOKEHSHAPE_CAUCHY_DISK:		return rnd_disc_cauchy(); break;

		default: return vec2( 0.0f ); break;											// no offset
	}
}