#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba32f ) uniform image2D adamN;			// the larger image, being downsampled
layout( binding = 1, rgba32f ) uniform image2D adamNPlusOne;	// the smaller image, being written to

uniform ivec2 dims;

void main () {
	// pixel location on layer N + 1... we are dispatching for texels in layer N + 1
	ivec2 newLoc = ivec2( gl_GlobalInvocationID.xy );

	// half res...
	ivec2 oldLoc = newLoc * 2;

	if ( oldLoc.x < dims.x && oldLoc.y < dims.y ) {
		// initialize sum of counts
		float count = 0.0f;
		vec4 sum = vec4( 0.0f );

		// each invocation needs to:
		ivec2 offsets[ 4 ] = ivec2[]( ivec2( 0, 0 ), ivec2( 1, 0 ), ivec2( 0, 1 ), ivec2( 1, 1 ) );
		for ( int i = 0; i < 4; i++ ) {

			// rgb + count
			vec4 valueSample = imageLoad( adamN, oldLoc + offsets[ i ] );

			// the sum of their counts, goes into adamCountNPlusOne
			count += valueSample.a;

			// so we're setting up for a weighted sum...
			// color samples weighted by count...
			sum.rgb += valueSample.rgb * valueSample.a;
		}

		if ( count != 0 ) {
		// so now, if we have a nonzero count, divide back out...
			// divide this to get a weighted sum here
			sum /= float( count );
		}

		sum.a = count;

		// and store the results for layer N + 1
		imageStore( adamNPlusOne, newLoc, sum );
	}
}