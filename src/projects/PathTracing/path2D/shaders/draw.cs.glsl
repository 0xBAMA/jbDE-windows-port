#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// the field buffer image
uniform sampler2D bufferImage;

#include "colorspaceConversions.glsl"

void main () {
	// pixel location
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	vec3 col = rgb_to_srgb( xyz_to_rgb( texture( bufferImage, vec2( loc + 0.5f ) / imageSize( accumulatorTexture ).xy ).rgb ) );

	// write the data to the image
	imageStore( accumulatorTexture, loc, vec4( col, 1.0f ) );
}
