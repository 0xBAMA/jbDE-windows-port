#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform usampler2D continuum;
uniform sampler3D shadedVolume;
uniform vec2 resolution;
uniform vec3 color;
uniform vec3 blockSize;
uniform float brightness;
uniform float scale;
uniform float IoR;
uniform mat3 invBasis;

#define PI 3.1415f

#include "intersect.h"

// ==============================================================================================
// Hash by David_Hoskins
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uvec2(UI0, UI1)
#define UI3 uvec3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))

vec3 hash33( vec3 p ) {
	uvec3 q = uvec3( ivec3( p ) ) * UI3;
	q = ( q.x ^ q.y ^ q.z )*UI3;
	return -1.0 + 2.0 * vec3( q ) * UIF;
}

// Gradient noise by iq (modified to be tileable)
float gradientNoise( vec3 x, float freq ) {
    // grid
    vec3 p = floor( x );
    vec3 w = fract( x );
    
    // quintic interpolant
    vec3 u = w * w * w * ( w * ( w * 6.0 - 15.0 ) + 10.0 );

    // gradients
    vec3 ga = hash33( mod( p + vec3( 0.0, 0.0, 0.0 ), freq ) );
    vec3 gb = hash33( mod( p + vec3( 1.0, 0.0, 0.0 ), freq ) );
    vec3 gc = hash33( mod( p + vec3( 0.0, 1.0, 0.0 ), freq ) );
    vec3 gd = hash33( mod( p + vec3( 1.0, 1.0, 0.0 ), freq ) );
    vec3 ge = hash33( mod( p + vec3( 0.0, 0.0, 1.0 ), freq ) );
    vec3 gf = hash33( mod( p + vec3( 1.0, 0.0, 1.0 ), freq ) );
    vec3 gg = hash33( mod( p + vec3( 0.0, 1.0, 1.0 ), freq ) );
    vec3 gh = hash33( mod( p + vec3( 1.0, 1.0, 1.0 ), freq ) );
    
    // projections
    float va = dot( ga, w - vec3( 0.0, 0.0, 0.0 ) );
    float vb = dot( gb, w - vec3( 1.0, 0.0, 0.0 ) );
    float vc = dot( gc, w - vec3( 0.0, 1.0, 0.0 ) );
    float vd = dot( gd, w - vec3( 1.0, 1.0, 0.0 ) );
    float ve = dot( ge, w - vec3( 0.0, 0.0, 1.0 ) );
    float vf = dot( gf, w - vec3( 1.0, 0.0, 1.0 ) );
    float vg = dot( gg, w - vec3( 0.0, 1.0, 1.0 ) );
    float vh = dot( gh, w - vec3( 1.0, 1.0, 1.0 ) );
	
    // interpolation
    return va + 
           u.x * ( vb - va ) + 
           u.y * ( vc - va ) + 
           u.z * ( ve - va ) + 
           u.x * u.y * ( va - vb - vc + vd ) + 
           u.y * u.z * ( va - vc - ve + vg ) + 
           u.z * u.x * ( va - vb - ve + vf ) + 
           u.x * u.y * u.z * ( -va + vb + vc - vd + ve - vf - vg + vh );
}

float perlinfbm( vec3 p, float freq, int octaves ) {
    float G = exp2( -0.85 );
    float amp = 1.0;
    float noise = 0.0;
    for ( int i = 0; i < octaves; ++i ) {
        noise += amp * gradientNoise( p * freq, freq );
        freq *= 2.0;
        amp *= G;
    }
    return noise;
}

// ==============================================================================================
vec3 eliNormal ( in vec3 pos, in vec3 center, in vec3 radii ) {
	return normalize( ( pos - center ) / ( radii * radii ) );
}
float eliIntersect ( in vec3 ro, in vec3 rd, in vec3 center, in vec3 radii ) {
	vec3 oc = ro - center;
	vec3 ocn = oc / radii;
	vec3 rdn = rd / radii;
	float a = dot( rdn, rdn );
	float b = dot( ocn, rdn );
	float c = dot( ocn, ocn );
	float h = b * b - a * ( c - 1.0f );
	if ( h < 0.0f ) return -1.0f;
	return ( -b - sqrt( h ) ) / a;
}

float RemapRange ( const float value, const float iMin, const float iMax, const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) / ( iMax - iMin ) ) * ( value - iMin ) );
}

float blueNoiseRead ( ivec2 loc, int idx ) {
	ivec2 wrappedLoc = loc % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	switch ( idx ) {
	case 0:
		return sampleValue.r / 255.0f;
		break;
	case 1:
		return sampleValue.g / 255.0f;
		break;
	case 2:
		return sampleValue.b / 255.0f;
		break;
	case 3:
		return sampleValue.a / 255.0f;
		break;
	}
}

