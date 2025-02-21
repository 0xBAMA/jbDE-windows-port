#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// four images for atomic writes - red, green, blue, luma
layout( r32ui ) uniform uimage2D redImage;
layout( r32ui ) uniform uimage2D greenImage;
layout( r32ui ) uniform uimage2D blueImage;
layout( r32ui ) uniform uimage2D lumaImage;

// buffers for per-column min/max - r, g, b, luma interleaved
// layout( binding = 2, std430 ) readonly buffer perColumnMins { uint columnMins[]; };
// layout( binding = 3, std430 ) readonly buffer perColumnMaxs { uint columnMaxs[]; };
layout( binding = 4, std430 ) buffer binMax { uint globalMax[ 4 ]; };

// the output image
layout( rgba8 ) uniform image2D compositedResult;

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	vec4 color = vec4( 0.0f, 0.0f, 0.0f, 1.0f );

	// I would like to add some gridlines

	const uint yIndex = 255 - loc.y;
	color.r = sqrt( float( imageLoad( redImage,		ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 0 ] ) );
	color.g = sqrt( float( imageLoad( greenImage,	ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 1 ] ) );
	color.b = sqrt( float( imageLoad( blueImage,	ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 2 ] ) );

	imageStore( compositedResult, loc, color );
}