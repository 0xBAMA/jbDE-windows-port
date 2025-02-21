#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// pathtracer output buffers
layout( binding = 0, r32ui ) uniform uimage2D rTally;
layout( binding = 1, r32ui ) uniform uimage2D gTally;
layout( binding = 2, r32ui ) uniform uimage2D bTally;
layout( binding = 3, r32ui ) uniform uimage2D count;

// mip 0 of the Adam representation
layout( binding = 4, rgba32f ) uniform image2D adam;

uniform ivec2 dims;

void main () {
	// pixel location
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	if ( loc.x < dims.x && loc.y < dims.y ) {

		// loading the data from the atomic images... scale by a constant, since we have ints
		float r = float( imageLoad( rTally, loc ).r ) / 1024.0f;
		float g = float( imageLoad( gTally, loc ).r ) / 1024.0f;
		float b = float( imageLoad( bTally, loc ).r ) / 1024.0f;
		float c = float( imageLoad( count, loc ).r );

		// averaging out the accumulated color data by sample count
		if ( c != 0.0f ) {
			vec4 outputValue = vec4( r / c, g / c, b / c, c );
			imageStore( adam, loc, outputValue );
		}
	}
}