void main () {
	// remapped uv
	vec2 subpixelOffset = vec2(
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 1 ),
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 2 )
	);
	// vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.5f ) ) / resolution.xy;
	vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + subpixelOffset ) / resolution.xy;
	uv = ( uv - 0.5f ) * 2.0f;

	// aspect ratio correction
	uv.x *= ( resolution.x / resolution.y );

	// box intersection
	float tMin, tMax;
	vec3 Origin = invBasis * vec3( scale * uv, -2.0f );
	vec3 Direction = invBasis * normalize( vec3( uv * 0.1f, 2.0f ) );

	// first, intersecting with a sphere, to refract
	// https://www.shadertoy.com/view/MlsSzn - tighter fitting ellipsoid makes more sense, I think
	// float sHit = eliIntersect( Origin, Direction, vec3( 0.0f ), blockSize );
	// float sHit = eliIntersect( Origin, Direction, vec3( 0.0f ), ( blockSize / 2.0f ) * 1.2f );
	float sHit = eliIntersect( Origin, Direction, vec3( 0.0f ), ( blockSize / 2.0f ) );
	if ( sHit > 0.0f ) {
		// update ray position to be at the sphere's surface
		Origin = Origin + sHit * Direction;
		// update ray direction to the refracted ray
		Direction = refract( Direction, eliNormal( Origin, vec3( 0.0f ), blockSize / 2.0f ), IoR );

		// then intersect with the AABB
		const bool hit = IntersectAABB( Origin, Direction, -blockSize / 2.0f, blockSize / 2.0f, tMin, tMax );

		if ( hit ) { // texture sample
			// for trimming edges
			const float epsilon = -0.003f; 
			const vec3 hitpointMin = Origin + tMin * Direction;
			const vec3 hitpointMax = Origin + tMax * Direction;
			const vec3 blockUVMin = vec3(
				RemapRange( hitpointMin.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
				RemapRange( hitpointMin.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
				RemapRange( hitpointMin.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0.0f - epsilon, 1.0f + epsilon )
			);

			const vec3 blockUVMax = vec3(
				RemapRange( hitpointMax.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
				RemapRange( hitpointMax.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
				RemapRange( hitpointMax.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0.0f - epsilon, 1.0f + epsilon )
			);

			const float thickness = abs( tMin - tMax );
			const float stepSize = max( thickness / 10.0f, 0.001f );
			const vec3 displacement = normalize( blockUVMax - blockUVMin );

			const int numSteps = 700;
			int iterationsToHit = 0;
			vec3 samplePoint = blockUVMin + blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 0 ) * 0.001f;
			vec3 final = vec3( 0.01f );
			for ( ; iterationsToHit < numSteps; iterationsToHit++ ) {
				samplePoint += stepSize * displacement;
				float result = brightness * ( texture( continuum, samplePoint.xy ).r / 1000000.0f );
				if ( result > ( ( samplePoint.z - 0.5f ) * 2.0f ) ) {
					break;
				}
				// vec3 iterationColor = brightness * result * color + vec3( 0.003f );
				// vec3 iterationColor = color;
				vec3 p = samplePoint * 3.0f;
				// vec3 iterationColor = ( step( 0.0f, cos( PI * p.x + PI / 2.0f ) * cos( PI * p.y + PI / 2.0f ) ) == 0 ) ? vec3( 0.618f ) : vec3( 0.1618f );
				// vec3 iterationColor = ( perlinfbm( p, 5.0f, 3 ) < 0.0f ) ? vec3( 0.618f ) : vec3( 0.1618f );
				vec3 iterationColor = color;
				// vec3 iterationColor = mix( ( step( 0.0f, cos( PI * p.x + PI / 2.0f ) * cos( PI * p.y + PI / 2.0f ) ) == 0 ) ? color : vec3( 0.1f ), vec3( 1.0f, 0.0f, 0.0f ), sin( vec3( samplePoint.z * samplePoint.z ) ) );
				final = mix( final, iterationColor, 0.01f );
			}
			// final = mix( final, brightness * ( texture( continuum, samplePoint.xy ).r / 1000000.0f ) * color + vec3( 0.003f ), 0.2f );
			// vec3 final = color * exp( -distance( samplePoint, Origin ) );
			final = color * exp( -distance( samplePoint, Origin ) );
			imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( final, 1.0f ) );
		} else {
			imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( vec3( 0.01618f ), 1.0f ) );
		}
	} else {
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( 0.0, 0.0f, 0.0f, 1.0f ) );
	}
}