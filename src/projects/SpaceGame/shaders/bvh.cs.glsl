#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2 ) uniform sampler2D selectedTexture;
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

#include "srgbConvertMini.h"

void main () {
	// screen UV
	vec2 iS = imageSize( accumulatorTexture ).xy;
	vec2 centeredUV = gl_GlobalInvocationID.xy / iS - vec2( 0.5f );
	centeredUV.x *= ( iS.x / iS.y );
	centeredUV.y *= -1.0f;

	centeredUV *= 20.0f; // what should this scale factor be?
	centeredUV += centerPoint;

	// traverse the BVH from above, rays pointing down the z axis
	// traversal will eventually do a custom leaf test, looking at the atlas texture for an alpha value

	vec4 result = rayTrace( vec3( centeredUV, 100.0f ), vec3( 0.0f, 0.0f, -1.0f ) );

	vec4 color = vec4( 0.0f );
	bool hit = ( result.x < 1e30f );
	if ( hit ) {
		// use the color data from the traversal...
			// it might need to do a small amount of alpha blending with 1 > alpha > 0, so you avoid having to do it twice by doing it there

		color = vec4( 1.0f, 0.0f, 0.0f, 1.0f );
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