#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// the field buffer image
uniform isampler2D bufferImageX;
uniform isampler2D bufferImageY;
uniform isampler2D bufferImageZ;
uniform isampler2D bufferImageCount;

// scale factor based on max observed brightness
// const float autoExposureBase = 1600000.0f;
uniform float autoExposureBase;
uniform int autoExposureTexOffset;
uniform sampler2D fieldMax;

#include "colorspaceConversions.glsl"

void main () {
	// pixel location
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	vec2 samplePoint = vec2( loc + 0.5f ) / imageSize( accumulatorTexture ).xy;

	// what is the autoexposure brightness factor? clamp on the bottom end
	const float autoExposureAdjust = 1.0f / max( 0.01f, texelFetch( fieldMax, ivec2( 0 ), autoExposureTexOffset ).r );

	// baking in the 0-255 AA factor
	const float count = float( texture( bufferImageCount, samplePoint ).r );
	const vec3 col = ( count == 0.0f ) ? vec3( 0.0f ) : // no data...
	rgb_to_srgb( xyz_to_rgb( autoExposureAdjust * vec3( // these are tally sums + number of samples for averaging
	( float( texture( bufferImageX, samplePoint ).r ) / 1024.0f ),
	( float( texture( bufferImageY, samplePoint ).r ) / 1024.0f ),
	( float( texture( bufferImageZ, samplePoint ).r ) / 1024.0f )
	) / autoExposureBase ) );
	/*
	const vec3 col = ( count == 0.0f ) ? vec3( 0.0f ) : // no data...
	rgb_to_srgb( xyz_to_rgb( autoExposureAdjust * vec3( // these are tally sums + number of samples for averaging
	( float( texture( bufferImageX, samplePoint ).r ) / 1024.0f ),
	( float( texture( bufferImageY, samplePoint ).r ) / 1024.0f ),
	( float( texture( bufferImageZ, samplePoint ).r ) / 1024.0f )
	) / count ) );
	*/

	// write the data to the image
	imageStore( accumulatorTexture, loc, vec4( col, 1.0f ) );
}
