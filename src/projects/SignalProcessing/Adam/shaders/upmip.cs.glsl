#version 430
layout( local_size_x = 1, local_size_y = 1, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

layout( binding = 2, rgba32f ) uniform image2D adamColorN;			// the larger image, being downsampled
layout( binding = 3, rgba32f ) uniform image2D adamColorNPlusOne;	// the smaller image, being written to

layout( binding = 4, r32ui ) uniform uimage2D adamCountN;
layout( binding = 5, r32ui ) uniform uimage2D adamCountNPlusOne;

void main () {
	// pixel location on layer N + 1... we are dispatching for texels in layer N + 1
	ivec2 newLoc = ivec2( gl_GlobalInvocationID.xy );

	// half res...
	ivec2 oldLoc = newLoc * 2;

	// initialize sum of counts
	uint count = 0;
	vec4 colorSum = vec4( 0.0f );

	// each invocation needs to:
	ivec2 offsets[ 4 ] = ivec2[]( ivec2( 0, 0 ), ivec2( 1, 0 ), ivec2( 0, 1 ), ivec2( 1, 1 ) );
	for ( int i = 0; i < 4; i++ ) {

		// update count
		uint countSample = imageLoad( adamCountN, oldLoc + offsets[ i ] ).r;

		// the sum of their counts, goes into adamCountNPlusOne
		count += countSample;

		// update color
		vec4 colorSample = imageLoad( adamColorN, oldLoc + offsets[ i ] );

		// so we're setting up for a weighted sum...
		// adamColorN samples weighted by adamCountN samples...
		colorSum += colorSample * countSample;
	}

	if ( count != 0 ) {
	// so now, if we have a nonzero count, divide back out...
		// divide this sum by the color sum of the adamCountN samples
		colorSum /= float( count );
	}

	// and store the results for layer N + 1
	imageStore( adamColorNPlusOne, newLoc, colorSum );
	imageStore( adamCountNPlusOne, newLoc, uvec4( count ) );
}