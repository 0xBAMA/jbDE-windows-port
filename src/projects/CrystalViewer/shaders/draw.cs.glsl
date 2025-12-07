#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

layout( binding = 2, r32ui ) uniform uimage3D SplatBuffer;

#include "random.h"
uniform int wangSeed;
uniform float animRatio;

uniform vec3 color1;
uniform vec3 color2;

float getDensity ( vec3 p ) {
//	return 0.001f + float( imageLoad( SplatBuffer, ivec3( p ) ).r ) / 64.0f;
	 return 0.001f + pow( float( imageLoad( SplatBuffer, ivec3( p ) ).r ) / ( 4096.0f * 4096 * 3 ), 0.125f );
//	return 0.001f + exp( float( imageLoad( SplatBuffer, ivec3( p ) ).r ) );
//	return 0.001f + float( imageLoad( SplatBuffer, ivec3( p ) ).r ) / 10000.0f;
}

void main () {
	// seed value for RNG
	seed = wangSeed + gl_GlobalInvocationID.x * 69420 + gl_GlobalInvocationID.y * 6181;

	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 col = vec3( 0.0f );

	const float ratio = ( 21.0f / 9.0f );

	// width ( x ) is from 0 to 1... height ( y ) is based on the 21:9/~2.35:1 "cinematic" ratio
	const vec2 lowThresh = vec2( 0, 0.5f / ratio );
	const vec2 highThresh = vec2( 1, 1.0f - 0.5f / ratio );

	const vec2 uvMask = vec2( writeLoc.x + 0.5f, writeLoc.y + 0.5f ) / vec2( imageSize( accumulatorTexture ).xy );
	if ( all( lessThan( uvMask, highThresh ) ) &&
		all( greaterThan( uvMask, lowThresh ) ) ) {

		// todo: weyl
		const ivec3 iS = imageSize( SplatBuffer );
		const float numSamples = 4.0f;
		for ( int s = 0; s < numSamples; s++ ) {
//			vec2 uv = ( vec2( writeLoc ) + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) );
			vec2 uv = ( vec2( writeLoc ) + ( 2.0f ) * rnd_disc_cauchy() );
	//		const vec3 rO = vec3( uv.x, iS.y - uv.y + 1, iS.z + 1.0f ); // start at the top of the volume

			const vec3 rO = vec3( uv, iS.z + 1.0f );
			const vec3 rD = vec3( 0.0f, 0.0f, -1.0f );

			// DoF
//			const vec3 rO = vec3( uv, iS.z + 1.0f );
//			const vec3 rD = normalize( vec3( writeLoc, mix( iS.z / 3.0f, 2.0f * iS.z / 3.0f, animRatio ) ) - rO );

			// delta tracking raymarch...
			vec3 p = rO;
			const int maxSteps = 10000;
			const int maxStepsShadow = 10000;
			vec3 shadowTerm = vec3( 1.0f );
			for ( int i = 0; i < maxSteps; i++ ) {
				float t = -log( NormalizedRandomFloat() );
				p += t * rD;

				if ( any( lessThan( ivec3( p ), ivec3( 0 ) ) ) ||
					any( greaterThan( ivec3( p ), iS ) ) ) {
					// oob
					break;
				}

				// if you hit
				if ( getDensity( p ) > NormalizedRandomFloat() ) {

		//			shadowTerm = distance( rO, p );
		//			break;

					// raymarch towards the light
					const vec3 lightP = vec3( 100.0f * NormalizedRandomFloat() + 240.0f + 600.0f * animRatio, 0.0f, 160.0f + 300.0f * animRatio );
					const vec3 lightP2 = vec3( 200.0f + 400.0f * animRatio, 400.0f + 30.0f * animRatio, 300.0f );

					const vec3 lightDir = normalize( lightP - p );
					const vec3 lightDir2 = normalize( vec3( 1.0f, 1.0f - animRatio, 1.0f ) );
					const vec3 lightDir3 = normalize( lightP2 - p );

//					const vec3 lightDir = normalize( -vec3( 0.25f, 0.35f, -1.0f ) );
//					const vec3 lightDir3 = normalize( vec3( 0.0f, 0.0f, -100.0f ) + vec3( iS / 2.0f ) - p );
//					const float lightDist = length( vec3( iS / 2.0f ) - p );

					// shadow ray trace(s)
					vec3 pShadow = p;
					float d1 = 0.0f;
					for ( ; d1 < maxStepsShadow; ) {
						// light direction needs to go on renderconfig
						d1 += -log( NormalizedRandomFloat() );
						pShadow = p + d1 * lightDir;
						if ( getDensity( pShadow ) > NormalizedRandomFloat() ) {
							shadowTerm.r = 0.01f;
							break;
						}
						if ( any( lessThan( ivec3( pShadow ), ivec3( 0 ) ) ) ||
							any( greaterThan( ivec3( pShadow ), iS ) ) ) {
								// oob
							break;
						}
					}

					shadowTerm.g = 0.0f;

					float d2 = 0.0f;
					pShadow = p;
					for ( ; d2 < maxStepsShadow; ) {
						// light direction needs to go on renderconfig
						d2 += -log( NormalizedRandomFloat() );
						pShadow = p + d2 * lightDir3;
						if ( getDensity( pShadow ) > NormalizedRandomFloat() ) {
							shadowTerm.b = 0.01f;
							break;
						}
						if ( any( lessThan( ivec3( pShadow ), ivec3( 0 ) ) ) ||
						any( greaterThan( ivec3( pShadow ), iS ) ) ) {
							// oob
							break;
						}
					}
					//	col = vec3( shadowTerm / 128.0f );
//					col += ( vec3( 0.01f ) + vec3( 0.3f ) * shadowTerm.r + vec3( 0.0f, 0.0f, 0.2f ) * shadowTerm.g + vec3( 0.9f, 0.2f, 0.0f ) * shadowTerm.b ) / numSamples;
//					col += ( vec3( 0.01f ) + vec3( 0.99f ) * shadowTerm.r + mix( vec3( 0.0f, 0.5f + 0.24f * sin( animRatio * 33.0f ), 0.0f ), vec3( sin( animRatio * 10.0f ) * 0.3f + 0.4f, 0.0f, 0.0f ), vec3( animRatio ) ) * shadowTerm.b ) / numSamples;
					col += ( vec3( 0.01f ) + color1 * shadowTerm.r + color2 * shadowTerm.b ) / numSamples;

					break;
				}
			}
		}
	}



	// write the data to the image (+blending... uniform input to wipe accumulation on trident dirty flag)
	vec4 previousColor = imageLoad( accumulatorTexture, writeLoc );
	imageStore( accumulatorTexture, writeLoc, vec4( mix( previousColor.xyz, col, 1.0f / max( previousColor.a, 1.0f ) ), previousColor.a + 1.0f ) );
}
