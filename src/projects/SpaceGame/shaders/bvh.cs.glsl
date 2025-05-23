#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2 ) uniform sampler2D selectedTexture;
//=============================================================================================================================
uniform ivec2 noiseOffset;
vec4 blueNoiseRef( ivec2 pos ) {
	pos.x = ( pos.x + noiseOffset.x ) % imageSize( blueNoiseTexture ).x;
	pos.y = ( pos.y + noiseOffset.y ) % imageSize( blueNoiseTexture ).y;
	return imageLoad( blueNoiseTexture, pos ) / 255.0f;
}
//=============================================================================================================================
// SSBOs for the BVH - nodes + triangle data
layout( binding = 0, std430 ) readonly buffer cwbvhNodesBuffer { vec4 cwbvhNodes[]; };
layout( binding = 1, std430 ) readonly buffer cwbvhTrisBuffer { vec4 cwbvhTris[]; };
layout( binding = 2, std430 ) readonly buffer triangleDataBuffer { vec4 triangleData[]; };
//=============================================================================================================================
// gpu-side code for ray-BVH traversal
	// used for computing rD, reciprocal direction
float tinybvh_safercp( const float x ) { return x > 1e-12f ? ( 1.0f / x ) : ( x < -1e-12f ? ( 1.0f / x ) : 1e30f ); }
vec3 tinybvh_safercp( const vec3 x ) { return vec3( tinybvh_safercp( x.x ), tinybvh_safercp( x.y ), tinybvh_safercp( x.z ) ); }
//=============================================================================================================================

#define NODEBUFFER cwbvhNodes
#define TRIBUFFER cwbvhTris
#define TRAVERSALFUNC traverseFunc

// this will change to do an alpha test in the leaf node, allowing quads to be alpha masked
#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC

// ray trace helper
vec4 rayTrace ( vec3 origin, vec3 direction ) {
	return traverseFunc( origin, direction, tinybvh_safercp( direction ), 1e30f );
}

// SSBO for the atlas texture
// the atlas texture itself

uniform vec2 centerPoint;
uniform float globalZoom;

#include "srgbConvertMini.h"


void main () {
	// screen UV
	vec2 iS = imageSize( accumulatorTexture ).xy;

	int countHits = 0;
	const int AAf = 4;
	for ( int AAx = 0; AAx < AAf; AAx++ ) {
		for ( int AAy = 0; AAy < AAf; AAy++ ) {
			vec2 centeredUV = ( gl_GlobalInvocationID.xy + vec2( blueNoiseRef( ivec2( AAx, AAy ) ).xy ) ) / iS - vec2( 0.5f );
			centeredUV.x *= ( iS.x / iS.y );
			centeredUV.y *= -1.0f;

			centeredUV *= globalZoom; // we need to know this for the UI, so it has to be a uniform
			centeredUV += centerPoint;

			// traverse the BVH from above, rays pointing down the z axis
			// traversal will eventually do a custom leaf test, looking at the atlas texture for an alpha value
			if ( rayTrace( vec3( centeredUV, 100.0f ), vec3( 0.0f, 0.0f, -1.0f ) ).x < 1e30f ) {
				countHits++;
			}
		}
	}

	vec4 color = vec4( 0.0f );
	if ( countHits != 0 ) {
		// use the color data from the traversal...
			// it might need to do a small amount of alpha blending with 1 > alpha > 0, so you avoid having to do it twice by doing it there

		color = vec4( 1.0f, 0.0f, 0.0f, pow( countHits / float( AAf * AAf ), 0.5f ) );
	}

	if ( color.a != 0.0f ) {
		// blend with the accumulator contents...
		vec4 previousColor = imageLoad( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ) );
		previousColor.rgb = rgb_to_srgb( previousColor.rgb );

		// alpha blending, new sample over running color
		float alphaSquared = pow( color.a, 2.0f );
		previousColor.a = max( alphaSquared + previousColor.a * ( 1.0f - alphaSquared ), 0.001f );
		previousColor.rgb = color.rgb * alphaSquared + previousColor.rgb * previousColor.a * ( 1.0f - alphaSquared );
		previousColor.rgb /= previousColor.a;

		color.rgb = srgb_to_rgb( previousColor.rgb );
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( color.rgb, 1.0f ) );
	}
}