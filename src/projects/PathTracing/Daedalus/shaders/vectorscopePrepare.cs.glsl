#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// the source data, that we are evaluating
layout( rgba32f ) uniform image2D tonemappedSourceData;

// texture for the atomic writes
layout( r32ui ) uniform uimage2D vectorscopeImage;

// buffer for max pixel count
layout( binding = 5, std430 ) buffer perColumnMins { uint maxCount; };

// #include "hsvConversions.h"
#include "yuvConversions.h"
#include "mathUtils.h"

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	if ( all( lessThan( loc, imageSize( tonemappedSourceData ) ) ) ) {
		vec4 pixelColor = saturate( imageLoad( tonemappedSourceData, loc ) );

		// // find hue and saturation, by converting rgb to hsv
		// const vec3 hsvColor = rgb2hsv( pixelColor.rgb );
		// const float hue = hsvColor.r * 6.28f;
		// const float sat = hsvColor.g;
		// const ivec2 writePoint = ivec2( ( vec2(
		// 	sat * cos( hue ),
		// 	sat * sin( hue )
		// ) * 0.5f + vec2( 0.5f ) ) * imageSize( vectorscopeImage ) );

		const ivec2 writePoint = ivec2( vec2( -rgb_yccbccrc( pixelColor.rgb ).yz + vec2( 0.5f ) ) * imageSize( vectorscopeImage ) );

		// increment the plot + update maxcount
		atomicMax( maxCount, imageAtomicAdd( vectorscopeImage, writePoint, 1 ) + 1 );
	}
}