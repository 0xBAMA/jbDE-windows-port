#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// CA state buffers
layout( binding = 2, r32ui ) uniform uimage2D backBuffer;
layout( binding = 3, r32ui ) uniform uimage2D frontBuffer;

bool getStateForBit ( ivec2 location, uint bit ) {
	// compute the bitmask
	uint bitmask = 1u << bit;

	// read state from back buffer
	uint previousState = ( imageLoad( backBuffer, location ).r & bitmask ) != 0 ? 1 : 0;

	// read neighborhood values from back buffer
	uint count = 0;
	count += ( imageLoad( backBuffer, location + ivec2( -1, -1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count += ( imageLoad( backBuffer, location + ivec2(  0, -1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count += ( imageLoad( backBuffer, location + ivec2(  1, -1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count += ( imageLoad( backBuffer, location + ivec2( -1,  0 ) ).r & bitmask ) != 0 ? 1 : 0;
	// skip center pixel, already exists in previousState
	count += ( imageLoad( backBuffer, location + ivec2(  1,  0 ) ).r & bitmask ) != 0 ? 1 : 0;
	count += ( imageLoad( backBuffer, location + ivec2( -1,  1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count += ( imageLoad( backBuffer, location + ivec2(  0,  1 ) ).r & bitmask ) != 0 ? 1 : 0;
	count += ( imageLoad( backBuffer, location + ivec2(  1,  1 ) ).r & bitmask ) != 0 ? 1 : 0;

	// determine new state - Conway's Game of Life rules
	return ( ( count == 2 || count == 3 ) && previousState == 1 ) || ( count == 3 && previousState == 0 );
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