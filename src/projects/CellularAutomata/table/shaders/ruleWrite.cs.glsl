#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 4, rg32ui ) uniform uimage2D ruleBuffer;

#include "random.h"
#include "mathUtils.h"

// encoded rules
uniform int numRules;
uniform uvec2 rules[ 1000 ];

uniform int rulePick;

vec3 hash3( vec2 p ) {
	vec3 q = vec3( dot(p,vec2(127.1,311.7)),
					dot(p,vec2(269.5,183.3)),
					dot(p,vec2(419.2,371.9)) );
	return fract(sin(q)*43758.5453);
}

float voronoise( in vec2 p, float u, float v ) {
	float k = 1.0+63.0*pow(1.0-v,6.0);

	vec2 i = floor(p);
	vec2 f = fract(p);

	vec2 a = vec2(0.0,0.0);
	for( int y=-2; y<=2; y++ )
	for( int x=-2; x<=2; x++ ) {
		vec2  g = vec2( x, y );
		vec3  o = hash3( i + g ) * vec3( u, u, 1.0f );
		vec2  d = g - f + o.xy;
		float w = pow( 1.0f - smoothstep( 0.0f, 1.414f, length( d ) ), k );
		a += vec2( o.z * w, w );
	}

	return a.x/a.y;
}


uvec2 getRule () {
	// uint rule[ 25 ] = {
	// 	0, 1, 0, 2, 2,
	// 	0, 2, 2, 1, 0,
	// 	2, 0, 0, 2, 1,
	// 	0, 0, 0, 2, 0,
	// 	2, 0, 0, 2, 2
	// };

	// uvec2 encodedRule = uvec2( 0 );
	// for ( int i = 0; i < 16; i++ ) { // encode 0..15
	// 	encodedRule.x += ( rule[ i ] << ( 30 - 2 * i ) );
	// }

	// for ( int i = 16; i < 25; i++ ) { // encode 16..24
	// 	encodedRule.y += ( rule[ i ] << ( 30 - 2 * ( i - 16 ) ) );
	// }

	// int offset = checkerBoard( 2000.0f, vec3( gl_GlobalInvocationID.xy, 0.0f ) ) ? 100 : 0;
	// uvec2 encodedRule = rules[ ( int( NormalizedRandomFloat() * numRules ) + offset ) % numRules ];

	uint value = ( PCGHash() % numRules ) % 8u;

	uvec2 encodedRule = ( value < 3 ) ? uvec2( 0u ) : rules[ value + rulePick ];
	return encodedRule;
}

void main () {
	// ivec2 gridIdx = ivec2( gl_GlobalInvocationID.xy ) / 128;
	// rng_state = gridIdx.x + gridIdx.y * 100 + rulePick;

	// rng_state = int( 1000 * voronoise( vec2( gl_GlobalInvocationID.xy ) * 0.03f, 1.6f, 0.0f ) + rulePick );
	// rng_state = int( rulePick );

	// figuring out what values to write... start with a constant
	// imageStore( ruleBuffer, ivec2( gl_GlobalInvocationID.xy ), getRule().rgrg );
	imageStore( ruleBuffer, ivec2( gl_GlobalInvocationID.xy ), uvec4( rules[ rulePick ].rgrg ) );
}