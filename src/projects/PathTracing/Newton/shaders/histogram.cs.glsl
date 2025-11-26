#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// film plane state - film plane is 3x as wide as it would be otherwise, to accomodate the separate channels
layout( binding = 3, r32ui ) uniform uimage2D filmPlaneImage;

// histogram storage for pre- and post-transformed color values
layout( binding = 4, r32ui ) uniform uimage2D inputValueHistogram;		// extremely wide dynamic range from 32bit tally
layout( binding = 5, r32ui ) uniform uimage2D outputValueHistogram;		// low dynamic range prepped output

#include "mathUtils.h"
#include "biasGain.h"

uniform float slope;
uniform float thresh;
uniform float powerScalar;

const vec3 lumaWeights = vec3( 0.299f, 0.587f, 0.114f );

void inputTally ( uvec3 inputSample ) {
	const uint luma = uint( dot( vec3( inputSample ), lumaWeights ) );
	// locations to tally
	ivec4 xpos = ivec4(
		findMSB( inputSample.x ),
		findMSB( inputSample.y ),
		findMSB( inputSample.z ),
		findMSB( luma )
	);
	// make the appropriate tallies
	imageAtomicAdd( inputValueHistogram, ivec2( 4 * xpos.x + 0, 0 ), 1 );
	imageAtomicAdd( inputValueHistogram, ivec2( 4 * xpos.y + 1, 0 ), 1 );
	imageAtomicAdd( inputValueHistogram, ivec2( 4 * xpos.z + 2, 0 ), 1 );
	imageAtomicAdd( inputValueHistogram, ivec2( 4 * xpos.w + 3, 0 ), 1 );
}

void outputTally ( vec3 outputSample ) {
	const float w = imageSize( outputValueHistogram ).x / 4.0f;
	const float luma = saturate( dot( vec3( outputSample ), lumaWeights ) );
	// locations to tally
	ivec4 xpos = ivec4(
		int( outputSample.x * w ),
		int( outputSample.y * w ),
		int( outputSample.z * w ),
		int( luma * w )
	);
	// make the appropriate tallies
	imageAtomicAdd( outputValueHistogram, ivec2( 4 * xpos.x + 0, 0 ), 1 );
	imageAtomicAdd( outputValueHistogram, ivec2( 4 * xpos.y + 1, 0 ), 1 );
	imageAtomicAdd( outputValueHistogram, ivec2( 4 * xpos.z + 2, 0 ), 1 );
	imageAtomicAdd( outputValueHistogram, ivec2( 4 * xpos.w + 3, 0 ), 1 );
}

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// load the values
	uvec3 inputSample = uvec3(
		imageLoad( filmPlaneImage, ivec2( 3, 1 ) * loc + ivec2( 0, 0 ) ).r,
		imageLoad( filmPlaneImage, ivec2( 3, 1 ) * loc + ivec2( 1, 0 ) ).r,
		imageLoad( filmPlaneImage, ivec2( 3, 1 ) * loc + ivec2( 2, 0 ) ).r
	);

	// tally the input value histogram... not sure what the plan is for mapping here, use bfind to find higest bit set?
	inputTally( inputSample );

	// transform the values for output...
	vec3 outputSample = vec3(
		biasGain( saturate( inputSample.r / powerScalar ), slope, thresh ),
		biasGain( saturate( inputSample.g / powerScalar ), slope, thresh ),
		biasGain( saturate( inputSample.b / powerScalar ), slope, thresh )
	);

	// tally these values for the output histogram
	outputTally( outputSample );
}