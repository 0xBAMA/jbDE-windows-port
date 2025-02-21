#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// the source data, that we are evaluating
layout( rgba32f ) uniform image2D tonemappedSourceData;

// four images for atomic writes - red, green, blue, luma
layout( r32ui ) uniform uimage2D redImage;
layout( r32ui ) uniform uimage2D greenImage;
layout( r32ui ) uniform uimage2D blueImage;
layout( r32ui ) uniform uimage2D lumaImage;

// buffers for per-column min/max - r, g, b, luma interleaved
layout( binding = 2, std430 ) buffer perColumnMins { uint columnMins[]; };
layout( binding = 3, std430 ) buffer perColumnMaxs { uint columnMaxs[]; };
layout( binding = 4, std430 ) buffer binMax { uint globalMax[ 4 ]; };

#include "mathUtils.h"

void main () {
	// this pixel has a color, with three channels that we need to look at + calculated luma
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	if ( all( lessThan( loc, imageSize( tonemappedSourceData ) ) ) ) {
		vec4 pixelColor = imageLoad( tonemappedSourceData, loc );
		uvec4 binCrements = uvec4( // uint colors, computed luma
			uint( saturate( pixelColor.r ) * 255.0f ),
			uint( saturate( pixelColor.g ) * 255.0f ),
			uint( saturate( pixelColor.b ) * 255.0f ),
			uint( saturate( dot( pixelColor.rgb, vec3( 0.299f, 0.587f, 0.114f ) ) ) * 255.0f )
		);

		// update the buffer data
		const int column = loc.x;
		atomicMax( columnMaxs[ 4 * column + 0 ], binCrements.r );
		atomicMax( columnMaxs[ 4 * column + 1 ], binCrements.g );
		atomicMax( columnMaxs[ 4 * column + 2 ], binCrements.b );
		atomicMax( columnMaxs[ 4 * column + 3 ], binCrements.a );

		atomicMin( columnMins[ 4 * column + 0 ], binCrements.r );
		atomicMin( columnMins[ 4 * column + 1 ], binCrements.g );
		atomicMin( columnMins[ 4 * column + 2 ], binCrements.b );
		atomicMin( columnMins[ 4 * column + 3 ], binCrements.a );

		atomicMax( globalMax[ 0 ], imageAtomicAdd( redImage, ivec2( column, binCrements.r ), 1 ) + 1 );
		atomicMax( globalMax[ 1 ], imageAtomicAdd( greenImage, ivec2( column, binCrements.g ), 1 ) + 1 );
		atomicMax( globalMax[ 2 ], imageAtomicAdd( blueImage, ivec2( column, binCrements.b ), 1 ) + 1 );
		atomicMax( globalMax[ 3 ], imageAtomicAdd( lumaImage, ivec2( column, binCrements.a ), 1 ) + 1 );
	}
}