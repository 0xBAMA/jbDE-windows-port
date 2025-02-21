#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// probably move the 3D's DDA traversal into another image, sampled here in this shader
layout( binding = 2, r32ui ) uniform uimage2D dataTexture2D;
layout( binding = 3, r32ui ) uniform uimage3D dataTexture3D;

// target for the DDA draw
layout( binding = 4, r16f ) uniform image2D blockImage;

// falloff curve control, tbd
uniform float histogramDisplayScale;
uniform float histogramDisplayPower;

// windowing the data
uniform uint byteOffset;
uniform uint numBytes;

// ssbo for data bytes
layout( binding = 0, std430 ) buffer dataBuffer { uint data[]; };

// ssbo for 2d histogram max
layout( binding = 1, std430 ) buffer histogram2DMax { uint max2D; };

// ssbo for 3d histogram max
layout( binding = 2, std430 ) buffer histogram3DMax { uint max3D; };

// need to unpack uint8s from full fat uint32s
uint getByte ( uint index ) {
	// since we pack 4 bytes into each uint...
	uint base = index / 4;

	// get the initial uint value (4 bytes)
	uint value = data[ base ];

	// looking at the remainder, to determine how to mask
	switch ( index - ( base * 4 ) ) {
		case 0:
		value = 0xFF000000u & value;
		value = value >> 24;
		break;

		case 1:
		value = 0x00FF0000u & value;
		value = value >> 16;
		break;

		case 2:
		value = 0x0000FF00u & value;
		value = value >> 8;
		break;

		case 3:
		value = 0x000000FFu & value;
		break;
	}
	return value;
}

#include "colorRamps.glsl.h"
// uniform int paletteSelect;
uniform int paletteSelect = 7;

#include "spaceFillingCurves.h.glsl"

float luma ( vec3 rgb ) {
	return dot( vec3( 0.2126f, 0.7152f, 0.0722f ), rgb );
}

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	vec3 color = vec3( 0.0f );
	float scaleFactor = 1.0f;

	if ( writeLoc.x < 256 ) {
		// linear/hilbert data view

		// linear
		// uint index = writeLoc.x + 256 * writeLoc.y;

		// hilbert curve
		uint index = uint( hilbert( ivec2( writeLoc.y % 256, writeLoc.x ), 8 ) ) + ( 256 * 256 * ( writeLoc.y / 256 ) );

		// z curve
		// uint index = uint( xy2z( ivec2( writeLoc.x, writeLoc.y % 256 ) ) ) + ( 256 * 256 * ( writeLoc.y / 256 ) );

		if ( index < byteOffset || index >= byteOffset + numBytes  ) {
			scaleFactor = 0.1f;
		}

		// get the byte value and color it accordingly
		// color = refPalette( getByte( index ) / 255.0f, paletteSelect ).rgb;
		color = vec3( getByte( index ) / 255.0f );

	} else {
		ivec2 loc = writeLoc;
		loc.x -= 300;

		// block visual
		color = imageLoad( blockImage, loc ).rrr;

		// 2d histogram overlay
		// color += refPalette( pow( float( imageLoad( dataTexture2D, loc / 2 ).r ) / float( max2D ), 0.5f ), paletteSelect ).rgb;
		color += vec3( histogramDisplayScale * pow( float( imageLoad( dataTexture2D, loc / 2 ).r ) / float( max2D ),histogramDisplayPower ) );
	}

	// write the data to the image
	// imageStore( accumulatorTexture, writeLoc, vec4( scaleFactor * refPalette( luma( color ), paletteSelect ).rgb, 1.0f ) );
	imageStore( accumulatorTexture, writeLoc, vec4( scaleFactor * vec3( luma( color ) ), 1.0f ) );
}
