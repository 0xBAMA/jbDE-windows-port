#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// util + output
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// results of the precompute step
layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D colorBuffer;

// forward pathtracing buffers
layout( r32ui ) uniform uimage3D redAtomic;
layout( r32ui ) uniform uimage3D greenAtomic;
layout( r32ui ) uniform uimage3D blueAtomic;
layout( r32ui ) uniform uimage3D countAtomic;

// sphere data
const int numSpheres = 65535;
struct sphere_t {
	vec4 positionRadius;
	vec4 colorMaterial;
};
layout( binding = 0, std430 ) buffer sphereData {
	sphere_t spheres[];
};

vec2 RaySphereIntersect ( vec3 r0, vec3 rd, vec3 s0, float sr ) {
	// r0 is ray origin
	// rd is ray direction
	// s0 is sphere center
	// sr is sphere radius
	// return is the roots of the quadratic, includes negative hits
	float a = dot( rd, rd );
	vec3 s0_r0 = r0 - s0;
	float b = 2.0f * dot( rd, s0_r0 );
	float c = dot( s0_r0, s0_r0 ) - ( sr * sr );
	float disc = b * b - 4.0f * a * c;
	if ( disc < 0.0f ) {
		return vec2( -1.0f, -1.0f );
	} else {
		return vec2( -b - sqrt( disc ), -b + sqrt( disc ) ) / ( 2.0f * a );
	}
}

mat3 Rotate3D ( const float angle, const vec3 axis ) {
	const vec3 a = normalize( axis );
	const float s = sin( angle );
	const float c = cos( angle );
	const float r = 1.0f - c;
	return mat3(
		a.x * a.x * r + c,
		a.y * a.x * r + a.z * s,
		a.z * a.x * r - a.y * s,
		a.x * a.y * r - a.z * s,
		a.y * a.y * r + c,
		a.z * a.y * r + a.x * s,
		a.x * a.z * r + a.y * s,
		a.y * a.z * r - a.x * s,
		a.z * a.z * r + c
	);
}

uniform ivec2 noiseOffset;
uvec4 blueNoiseRead ( ivec2 loc ) {
	ivec2 wrappedLoc = ( loc + noiseOffset ) % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	return sampleValue;
}

float blueNoiseRead ( ivec2 loc, int idx ) {
	vec4 sampleValue = blueNoiseRead( loc ) / 255.0f;
	switch ( idx ) {
	case 0:
		return sampleValue.r;
		break;
	case 1:
		return sampleValue.g;
		break;
	case 2:
		return sampleValue.b;
		break;
	case 3:
		return sampleValue.a;
		break;
	}
}

uniform int wangSeed;
#include "random.h"
#include "wood.h"

vec3 HenyeyGreensteinSampleSphere ( const vec3 n, const float g ) {
	float t = ( 0.5f - g *0.5) / ( 0.5f - g + 0.5f * g * NormalizedRandomFloat() );
	float cosTheta = ( 1.0f + g * g - t ) / ( 2.0f * g );
	float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
	float phi = 2.0f * 3.14159f * NormalizedRandomFloat();

	vec3 xyz = vec3( cos( phi ) * sinTheta, sin( phi ) * sinTheta, cosTheta );
	vec3 W = ( abs(n.x) > 0.99f ) ? vec3( 0.0f, 1.0f, 0.0f ) : vec3( 1.0f, 0.0f, 0.0f );
	vec3 N = n;
	vec3 T = normalize( cross( N, W ) );
	vec3 B = cross( T, N );
	return normalize( xyz.x * T + xyz.y * B + xyz.z * N );
}

vec3 SimpleRayScatter ( const vec3 n ) {
	return normalize( n + 0.1f * RandomUnitVector() );
}

