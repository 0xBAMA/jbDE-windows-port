#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, r32f ) uniform image2D waterfall;

uniform int waterfallOffset;
uniform int dataSize;
uniform int paletteSelect;

layout( binding = 0, std430 ) readonly buffer signalData	{ float inputData[]; };
layout( binding = 1, std430 ) readonly buffer fftData		{ float outputData[]; };

#include "mathUtils.h"
#include "colorRamps.glsl.h"

void main () {
	// pixel location
	const ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const ivec2 sampleLoc = writeLoc / ivec2( 2, 1 ); // make it bigger by dividing

	// pull data from the buffer
	const float signalValue = inputData[ sampleLoc.x ];

	// dc offset is in fftData[ 0 ], also remember that real signals are mirrored, here, so we take the bottom half to get what we want
		// note also that the lowest frequencies are towards the top end of the array, so it's inverted
	const int idx = sampleLoc.x / 2;
	const float fftValue = outputData[ idx ];

	const int heightSignalDisplay = 150;
	const int heightFFTDisplay = 100;

	vec3 col = vec3( 0.0f );
	if ( sampleLoc.x < dataSize ) {
		if ( sampleLoc.y < heightSignalDisplay ) {
			// waveform graph, showing the signal directly
			const float graphY = RangeRemapValue( float( sampleLoc.y ), 0.0f, heightSignalDisplay, 1.0f, -1.0f );

			// maybe something to make the vertical span more reasonable

			if ( abs( graphY - signalValue ) < 0.0125f ) {
				// col = vec3( saturate( signalValue ), 0.0f, saturate( -signalValue ) );
				col = vec3( 1.0f );
			} else if ( abs( graphY ) < 0.0125f ) { // axis marker
				col = vec3( 0.618f );
			}
		} else if ( sampleLoc.y <= heightSignalDisplay + heightFFTDisplay ) {
			// show the fft data
			const float graphY = RangeRemapValue( float( sampleLoc.y ), heightSignalDisplay, heightSignalDisplay + heightFFTDisplay, 50.0f, 0.0f );
			if ( graphY < fftValue ) {
				// col = vec3( 1.0f );
				// col = refPalette( saturate( RangeRemapValue( graphY, 0.0f, 50.0f, 0.0f, 1.0f ) ), paletteSelect ).rgb;
				col = refPalette( saturate( RangeRemapValue( fftValue, 0.0f, 100.0f, 0.0f, 1.0f ) ), paletteSelect ).rgb;
			}
		} else {
			// waterfall graph, ring buffer
			ivec2 wSampleLoc = sampleLoc;
			wSampleLoc.x /= 2;
			wSampleLoc.y -= ( heightSignalDisplay + heightFFTDisplay );
			wSampleLoc.y += waterfallOffset;
			wSampleLoc.y = ( wSampleLoc.y % imageSize( waterfall ).y );
			if ( sampleLoc.y < ( heightSignalDisplay + heightFFTDisplay + imageSize( waterfall ).y ) ) {
				col = refPalette( saturate( RangeRemapValue( imageLoad( waterfall, wSampleLoc ).r, 0.0f, 100.0f, 0.0f, 1.0f ) ), paletteSelect ).rgb;
			}
		}
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
