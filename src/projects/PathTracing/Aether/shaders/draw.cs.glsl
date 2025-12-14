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

// the field buffer image
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

#include "wood.h"


//https://www.shadertoy.com/view/MtX3Ws // noise marble
vec2 cmul( vec2 a, vec2 b )  { return vec2( a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x ); }
vec2 csqr( vec2 a )  { return vec2( a.x*a.x - a.y*a.y, 2.*a.x*a.y  ); }

float getDensity ( vec3 pos ) {
//	return 0.001f;
	pos /= 500.0f;
//	return clamp( cos( pos.x ) * cos( pos.y ) * sin( pos.z ), 0.0f, 1.0f );
//	return matWood( pos ).r / 100.0f;

	float res = 0.;

	vec3 p = pos;
	vec3 c = p;
	for (int i = 0; i < 10; ++i) {
		p =.7*abs(p)/dot(p,p) -.7;
		p.yz= csqr(p.yz);
		p=p.zxy;
		res += exp(-19. * abs(dot(p,c)));

	}
	return res/200.;
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
	const vec3 origin = invBasis * vec3( 1000.0f * uv, -2000.0f );
	vec3 direction = invBasis * normalize( vec3( uv * 0.01f, 2.0f ) );
	float tMin = -10000.0f;
	float tMax = 10000.0f;

	// raymarching...
	vec3 blockSize = dimensions;
	bool hit = IntersectAABB( origin, direction, -blockSize / 2.0f, blockSize / 2.0f, tMin, tMax );

	// placeholder, we need to do a delta tracking process through the volume
	//col = vec3( ( hit ? ( tMax - tMin ) / 1000.0f : 0.0f ) );

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
			col = vec3( 1.0f );
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
