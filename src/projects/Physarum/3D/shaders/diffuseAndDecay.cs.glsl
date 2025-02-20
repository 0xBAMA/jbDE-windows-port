#version 430 core
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;

layout( binding = 1, r32ui ) uniform uimage3D previous;
layout( binding = 2, r32ui ) uniform uimage3D current;

layout( binding = 3, r8ui ) uniform uimage3D mask;

uniform uint stampAmount;
uniform bool applyStamp;

bool maskValue ( ivec3 pos ) {
	return ( imageLoad( mask, pos ).r > 0 );
}

uniform float decayFactor;

void main() {
	// gaussian kernel
	ivec3 pos = ivec3( gl_GlobalInvocationID.xyz );
	uint g = (
		1 * imageLoad( previous, pos + ivec3( -1, -1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec3(  1, -1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec3( -1,  1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec3(  1,  1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec3( -1, -1,  1 ) ).r +
		1 * imageLoad( previous, pos + ivec3(  1, -1,  1 ) ).r +
		1 * imageLoad( previous, pos + ivec3( -1,  1,  1 ) ).r +
		1 * imageLoad( previous, pos + ivec3(  1,  1,  1 ) ).r +

		2 * imageLoad( previous, pos + ivec3(  1,  0, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  0, -1, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  0,  1, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec3( -1,  0, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  1,  0,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  0, -1,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  0,  1,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec3( -1,  0,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  1,  1,  0 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  1, -1,  0 ) ).r +
		2 * imageLoad( previous, pos + ivec3( -1,  1,  0 ) ).r +
		2 * imageLoad( previous, pos + ivec3( -1, -1,  0 ) ).r +

		4 * imageLoad( previous, pos + ivec3( -1,  0,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  1,  0,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  0, -1,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  0,  1,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  0,  0, -1 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  0,  0,  1 ) ).r +

		8 * imageLoad( previous, pos + ivec3(  0,  0,  0 ) ).r ) / 64;

	// ...trying with no blur... since this post seems to skip it? or not... need to look at the code - https://denizbicer.com/202408-UnderstandingPhysarum.html
	// uint g = imageLoad( previous, ( pos ) % ivec3( imageSize( previous ) ) ).r;

	// if ( maskValue( pos ) && applyStamp ) {
	if ( maskValue( pos ) ) {
		// imageStore( current, pos, uvec4( uint( stampAmount ) ) );
		imageStore( current, pos, uvec4( max( uint( 400000 ), uint( decayFactor * g ) ) ) );
	} else {
		imageStore( current, pos, uvec4( uint( decayFactor * g ) ) );
	}
}
