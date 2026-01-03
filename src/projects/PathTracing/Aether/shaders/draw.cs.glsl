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

#include "hg_sdf.glsl"
#include "aetherScene.h"

#include "intersect.h"
uniform mat3 invBasis;
uniform ivec3 dimensions;

uniform float scale;

ivec3 getRemappedPosition ( vec3 pos ) {
	return ivec3( -pos + vec3( imageSize( bufferImageY ).xyz / 2.0f ) );
}

const float scalar = 10000.0f;

float getDensity ( vec3 pos ) {
	ivec3 p = getRemappedPosition( pos );
	// return pow( imageLoad( bufferImageY, getRemappedPosition( pos ) ).r / scalar, 2.0f );//  + 0.0001f;
//	return exp( -imageLoad( bufferImageY, getRemappedPosition( pos ) ).r / scalar );//  + 0.0001f;
	return  xyz_to_xyY(
	( 1.0f / scalar ) * vec3( // these are tally sums + number of samples for averaging
	( float( imageLoad( bufferImageX, p ).r ) / 16.0f ),
	( float( imageLoad( bufferImageY, p ).r ) / 16.0f ),
	( float( imageLoad( bufferImageZ, p ).r ) / 16.0f ) )
	).b;
//	return 0.001f;
}

#include "spectrumXYZ.h"

vec3 getColor ( vec3 pos, float wavelength ) {
	ivec3 p = getRemappedPosition( pos );
	// return rgb_to_srgb( xyz_to_rgb(
	return xyY_to_rgb( xyz_to_xyY(
	( 1.0f / scalar ) * vec3( // these are tally sums + number of samples for averaging
	( float( imageLoad( bufferImageX, p ).r ) / 16.0f ),
	( float( imageLoad( bufferImageY, p ).r ) / 16.0f ),
	( float( imageLoad( bufferImageZ, p ).r ) / 16.0f ) ) * wavelengthColor( wavelength )
	) );// + vec3( 0.01f );
}

void main () {
	// pixel location
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// initial wang state
	seed = wangSeed + loc.x * 69696 + loc.y * 8675309;

	float wavelength = mix( 380.0f, 700.0f, NormalizedRandomFloat() );

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
	const vec3 blockSizeHalf = dimensions / 2.0f;
	vec3 p = origin;

	for ( int bounce = 0; bounce < 32; bounce++ ) {
		// up to three bounces... I want to be able to refract, and also scatter in the volume...
		bool hit = IntersectAABB( -p, -direction, -blockSizeHalf, blockSizeHalf, tMin, tMax );

		if ( !hit ) { break; } // if we are not inside the scatter volume, we're done
		p = origin + tMin * direction;

		intersectionResult intersection = sceneTrace( -p * 2.0f, -direction, wavelength );
		intersection.dist /= 2.0f; // compensating for scaling
//		if ( intersection.materialType != NOHIT ) {
//			col = 0.5f * intersection.normal + vec3( 0.5f );
//			break;
//		}

		// delta tracking raymarch...
		const int maxSteps = 10000;
		vec3 originCache = p;
		for ( int i = 0; i < maxSteps; i++ ) {
			float t = -log( NormalizedRandomFloat() );
			p += t * direction;

			if ( any( lessThan( p, -blockSizeHalf ) ) ||
			any( greaterThan( p, blockSizeHalf ) ) ) {
				// oob
				bounce = 1000;
				col += vec3( 0.001f );
				break;
			}

			// if you hit glass before scattering...
			if ( distance( originCache, p ) > intersection.dist ) {
				p = originCache + intersection.dist * direction;
				direction = refract( direction, intersection.normal, intersection.IoR );

//				col = 0.5f * intersection.normal + vec3( 0.5f );
//				i = 1000;
//				bounce = 1000;
//				break;

				// we need to do something about this... everything is mirrored, and it sucks
				intersection.normal *= -1.0f;

				p -= intersection.normal * epsilon * 3.0f;
				intersection.IoR = intersection.frontFacing ? ( 1.0f / intersection.IoR ) : ( intersection.IoR ); // "reverse" back to physical properties for IoR
				float cosTheta = min( dot( -normalize( direction ), intersection.normal ), 1.0f );
				float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
				bool cannotRefract = ( intersection.IoR * sinTheta ) > 1.0f; // accounting for TIR effects
				if ( cannotRefract || Reflectance( cosTheta, intersection.IoR ) > NormalizedRandomFloat() ) {
					direction = normalize( mix( reflect( normalize( direction ), intersection.normal ), RandomUnitVector(), intersection.roughness ) );
				} else {
					direction = normalize( mix( refract( normalize( direction ), intersection.normal, intersection.IoR ), RandomUnitVector(), intersection.roughness ) );
				}

				if ( any( bvec3( isnan( p.x ), isnan( p.y ), isnan( p.z ) ) ) || any( bvec3( isnan( direction.x ), isnan( direction.y ), isnan( direction.z ) ) ) ) {
					// we got in a bad situation
					col = vec3( 0.001f );
					i = 1000;
					bounce = 1000; // break out of loops
					break;
				}

				// state pump
				originCache = p;
				intersection = sceneTrace( -p * 2.0f, -direction, wavelength );
				intersection.dist /= 2.0f; // compensating for scaling

			// else see if we scatter
			} else if ( getDensity( p ) > NormalizedRandomFloat() ) {
				col = getColor( p, wavelength );
				i = 1000;
				bounce = 1000; // break out of loops
				break;
			}
		}
	}

	col = clamp( col, vec3( 0.0f ), vec3( 3.0f ) );

	// blending with the history
	vec4 previousColor = imageLoad( accumulatorTexture, loc );
	float sampleCount = max( 1.0f, previousColor.a + 1.0f );
	const float mixFactor = 1.0f / sampleCount;

	// write the data to the image
	imageStore( accumulatorTexture, loc, vec4( mix( previousColor.rgb, col, vec3( mixFactor ) ), sampleCount ) );
}
