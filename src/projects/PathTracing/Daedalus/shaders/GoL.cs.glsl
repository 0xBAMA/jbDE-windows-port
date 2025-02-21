#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform int previousSlice;
layout( rgba8ui ) uniform uimage3D ddatex;

vec3 colorAccumulate = vec3( 0.0f );
float contributions = 0.0f;

bool check ( ivec3 location ) {
	uvec4 value = imageLoad( ddatex, location );
	if ( value.a == 255 ) {
		colorAccumulate += value.rgb;
		contributions += 1.0f;
	}
	return ( value.a == 255 );
}

bool getState ( ivec3 location ) {
	// read state from back buffer
	uint previousState = ( ( imageLoad( ddatex, location ).a ) == 255 ) ? 1 : 0;

	// read neighborhood values from back buffer
	uint count = 0;
	count += check( location + ivec3( -1, -1,  0 ) ) ? 1 : 0;
	count += check( location + ivec3(  0, -1,  0 ) ) ? 1 : 0;
	count += check( location + ivec3(  1, -1,  0 ) ) ? 1 : 0;
	count += check( location + ivec3( -1,  0,  0 ) ) ? 1 : 0;
	// skip center pixel, already exists in previousState
	count += check( location + ivec3(  1,  0,  0 ) ) ? 1 : 0;
	count += check( location + ivec3( -1,  1,  0 ) ) ? 1 : 0;
	count += check( location + ivec3(  0,  1,  0 ) ) ? 1 : 0;
	count += check( location + ivec3(  1,  1,  0 ) ) ? 1 : 0;

	// determine new state - Conway's Game of Life rules
	return ( ( count == 2 || count == 3 ) && previousState == 1 ) || ( count == 3 && previousState == 0 );
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uint state = ( getState( ivec3( writeLoc, previousSlice ) ) ? 255 : 0 );
	uvec3 color = imageLoad( ddatex, ivec3( writeLoc, previousSlice ) ).rgb;

	// // mitigation of blinkers, making constant value columns
	// if ( state == 255 ) {
	// 	int sum = 0;
	// 	for ( int i = 0; i < 10; i++ ) {
	// 		sum += ( imageLoad( ddatex, ivec3( writeLoc, previousSlice - i ) ).a == 255 ) ? 0 : 1;
	// 	}
	// 	if ( sum <= 2 ) {
	// 		state = 0;
	// 	}
	// }

	if ( contributions != 0.0f ) {
		colorAccumulate /= contributions;
		// color = uvec3( mix( vec3( color ), colorAccumulate, 0.1f ) );
		color = uvec3( colorAccumulate ) + uvec3( 1 );
	}

	// write the data to the front buffer
	imageStore( ddatex, ivec3( writeLoc, previousSlice + 1 ), uvec4( color, state ) );
}