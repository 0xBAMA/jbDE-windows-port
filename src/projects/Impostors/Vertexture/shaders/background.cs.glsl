#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	// uvec3 result = uvec3( imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).r * 0.1618f );
	// imageStore( accumulatorTexture, writeLoc, uvec4( result, 255 ) );
	imageStore( accumulatorTexture, writeLoc, vec4( 0.0f, 0.0f, 0.0f, 1.0f ) );
}
