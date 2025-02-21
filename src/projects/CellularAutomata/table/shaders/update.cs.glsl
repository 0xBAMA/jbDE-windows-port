#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// CA state buffers
layout( binding = 2, r32ui ) uniform uimage2D backBuffer;
layout( binding = 3, r32ui ) uniform uimage2D frontBuffer;
layout( binding = 4, rg32ui ) uniform uimage2D ruleBuffer;

// uniform int rule[ 25 ];

int getRuleValue ( int x, int y ) {
	// int rule[] = {
	// 	// 2, 0, 2, 2, 1,
	// 	// 0, 2, 0, 2, 2,
	// 	// 1, 1, 0, 0, 2,
	// 	// 0, 1, 1, 2, 1,
	// 	// 2, 2, 1, 1, 2

	// 	2, 0, 1, 2, 1,
	// 	0, 1, 0, 2, 2,
	// 	1, 1, 0, 0, 2,
	// 	0, 2, 1, 2, 1,
	// 	2, 2, 1, 1, 2
	// };

	// return rule[ x + 5 * y ];

	// rule is encoded in 2x 32-bit values...
	uvec2 encodedRule = imageLoad( ruleBuffer, ivec2( gl_GlobalInvocationID.xy ) ).rg;
	const int index = clamp( x + 5 * y, 0, 24 );

	// masking out 2 bits of the value loaded from the texel
	if ( index < 16 ) {
		// we're pulling from the red channel
		int shift = 30 - 2 * index;
		return int( clamp( ( encodedRule.r >> shift ) & 3u, 0, 2 ) );
	} else {
		// we are getting data from the green channel
		int shift = 30 - 2 * ( index - 16 );
		return int(  clamp( ( encodedRule.g >> shift ) & 3u, 0, 2 ) );
	}
}

bool getStateForBit ( ivec2 location, uint bit ) {
	// compute the bitmask
	uint bitmask = 1u << bit;

	// read state from back buffer
	uint previousState = ( imageLoad( backBuffer, location ).r & bitmask ) != 0 ? 1 : 0;

	// read neighborhood values from back buffer
	ivec2 count = ivec2( 0 );
	// ortho directions
	count[ 0 ] += ( imageLoad( backBuffer, location + ivec2(  1,  0 ) ).r & bitmask ) != 0 ? 1 : 0;
	count[ 0 ] += ( imageLoad( backBuffer, location + ivec2(  0,  1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count[ 0 ] += ( imageLoad( backBuffer, location + ivec2(  0, -1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count[ 0 ] += ( imageLoad( backBuffer, location + ivec2( -1,  0 ) ).r & bitmask ) != 0 ? 1 : 0;

	// diagonals
	count[ 1 ] += ( imageLoad( backBuffer, location + ivec2(  1, -1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count[ 1 ] += ( imageLoad( backBuffer, location + ivec2( -1, -1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count[ 1 ] += ( imageLoad( backBuffer, location + ivec2( -1,  1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count[ 1 ] += ( imageLoad( backBuffer, location + ivec2(  1,  1 ) ).r & bitmask ) != 0 ? 1 : 0;

	// determine new state - Conway's Game of Life rules
	// return ( ( count == 2 || count == 3 ) && previousState == 1 ) || ( count == 3 && previousState == 0 );

	int rule = getRuleValue( clamp( count[ 0 ], 0, 4 ), clamp( count[ 1 ], 0, 4 ) );
	if ( rule == 2 )
		return ( previousState != 0 );
	else
		return ( rule == 1 );
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// iterate through the 32 bits in the buffer
		// they will all be evaluated - CPU side makes the determination
		// of whether they are active, by either having populated that
		// sort of bit layer or having left it empty

	uint previousState = imageLoad( backBuffer, writeLoc ).r;
	uint state = ( previousState << 1 ) + ( getStateForBit( writeLoc, 0 ) ? 1 : 0 );

	// write the data to the front buffer
	imageStore( frontBuffer, writeLoc, uvec4( state, 0, 0, 0 ) );
}