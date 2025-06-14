#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// tally buffers
layout( binding = 0, r32ui ) uniform uimage2D redTally;
layout( binding = 1, r32ui ) uniform uimage2D greenTally;
layout( binding = 2, r32ui ) uniform uimage2D blueTally;
layout( binding = 3, r32ui ) uniform uimage2D sampleTally;

// depth buffer, for clearing
layout( binding = 4, r32ui ) uniform uimage2D depthBuffer;

// id buffer
layout( binding = 5, r32ui ) uniform uimage2D idBuffer;

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// do the clears here
	imageStore( redTally, loc, uvec4( 0 ) );
	imageStore( greenTally, loc, uvec4( 0 ) );
	imageStore( blueTally, loc, uvec4( 0 ) );
	imageStore( sampleTally, loc, uvec4( 0 ) );
	imageStore( depthBuffer, loc, uvec4( 0 ) );
	imageStore( idBuffer, loc, uvec4( 0 ) );
}