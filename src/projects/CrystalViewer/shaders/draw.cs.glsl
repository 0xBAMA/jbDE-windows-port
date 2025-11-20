#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

layout( binding = 2, r32ui ) uniform uimage3D SplatBuffer;

#include "random.h"
uniform int wangSeed;

void main () {
	// seed value for RNG
	seed = wangSeed + gl_GlobalInvocationID.x * 69420 + gl_GlobalInvocationID.y * 6181;

	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 col = vec3( 0.0f );

	// todo: weyl
	const ivec3 iS = imageSize( SplatBuffer );
	const float numSamples = 4.0f;
	for ( int s = 0; s < numSamples; s++ ) {
		vec2 uv = ( vec2( writeLoc ) + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) );
		const vec3 rO = vec3( uv, iS.z + 1.0f ); // start at the top of the volume
		const vec3 rD = vec3( 0.0f, 0.0f, -1.0f );

		// delta tracking raymarch...
		vec3 p = rO;
		const int maxSteps = 10000;
		float shadowTerm = 1.0f;
		for ( int i = 0; i < maxSteps; i++ ) {
			float t = -log( NormalizedRandomFloat() );
			p += t * rD;

			if ( any( lessThan( ivec3( p ), ivec3( 0 ) ) ) ||
				any( greaterThan( ivec3( p ), iS ) ) ) {
				// oob
				break;
			}

			// if you hit
			if ( ( float( imageLoad( SplatBuffer, ivec3( p ) ).r ) / 64.0f ) > NormalizedRandomFloat() ) {

	//			shadowTerm = distance( rO, p );
	//			break;

				// raymarch towards the light
				const vec3 lightDir = normalize( vec3( 1.0f, 1.0f, 1.0f ) ) ;
	//			const vec3 lightP = vec3( 10.0f * NormalizedRandomFloat() + 240.0f, 720.0f, 60.0f + 10.0f * NormalizedRandomFloat() );
	//			vec3 lightDir = lightP - p;

				// shadow ray trace(s)
				vec3 pShadow = p;
				for ( int j = 0; j < 10000; j++ ) {
					// light direction needs to go on renderconfig
					pShadow += lightDir * -log( NormalizedRandomFloat() );
					if ( ( float( imageLoad( SplatBuffer, ivec3( pShadow ) ).r ) / 64.0f ) > NormalizedRandomFloat() ) {
						shadowTerm = 0.0f;
						break;
					}
					if ( any( lessThan( ivec3( pShadow ), ivec3( 0 ) ) ) ||
						any( greaterThan( ivec3( pShadow ), iS ) ) ) {
							// oob
						break;
					}
				}
				//	col = vec3( shadowTerm / 128.0f );
				col += ( vec3( 0.01f ) + vec3( 0.5f, 0.1f, 0.0f ) * shadowTerm ) / numSamples;
				break;
			}
		}
	}


	// write the data to the image (+blending... uniform input to wipe accumulation on trident dirty flag)
	vec4 previousColor = imageLoad( accumulatorTexture, writeLoc );
	imageStore( accumulatorTexture, writeLoc, vec4( mix( previousColor.xyz, col, 1.0f / max( previousColor.a, 1.0f ) ), previousColor.a + 1.0f ) );
}
