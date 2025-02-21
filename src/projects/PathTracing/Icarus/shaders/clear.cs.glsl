#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// pathtracer accumulator buffers
layout( binding = 0, r32ui ) uniform uimage2D rTally;
layout( binding = 1, r32ui ) uniform uimage2D gTally;
layout( binding = 2, r32ui ) uniform uimage2D bTally;
layout( binding = 3, r32ui ) uniform uimage2D count;
layout( binding = 4, rgba32f ) uniform image2D outputBuffer;


void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	imageStore( rTally, loc, uvec4( 0 ) );
	imageStore( gTally, loc, uvec4( 0 ) );
	imageStore( bTally, loc, uvec4( 0 ) );
	imageStore( count, loc, uvec4( 0 ) );
	imageStore( outputBuffer, loc, vec4( 0.0f ) );
}