#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

void main () {

	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// some xor shit
	uint x = uint( writeLoc.x ) % 256;
	uint y = uint( writeLoc.y ) % 256;
	uint xor = ( x ^ y );

	// get some blue noise going, for additional shits
	vec3 col = ( xor < 128 ) ?
		( imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).xyz / 255.0f ) :
		vec3( xor / 255.0f );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );

}
