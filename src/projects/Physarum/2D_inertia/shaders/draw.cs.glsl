#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2 ) uniform sampler2D pheremoneBuffer;

// physarum buffer (float 1)	// will have mipchain with autoexposure information, as well. Contains prepared sim data for present

uniform float time;
uniform int autoExposureLevel;

void main () {
	vec2 position = vec2( gl_GlobalInvocationID.xy );
	vec4 color;
	if ( autoExposureLevel != -1 ) {
		color = vec4( texture( pheremoneBuffer, ( position + vec2( 0.5f ) ) / imageSize( accumulatorTexture ).xy ).rrr / texelFetch( pheremoneBuffer, ivec2( 0 ), autoExposureLevel ).r, 1.0f );
	} else {
		color = vec4( texture( pheremoneBuffer, ( position + vec2( 0.5f ) ) / imageSize( accumulatorTexture ).xy ).rrr / 10000.0f, 1.0f );
	}

	imageStore( accumulatorTexture, ivec2( position ), color );
}
