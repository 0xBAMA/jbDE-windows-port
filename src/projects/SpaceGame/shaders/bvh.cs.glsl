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
struct atlasEntry {
	uvec2 basePoint;
	uvec2 size;
};
layout( binding = 4, std430 ) readonly buffer atlasOffsets{ atlasEntry atlasEntries[]; };
layout( binding = 3 ) uniform sampler2D atlasTexture;

// this needs to go into the custom leaf test, for alpha testing
vec4 sampleSelectedTexture ( int texIndex, vec2 uv ) {
	// load the parameters to sample the atlas texture...
	atlasEntry myAtlasEntry = atlasEntries[ texIndex ];

	// get a sample from the atlas texture, based on the specified UV
	vec2 atlasSize = vec2( textureSize( atlasTexture, 0 ).xy );
	vec2 samplePoint = mix( vec2( myAtlasEntry.basePoint ) / atlasSize, vec2( myAtlasEntry.basePoint + myAtlasEntry.size ) / atlasSize, uv );
	return texture( atlasTexture, samplePoint );
}

#include "srgbConvertMini.h"

#define NODEBUFFER cwbvhNodes
#define TRIBUFFER cwbvhTris
#define TRAVERSALFUNC traverseFunc

vec4 blendedResult = vec4( 0.0f );
bool leafTestFunc ( vec3 origin, vec3 direction, inout uint index, inout float tmax, inout vec2 uv, vec3 e1, vec3 e2, vec4 v0 ) {

	// test against the triangle...
	const vec3 r = cross( direction.xyz, e1 );
	const float a = dot( e2, r );
	if ( abs( a ) < 0.0000001f ) return false;
	const float f = 1 / a;
	const vec3 s = origin.xyz - v0.xyz;
	const float u = f * dot( s, r );
	if ( u < 0 || u > 1 ) return false;
	const vec3 q = cross( s, e2 );
	const float v = f * dot( direction.xyz, q );
	if ( v < 0 || u + v > 1 ) return false;
	const float d = f * dot( e1, q );
	if ( d <= 0.0f || d >= tmax ) return false;
	uv = vec2( u, v ), tmax = d;
	index = floatBitsToUint( v0.w );

	// we need to interpolate the texcoord from the vertex data...
	const uint baseIndex = 3 * index;
	vec4 t0 = triangleData[ baseIndex + 1 ];
	vec4 t1 = triangleData[ baseIndex + 2 ];
	vec4 t2 = triangleData[ baseIndex + 0 ];
	// using the barycentrics
	vec2 sampleLocation = t0.xy * uv.x + t1.xy * uv.y + t2.xy * ( 1.0f - uv.x - uv.y );

	// and then perform the sample
	vec4 textureSample = vec4( sampleSelectedTexture( int( t0.z ), sampleLocation ) );
	// vec4 textureSample = vec4( 1.0f, sampleLocation, 1.0f );
	if ( textureSample.a != 0.0f ) {
		/*
		// blending the blended result over the texture sample, it's a little weird but it's how the traversal will encounter them
		vec4 previousColor = textureSample;
		previousColor.rgb = rgb_to_srgb( previousColor.rgb );

		// alpha blending, new sample over running color
		float alphaSquared = pow( blendedResult.a, 2.0f );
		previousColor.a = max( alphaSquared + previousColor.a * ( 1.0f - alphaSquared ), 0.001f );
		previousColor.rgb = blendedResult.rgb * alphaSquared + previousColor.rgb * previousColor.a * ( 1.0f - alphaSquared );
		previousColor.rgb /= previousColor.a;

		blendedResult.rgb = srgb_to_rgb( previousColor.rgb );
		blendedResult.a = textureSample.a;
		*/
		blendedResult = textureSample;
		return true;
	} else {
		return false;
	}
}

#define CUSTOMLEAFTEST leafTestFunc
#define LEAFTEST2

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC
#undef CUSTOMLEAFTEST
#undef LEAFTEST2

// ray trace helper
vec4 rayTrace ( vec3 origin, vec3 direction ) {
	blendedResult = vec4( 0.0f ); // reset state for the blended ray
	return traverseFunc( origin, direction, tinybvh_safercp( direction ), 1e30f );
}

// SSBO for the atlas texture
// the atlas texture itself

uniform vec2 centerPoint;
uniform float globalZoom;

void main () {
	// screen UV
	vec2 iS = imageSize( accumulatorTexture ).xy;
	vec4 color = vec4( 0.0f );

	vec4 blendedResultSum = vec4( 0.0f );
	float countHits = 0;
	const int AAf = 3;
	for ( int AAx = 0; AAx < AAf; AAx++ ) {
		for ( int AAy = 0; AAy < AAf; AAy++ ) {
			vec2 centeredUV = ( gl_GlobalInvocationID.xy + vec2( blueNoiseRef( ivec2( AAx, AAy ) ).xy ) ) / iS - vec2( 0.5f );
			// vec2 centeredUV = ( gl_GlobalInvocationID.xy + vec2( blueNoiseRef( ivec2( gl_GlobalInvocationID.xy ) ).xy ) ) / iS - vec2( 0.5f );
			centeredUV.x *= ( iS.x / iS.y );
			centeredUV.y *= -1.0f;

			centeredUV *= globalZoom; // we need to know this for the UI, so it has to be a uniform
			centeredUV += centerPoint;

			// traverse the BVH from above, rays pointing down the z axis
			// traversal will eventually do a custom leaf test, looking at the atlas texture for an alpha value
			vec4 result = rayTrace( vec3( centeredUV, 1001.0f ), vec3( 0.0f, 0.0f, -1.0f ) );
			if ( result.x < 1e30f ) {
				// get a sample of the blended color...
				// blendedResultSum.a += blendedResult.a;
				// blendedResultSum.rgb += srgb_to_rgb( blendedResult.rgb );

				// blendedResultSum.a += 1.0f;
				// blendedResultSum.rgb += vec3( 1.0f, 0.0f, 0.0f );
				// blendedResultSum.rgb += rgb_to_srgb( vec3( result.yz, 1.0f - result.y - result.z ) );
				// countHits++;

				// if ( blendedResult.a > 0.0f ) {
					// blendedResultSum += blendedResult;
					// countHits += blendedResult.a;
				// }

				color += blendedResult;
				countHits += 1.0f;
			}
		}
	}

	if ( countHits > 0.2f ) {
		// use the color data from the traversal...
			// it might need to do a small amount of alpha blending with 1 > alpha > 0, so you avoid having to do it twice by doing it there

		color = color / countHits;
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