vec3 GetColorForTemperature ( const float temperature ) {
	mat3 m = ( temperature <= 6500.0f )
		? mat3( vec3( 0.0f, -2902.1955373783176f, -8257.7997278925690f ),
				vec3( 0.0f, 1669.5803561666639f, 2575.2827530017594f ),
				vec3( 1.0f, 1.3302673723350029f, 1.8993753891711275f ) )
		: mat3( vec3( 1745.0425298314172f, 1216.6168361476490f, -8257.7997278925690f ),
				vec3( -2666.3474220535695f, -2173.1012343082230f, 2575.2827530017594f ),
				vec3( 0.55995389139931482f, 0.70381203140554553f, 1.8993753891711275f ) );
	return mix( clamp( vec3( m[ 0 ] / ( vec3( clamp( temperature, 1000.0f, 40000.0f ) ) + m[ 1 ] )
		+ m[ 2 ] ), vec3( 0.0f ), vec3( 1.0f ) ), vec3( 1.0f ), smoothstep( 1000.0f, 0.0f, temperature ) );
}

float RangeRemapValue ( float value, float inLow, float inHigh, float outLow, float outHigh ) {
	return outLow + ( value - inLow ) * ( outHigh - outLow ) / ( inHigh - inLow );
}

#ifndef saturate
#define saturate(x) clamp(x, 0, 1)
#endif

#ifndef UINT_MAX
#define UINT_MAX (0xFFFFFFFF-1)
#endif

#ifndef PI_DEFINED
#define PI_DEFINED
const float pi = 3.141592f;
const float tau = 2.0f * pi;
const float sqrtpi = 1.7724538509f;
#endif

mat2 Rotate2D ( in float a ) {
	float c = cos( a ), s = sin( a );
	return mat2( c, s, -s, c );
}

mat3 Rotate3D ( const float angle, const vec3 axis ) {
	const vec3 a = normalize( axis );
	const float s = sin( angle );
	const float c = cos( angle );
	const float r = 1.0f - c;
	return mat3(
		a.x * a.x * r + c,
		a.y * a.x * r + a.z * s,
		a.z * a.x * r - a.y * s,
		a.x * a.y * r - a.z * s,
		a.y * a.y * r + c,
		a.z * a.y * r + a.x * s,
		a.x * a.z * r + a.y * s,
		a.y * a.z * r - a.x * s,
		a.z * a.z * r + c
	);
}

// hash funcs

// https://www.pcg-random.org/
uint pcg( uint v ) {
	uint state = v * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

uvec2 pcg2d( uvec2 v ) {
	v = v * 1664525u + 1013904223u;

	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;

	v = v ^ (v>>16u);

	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;

	v = v ^ (v>>16u);

	return v;
}

// http://www.jcgt.org/published/0009/03/02/
uvec3 pcg3d( uvec3 v ) {
	v = v * 1664525u + 1013904223u;

	v.x += v.y*v.z;
	v.y += v.z*v.x;
	v.z += v.x*v.y;

	v ^= v >> 16u;

	v.x += v.y*v.z;
	v.y += v.z*v.x;
	v.z += v.x*v.y;

	return v;
}

bool checkerBoard ( in float scale, in vec3 p ) {
	return ( step( 0.0f,
		cos( scale * pi * p.x + pi / 2.0f ) *
		cos( scale * pi * p.y + pi / 2.0f ) *
		cos( scale * pi * p.z + pi / 2.0f ) ) == 0 );
}
//=============================================================================================================================
// given a particular input vector, we basically need two perpendicular vectors to be able to freely place in 3 dimensions
void createBasis ( in vec3 z, out vec3 x, out vec3 y ) {
	x = normalize( cross( z, ( z.y > 0.999f ) ? vec3( 1.0f, 0.0f, 0.0f ) : vec3( 0.0f, 1.0f, 0.0f ) ) ); // prevent using an identical vector when taking the initial cross product
	y = normalize( cross( x, z ) ); // y simply needs to be mutually perpendicular to these two vectors, which are themselves mutually perpendicular to one another
}
