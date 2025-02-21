#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

void main () { // placeholder blue noise background
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 result = imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).xyz / 255.0f;
	imageStore( accumulatorTexture, writeLoc, vec4( result, 1.0f ) );
}