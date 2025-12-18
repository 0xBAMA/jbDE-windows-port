#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// RNG seeding...
#include "random.h"
uniform int wangSeed;

// blue noise
uniform ivec2 noiseOffset;
vec4 blueNoiseRef ( ivec2 pos ) {
	pos.x = ( pos.x + noiseOffset.x ) % imageSize( blueNoiseTexture ).x;
	pos.y = ( pos.y + noiseOffset.y ) % imageSize( blueNoiseTexture ).y;
	return imageLoad( blueNoiseTexture, pos ) / 255.0f;
}

// the field buffer should probably use a texture interface
//uniform isampler3D bufferImageX;
//uniform isampler3D bufferImageY;
//uniform isampler3D bufferImageZ;
//uniform isampler3D bufferImageCount;
layout( binding = 2, r32ui ) uniform uimage3D bufferImageX;
layout( binding = 3, r32ui ) uniform uimage3D bufferImageY;
layout( binding = 4, r32ui ) uniform uimage3D bufferImageZ;
layout( binding = 5, r32ui ) uniform uimage3D bufferImageCount;

#include "colorspaceConversions.glsl"

#include "intersect.h"
uniform mat3 invBasis;
uniform ivec3 dimensions;

uniform float scale;

ivec3 getRemappedPosition ( vec3 pos ) {
	return ivec3( pos + vec3( imageSize( bufferImageY ).xyz / 2.0f ) );
}

const float scalar = 10000.0f;

float getDensity ( vec3 pos ) {
	return imageLoad( bufferImageY, getRemappedPosition( pos ) ).r / scalar;//  + 0.0001f;
}

vec3 getColor ( vec3 pos ) {
	ivec3 p = getRemappedPosition( pos );
	return rgb_to_srgb( xyz_to_rgb( ( 1.0f / scalar ) * vec3( // these are tally sums + number of samples for averaging
	( float( imageLoad( bufferImageX, p ).r ) / 16.0f ),
	( float( imageLoad( bufferImageY, p ).r ) / 16.0f ),
	( float( imageLoad( bufferImageZ, p ).r ) / 16.0f )
) ) );// + vec3( 0.01f );
}

void main () {
	// pixel location
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// initial wang state
	seed = wangSeed + loc.x * 69696 + loc.y * 8675309;

	// jittered pixel UV
	vec2 samplePoint = vec2( loc + blueNoiseRef( loc ).xy + 0.0618f * rnd_disc_cauchy() ) / imageSize( accumulatorTexture ).xy;
	samplePoint.y = 1.0f - samplePoint.y;

	// do the rendering...
	vec3 col = vec3( 0.0f );

	// create a view ray...
	vec2 uv = ( samplePoint - vec2( 0.5f ) );
	uv.x *= ( float( imageSize( accumulatorTexture ).x ) / float( imageSize( accumulatorTexture ).y ) );
	const vec3 origin = invBasis * vec3( scale * uv, -2000.0f );
	vec3 direction = invBasis * normalize( vec3( uv * 0.01f, 2.0f ) );
	float tMin = -10000.0f;
	float tMax = 10000.0f;

	// raymarching...
	vec3 blockSize = dimensions;
	bool hit = IntersectAABB( origin, direction, -blockSize / 2.0f, blockSize / 2.0f, tMin, tMax );

	// delta tracking raymarch...
	vec3 p = origin + tMin * direction;
	const int maxSteps = 10000;
	for ( int i = 0; i < maxSteps; i++ ) {
		float t = -log( NormalizedRandomFloat() );
		p += t * direction;

		if ( any( lessThan( p, -blockSize / 2.0f ) ) ||
			any( greaterThan( p, blockSize / 2.0f ) ) ) {
			// oob
			break;
		}

		// if you hit
		if ( getDensity( p ) > NormalizedRandomFloat() ) {
			col = getColor( p );
		}
	}

	// blending with the history
	vec4 previousColor = imageLoad( accumulatorTexture, loc );
	float sampleCount = max( 1.0f, previousColor.a + 1.0f );
	const float mixFactor = 1.0f / sampleCount;

	// write the data to the image
	imageStore( accumulatorTexture, loc, vec4( mix( previousColor.rgb, col, vec3( mixFactor ) ), sampleCount ) );
//	imageStore( accumulatorTexture, loc, vec4( col, 1.0f ) );
}
