#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

#if 1 // 1 for guts, 0 for more generic version

	// yonatan/zozuar's pulsing guts shader ported from twi.gl : https://twigl.app/?ol=true&ss=-NNIajM4aEzy75lqtAUd
		// effect breakdown / explanation: https://www.shadertoy.com/view/dtSSDR
	const ivec2 bufferSize = imageSize( accumulatorTexture ).xy;
	vec2 uv = ( ( vec2( writeLoc ) + vec2( 0.5f ) ) / vec2( bufferSize ) ) * 2.0f - vec2( 1.0f );
	vec3 col = vec3( 0.0f );
	vec2 n = vec2( 0.0f );
	vec2 q = vec2( 0.0f );
	float d = dot( uv, uv );
	float S = 12.0f;
	float a = 0.0f;
	mat2 m = mat2( cos( 5.0f ), sin( 5.0f ), -sin( 5.0f ), cos( 5.0f ) );
	for ( float j = 0.0f; j < 20.0f; j++ ) {
		uv *= m;
		n *= m;
		q = uv * S + time * 4.0f + sin( time * 4.0f - d * 6.0f ) * 0.8f + j + n;
		a += dot( cos( q ) / S, vec2( 0.2f ) );
		n -= sin( q );
		S *= 1.2f;
	}

	col = vec3( 4.0f, 2.0f, 1.0f ) * ( a + 0.2f ) + a + a - d;
	col = pow( col, vec3( 2.2f ) ); // normal colors
	// col = pow( col, vec3( 2.0f ) ); // skews blue/white/etc

#else

	// some xor shit
	uint x = uint( writeLoc.x ) % 256;
	uint y = uint( writeLoc.y ) % 256;
	uint xor = ( x ^ y );

	// get some blue noise going, for additional shits
	vec3 col = ( xor < 128 ) ?
		( imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).xyz / 255.0f ) :
		vec3( xor / 255.0f );

#endif

	col = vec3( 0.0f );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
