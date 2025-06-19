#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// the field buffer image
layout( rgba32f ) uniform image2D bufferImage;

#include "colorspaceConversions.glsl"

void main () {
	// pixel location
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	vec3 col = vec3( 0.0f );
	if ( all( lessThan( loc, imageSize( bufferImage ) ) ) ) {
		// accumulated value is in the XYZ colorspace
		vec3 inXYZ = imageLoad( bufferImage, loc ).rgb;
		col = xyz_to_rgb( inXYZ );
	}

	// write the data to the image
	imageStore( accumulatorTexture, loc, vec4( col, 1.0f ) );
}
