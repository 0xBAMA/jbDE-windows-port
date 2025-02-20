#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// what we're drawing to
layout( binding = 2, rgba16f ) uniform image2D paintBuffer;

void main () {
	// pixel location
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// write the image contents to the screen
	imageStore( accumulatorTexture, loc, imageLoad( paintBuffer, loc ) );
}
