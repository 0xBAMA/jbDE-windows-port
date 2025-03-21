#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2 ) uniform sampler3D colorBlockFront;
layout( binding = 3 ) uniform sampler3D lightingBlock;

// =============================================================================
// rendering settings - consider packing these
uniform vec4 clearColor;
uniform float scale;
uniform float perspectiveFactor;
uniform float alphaPower;
uniform float blendFactor;
uniform float jitterFactor;
uniform int numSteps;

// basis vectors used to construct view ray
uniform mat3 invBasis;

// offsets, in texels
uniform ivec2 tileOffset;
uniform vec2 renderOffset;
uniform ivec2 noiseOffset;
// =============================================================================

#include "VoraldoCompatibility/intersect.h"

void getColorForPixel ( vec3 rO, vec3 rD, inout vec4 color ) {
	float tCurrent = tMax;
	const float stepSize = max( float( ( tMax - tMin ) / numSteps ), 0.001 );
	vec3 samplePosition = ( rO + tCurrent * rD + vec3( 1.0 ) ) / 2.0;
	vec4 newRead = texture( colorBlockFront, samplePosition );
	vec4 newLightRead = texture( lightingBlock, samplePosition );
	// vec4 newLightRead = vec4( 1.0 );
	for ( int i = 0; i < numSteps; i++ ) {
		if( tCurrent >= tMin ) {
			newRead.rgb *= newLightRead.xyz;
			float alphaSquared = pow( newRead.a, alphaPower ); // gives more usable range on the alpha channel
			// alpha blending, new sample over running color
			color.a = max( alphaSquared + color.a * ( 1.0 - alphaSquared ), 0.001 );
			color.rgb = newRead.rgb * alphaSquared + color.rgb * color.a * ( 1.0 - alphaSquared );
			color.rgb /= color.a; // missing piece of a over b alpha blending - can't really see a lot of difference
			tCurrent -= stepSize;
			samplePosition = ( rO + tCurrent * rD + vec3( 1.0 ) ) / 2.0;
			newRead = texture( colorBlockFront, samplePosition );
			newLightRead = texture( lightingBlock, samplePosition );
			// newLightRead = vec4( 1.0 );
		}
	}
}

void main () {
	const ivec2 invocation   = ivec2( gl_GlobalInvocationID.xy ) + tileOffset;
	const ivec2 location     = invocation + ivec2( renderOffset );
	const ivec2 iDimensions  = imageSize( accumulatorTexture );
	const vec2 dimensions    = vec2( iDimensions );
	const float aspectRatio  = dimensions.y / dimensions.x;
	const ivec2 noiseLoc     = ( noiseOffset + invocation ) % imageSize( blueNoiseTexture );
	const vec2 blueNoiseRead = jitterFactor * ( imageLoad( blueNoiseTexture, noiseLoc ).xy / 255.0 );
	const vec2 uv            = ( location + blueNoiseRead ) / dimensions;
	const vec2 mappedPos     = scale * ( ( uv - vec2( 0.5 ) ) * vec2( 1.0, aspectRatio ) );
	const vec3 rayOrigin     = invBasis * vec3( mappedPos, 2.0 );
	const vec3 rayDirection  = invBasis * vec3( perspectiveFactor * mappedPos, -2.0 );
	const vec4 prevColor     = imageLoad( accumulatorTexture, invocation );

	vec4 color = clearColor;
	if ( invocation.x < iDimensions.x && invocation.y < iDimensions.y ) {
		if ( Intersect( rayOrigin, rayDirection ) ) {
			getColorForPixel( rayOrigin, rayDirection, color );
		}
		imageStore( accumulatorTexture, invocation, color * ( 1.0 - blendFactor ) + prevColor * blendFactor );
	}
}
