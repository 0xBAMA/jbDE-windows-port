#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// tally buffers
layout( binding = 0, r32ui ) uniform uimage2D redTally;
layout( binding = 1, r32ui ) uniform uimage2D greenTally;
layout( binding = 2, r32ui ) uniform uimage2D blueTally;
layout( binding = 3, r32ui ) uniform uimage2D sampleTally;

// depth buffer, for clearing
layout( binding = 4, r32ui ) uniform uimage2D depthBuffer;

// id buffer
layout( binding = 5, rg32ui ) uniform uimage2D idBuffer;

// color attachment
layout( binding = 6, rgba16f ) uniform image2D compositedResult;

// line buffers
struct line_t {
	vec4 p0;
	vec4 p1;
	vec4 color0;
	vec4 color1;
};
layout( binding = 0, std430 ) buffer opaqueLineData { line_t opaqueData[]; };
layout( binding = 1, std430 ) buffer transparentLineData { line_t transparentData[]; };

vec4 blendColor( in vec4 under, in vec4 over ) {
	float a0 = ( over.a + under.a * ( 1.0f - over.a ) );
	return vec4( ( over.rgb * over.a + under.rgb * under.a * ( 1.0f - over.a ) ) / a0, a0 );
}

#include "mathUtils.h"
uniform float depthRange;

#include "random.h"
uniform int seedoffset;

void main() {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	seed = uint( 6969 * loc.x + 42069 * loc.y + seedoffset );

	// opaque color comes from index loaded from id buffer, indexing into the SSBO for opaqueData[ id ].color
	const uvec2 idx = uvec2( imageLoad( idBuffer, loc ).rg ) - uvec2( 1, 0 );
	vec4 opaqueColor = ( idx.r == 0 ) ? vec4( 0.0f ) : mix( opaqueData[ idx.r ].color0, opaqueData[ idx.r ].color1, uintBitsToFloat( idx.g ) );

	// transparent color resolve
	vec4 tallySamples = vec4( // similar scheme to Icarus
		float( imageLoad( redTally, loc ).r ) / ( 256.0f * 1024.0f ),
		float( imageLoad( greenTally, loc ).r ) / ( 256.0f * 1024.0f ),
		float( imageLoad( blueTally, loc ).r ) / ( 256.0f * 1024.0f ),
		float( imageLoad( sampleTally, loc ).r ) / 256.0f
	);
	// averaging out colors and get a Beer's-law-ish term for density
	// vec4 transparentColor = vec4( tallySamples.rgb / tallySamples.a, exp( -0.003f * tallySamples.a ) );
	vec4 transparentColor = vec4( tallySamples.rgb, sqrt( 1.0f - clamp( exp( -0.3f * tallySamples.a ), 0.0f, 1.0f ) ) );

	float zPos = RangeRemapValue( imageLoad( depthBuffer, loc ).r, 1.0f, float( UINT_MAX ), -depthRange, depthRange );
	opaqueColor.rgb *= RangeRemapValue( zPos, -depthRange, depthRange, 0.0f, 1.0f );

	// blend the transparent over the opaque
	vec3 finalColor = ( transparentColor.a == 0.0f ) ? opaqueColor.rgb : blendColor( opaqueColor, transparentColor ).rgb;
	// vec3 finalColor = opaqueColor.rgb;

	// store back in the composited result
	// imageStore( compositedResult, loc, vec4( mix( finalColor, imageLoad( compositedResult, ivec2( loc + 2.0f * UniformSampleHexagon( vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) ) ) ).rgb, 0.9f ), 1.0f ) );
	// imageStore( compositedResult, loc, vec4( finalColor, 1.0f ) );
	imageStore( compositedResult, loc, vec4( mix( finalColor, imageLoad( compositedResult, ivec2( loc ) ).rgb, 0.3f ), 1.0f ) );

	// // do the clears here
	// imageStore( redTally, loc, uvec4( 0 ) );
	// imageStore( greenTally, loc, uvec4( 0 ) );
	// imageStore( blueTally, loc, uvec4( 0 ) );
	// imageStore( sampleTally, loc, uvec4( 0 ) );
	// imageStore( depthBuffer, loc, uvec4( 0 ) );
	// imageStore( idBuffer, loc, uvec4( 0 ) );
}