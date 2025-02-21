#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

layout( binding = 2, rgba32f ) uniform image2D CPUTexture;

// uniform float time;

#include "colorRamps.glsl.h"
#include "mathUtils.h"

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	vec3 col = imageLoad( CPUTexture, ivec2( vec2(
		writeLoc + vec2( 0.5f ) ) / imageSize( accumulatorTexture ).xy * imageSize( CPUTexture ).xy ) ).rgb;

	// col = inferno2( imageLoad( CPUTexture, ivec2( vec2(
		// writeLoc + vec2( 0.5f ) ) / imageSize( accumulatorTexture ).xy * imageSize( CPUTexture ).xy ) ).a / 1000.0f );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
