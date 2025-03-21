#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, rgba8ui ) uniform uimage2D displayTexture;
layout( binding = 3, rgba8 ) uniform image2D pattern;

#include "VoraldoCompatibility/colorspaceConversions.h"
#include "VoraldoCompatibility/ditherNoiseFuncs.h"

uniform int numBits;
uniform int colorspacePick;
uniform int patternSelector;
uniform int frameNumber;

vec3 getNoise ( int mode ) {
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	switch ( mode ) {
	case 0: // no pattern
		return vec3( 0.0 );
		break;

	// texture based
	case 1:
	case 2:
	case 3: // bayer matricies
		return imageLoad( pattern, location % imageSize( pattern ) ).rrr;
		break;
	case 4: // blue noise
		return imageLoad( blueNoise, location % imageSize( blueNoise ) ).rrr / 255.0;
		break;
	case 5:	// rgb blue noise
		return imageLoad( blueNoise, location % imageSize( blueNoise ) ).rgb / 255.0;
		break;
	case 6: // cycled mono blue
		return vec3( cycle( imageLoad( blueNoise, location % imageSize( blueNoise ) ).r / 255.0, frameNumber ) );
		break;
	case 7: // cycled rgb blue
		return cycle( imageLoad( blueNoise, location % imageSize( blueNoise ) ).rgb / 255.0, frameNumber );
		break;

	// noise based
	case 8: // uniform random noise
		return getUniformNoise();
		break;
	case 9: // interleaved gradient noise
		return getInterleavedGradientNoise();
		break;
	case 10: // vlachos noise
		return getVlachosNoise();
		break;
	case 11: // triangle remapped vlachos noise
		return getVlachosTriangle();
		break;
	case 12: // triangle remapped uniform random noise
		return getMonoTriangle();
		break;
	case 13: // triangle remapped uniform ( three channel )
		return getRGBTriangle();
		break;

	default:
		return vec3( 0.0 );
		break;
	}
}

vec4 quantize ( vec4 value ) {
	// demofox's method https://www.shadertoy.com/view/4sKBWR
	vec4 noiseValue = getNoise( patternSelector ).rgbr;
	vec4 scale = vec4( exp2( float( numBits ) ) - 1.0 );
	value = floor( value * scale + noiseValue ) / scale;
	return value;
}

void main () {
	ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	uvec4 originalColor = imageLoad( displayTexture, location );
	vec4 converted = convert( originalColor, colorspacePick );
	vec4 processed = quantize( converted );
	uvec4 quantizedValue = convertBack( processed, colorspacePick );
	imageStore( displayTexture, location, quantizedValue );
}