void main () {
	// invocation index
	const ivec3 myLoc = ivec3( gl_GlobalInvocationID.xyz );
	const ivec3 size = imageSize( redAtomic );
	const uvec4 blueU = blueNoiseRead( myLoc.xy );
	const vec4 blue = blueU / 255.0f;

	// init rng
	seed = wangSeed + myLoc.x * 168 + myLoc.y * 451 + blueU.x * 69 + blueU.y * 420;

	vec3 colorScalars = vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() );

	// generate ray source point
	const vec3 offset = RandomUnitVector();
	// vec3 Origin = ( offset * 20.0f ) + vec3( 30.0f, 30.0f, 100.0f );
	// vec3 Direction = normalize( vec3( 1.0f, 2.3f, 0.0f ) );


	// vec3 Origin = vec3( 300.0f, 300.0f, 10.0f );
	// vec3 Direction = normalize( RandomUnitVector() + vec3( 0.1f, 0.3f, 0.75f ) );


	// vec3 Origin = vec3( NormalizedRandomFloat() * size.x / 10.0f + size.x / 2.0f, NormalizedRandomFloat() * size.y / 10.0f + size.y / 2.0f, NormalizedRandomFloat() * size.z + size.z / 2.0f );
	// vec3 Origin = vec3( NormalizedRandomFloat() * size.x / 3.0f + size.x / 2.0f, NormalizedRandomFloat() * size.y / 3.0f + size.y / 2.0f, NormalizedRandomFloat() * size.z + size.z / 2.0f );
	// vec3 Origin = vec3( ( NormalizedRandomFloat() - 0.5f ) * size.x / 10.0f + size.x / 2.0f, ( NormalizedRandomFloat() - 0.5f ) * size.y / 10.0f + size.y / 2.0f,  ( NormalizedRandomFloat() - 0.5f ) * size.z / 10.0f + size.z / 2.0f );
	// vec3 Origin = vec3( NormalizedRandomFloat() * size.x, NormalizedRandomFloat() * size.y, 5.0f );
	// vec3 Direction = normalize( vec3( Ra/ndomInUnitDisk(), 0.0f ) + 0.618f * RandomUnitVector() ).yzx;
	// vec3 Direction = RandomUnitVector();
	// vec3 Direction = vec3( RandomInUnitDisk(), 0.0f );
	// vec3 Direction = vec3( 0.0f, 0.0f, -1.0f );
	// vec3 Direction = vec3( 0.0f, 1.0f, 0.0f );

	// vec3 Origin = size / 2.0f + 10.0f * RandomUnitVector();
	// vec3 Direction = normalize( vec3( RandomInUnitDisk(), 1.0f ) );
	// vec3 Direction = RandomUnitVector();
	// Direction.xy = sign( Direction.xy );
	// Direction = sign( Direction );

	vec3 Origin = vec3( RandomInUnitDisk().xy * size.xy / 2.0f + size.xy / 2.0f, 10.0f );
	vec3 Direction = normalize( vec3( RandomInUnitDisk(), 1.0f ) );
	// vec3 Direction = RandomUnitVector();
	// vec3 Direction = vec3( 0.0f, 0.0f, 1.0f );

	// const bool pick = ( NormalizedRandomFloat() < 0.1f );
	// vec3 Origin = pick ? ( ( offset * 10.0f ) + vec3( 30.0f, 30.0f, 100.0f ) ) : vec3( NormalizedRandomFloat() * 600, 590, NormalizedRandomFloat() * 300 );
	// vec3 Direction = pick ? normalize( vec3( 1.0f, 1.0f, 0.0f ) + 0.2f * RandomUnitVector() ) : normalize( vec3( 0.0f, -1.0f, 0.0f ) + 0.2f * RandomUnitVector() );


	// vec3 Origin = vec3( UniformSampleHexagon( blue.xy ) * 100.0f + vec2( 300.0f, 300.0f ), NormalizedRandomFloat() * 20.0f + 90.0f );
	// vec3 Direction = normalize( Origin - vec3( 300.0f, 300.0f, 100.0f ) );


	// vec3 Origin = vec3( UniformSampleHexagon( blue.xy ) * 250.0f + vec2( 300.0f, 300.0f ), NormalizedRandomFloat() * 20.0f + 90.0f );
	// vec3 Direction = normalize( vec3( 300.0f, 300.0f, 100.0f ) - Origin );


	#define MAX_BOUNCES 20
	#define MAX_RAY_STEPS 2200
	for ( int b = 0; b < MAX_BOUNCES; b++ ) {
		// do the traversal
		vec3 deltaDist = 1.0f / abs( Direction );
		ivec3 rayStep = ivec3( sign( Direction ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( Origin + 0.0f ) );
		vec3 sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - Origin ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;

		for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, size ) ) ); i++ ) {
			// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
			bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
			vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
			ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

			// write some test values
			imageAtomicAdd( redAtomic, mapPos0, uint( colorScalars.x * 100 ) );
			imageAtomicAdd( greenAtomic, mapPos0, uint( colorScalars.y * 100 ) );
			imageAtomicAdd( blueAtomic, mapPos0, uint( colorScalars.z * 100 ) );
			imageAtomicAdd( countAtomic, mapPos0, 100 );

			// // evaluate chance to scatter, recompute traversal vars
			// if ( NormalizedRandomFloat() < 0.001f ) {
			// 	// multiply throughput by the albedo, if scattering occurs
			// 	Direction = SimpleRayScatter( Direction );
			// 	// Direction = HenyeyGreensteinSampleSphere( Direction, 0.76f );
			// 	deltaDist = 1.0f / abs( Direction );
			// 	rayStep = ivec3( sign( Direction ) );
			// 	mask0 = bvec3( false );
			// 	sideDist0 = ( ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;
			// }

			// const vec3 centerPt = vec3( 300, 300, 100 );
			// if ( distance( centerPt, mapPos0 ) < 80.0f ) {
			// 	vec2 cantidateHit = RaySphereIntersect( vec3( mapPos0 ), Direction, centerPt, 78.0f );
			// 	if ( cantidateHit.x >= 0.0f ) {
			// 		vec3 pos = vec3( mapPos0 ) + cantidateHit.x * Direction;
			// 		vec3 normal = normalize( pos - centerPt );
			// 		pos += 0.01f * normal;

			// 		if ( NormalizedRandomFloat() < 0.5f ) {
			// 			Direction = normalize( reflect( Direction, normal ) );
			// 		} else {
			// 			Direction = normalize( refract( Direction, normal, 1.0f / 1.3f ) );
			// 		}

			// 		colorScalars *= vec3( 1.0f, 0.3f, 0.1f );

			// 		// update traversal parameters
			// 		deltaDist = 1.0f / abs( Direction );
			// 		rayStep = ivec3( sign( Direction ) );
			// 		mask0 = bvec3( false );
			// 		ivec3 mapPos0 = ivec3( floor( pos + 0.0f ) );
			// 		imageAtomicAdd( redAtomic, mapPos0, uint( colorScalars.x * 10000 ) );
			// 		vec3 sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - pos ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;
			// 	}
			// }



		// load from the idx buffer to see if we need to check against a primitive
			uvec2 read = imageLoad( idxBuffer, mapPos0 ).xy;
			if ( read.r != 0 ) {
				// this should be the hit condition
				// break;

				// colorScalars *= spheres[ read.x ].colorMaterial.rgb * pow( spheres[ read.x ].colorMaterial.a, 5.0f );
				colorScalars = spheres[ read.x ].colorMaterial.rgb;
				// colorScalars = matWood( mapPos0 / 600.0f );

				vec3 offset = imageSize( idxBuffer ) / 2.0f;
				vec2 d = RaySphereIntersect( Origin, Direction, spheres[ read.x ].positionRadius.xyz + offset, spheres[ read.x ].positionRadius.w );
				if ( d.x >= 0.0 ) {
					Origin = Origin + Direction * d.x;
					vec3 Normal = normalize( Origin - ( spheres[ read.x ].positionRadius.xyz + offset ) );
					Origin += 0.01f * Normal;

					if ( read.r % 2 == 0 ) {
						Direction = reflect( Direction, Normal );
					} else {
						Direction = normalize( Normal * 1.001f + RandomUnitVector() );
					}

					deltaDist = 1.0f / abs( Direction );
					rayStep = ivec3( sign( Direction ) );
					mask0 = bvec3( false );
					mapPos0 = ivec3( floor( Origin + 0.0f ) );
					sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - Origin ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;

					break;

				}
			}

			sideDist0 = sideDist1;
			mapPos0 = mapPos1;
		}
	}

	// imageStore( colorBuffer, ivec3( Origin ), vec4( 1.0f, 0.1f, 0.1f, 1.0f ) );

}
