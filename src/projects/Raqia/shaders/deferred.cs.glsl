#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, rgba32ui ) uniform uimage2D deferredResult1;
layout( binding = 3, rgba32ui ) uniform uimage2D deferredResult2;
layout( binding = 4, r32ui ) uniform uimage2D deferredResult3;
layout( binding = 5 ) uniform sampler3D lightCache1;
layout( binding = 6 ) uniform sampler3D lightCache2;
layout( binding = 7 ) uniform sampler3D lightCache3;
//=============================================================================================================================
// gpu-side code for ray-BVH traversal
	// used for computing rD, reciprocal direction
float tinybvh_safercp( const float x ) { return x > 1e-12f ? ( 1.0f / x ) : ( x < -1e-12f ? ( 1.0f / x ) : 1e30f ); }
vec3 tinybvh_safercp( const vec3 x ) { return vec3( tinybvh_safercp( x.x ), tinybvh_safercp( x.y ), tinybvh_safercp( x.z ) ); }
//=============================================================================================================================
// node and triangle data specifically for the BVH
layout( binding = 0, std430 ) readonly buffer cwbvhNodesBuffer { vec4 cwbvhNodes[]; };
layout( binding = 1, std430 ) readonly buffer cwbvhTrisBuffer { vec4 cwbvhTris[]; };
// vertex data for the individual triangles' vertices, in a usable format ( .w can be used to pack more data )
layout( binding = 2, std430 ) readonly buffer triangleDataBuffer { vec4 triangleData[]; };

// second set, for the grass blades
layout( binding = 3, std430 ) readonly buffer cwbvhNodesBuffer2 { vec4 cwbvhNodes2[]; };
layout( binding = 4, std430 ) readonly buffer cwbvhTrisBuffer2 { vec4 cwbvhTris2[]; };
layout( binding = 5, std430 ) readonly buffer triangleDataBuffer2 { vec4 triangleData2[]; };
//=============================================================================================================================
#include "consistentPrimitives.glsl.h" // ray-sphere, ray-box inside traverse.h
#include "noise.h"
#include "pbrConstants.glsl"

#define NODEBUFFER cwbvhNodes
#define TRIBUFFER cwbvhTris
#define TRAVERSALFUNC traverse_cwbvh_terrain

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC

#define NODEBUFFER cwbvhNodes2
#define TRIBUFFER cwbvhTris2
#define TRAVERSALFUNC traverse_cwbvh_grass

vec3 grassColorLeaf = vec3( 0.0f );
bool leafTestFunc ( vec3 origin, vec3 direction, uint index, inout float tmax, inout vec2 uv ) {
	// test against N triangles for one blade of grass
		// initially, we will just be doing a single triangle

	const uint baseIndex = 4 * index;
	vec4 v0 = triangleData2[ baseIndex + 0 ];
	vec4 v1 = triangleData2[ baseIndex + 1 ];
	vec4 v2 = triangleData2[ baseIndex + 2 ];

// moller trombore on a dynamic triangle
	// precompute vectors representing edges - this computation should moved to the grass shader, out of the traversal code... should be at least a bit of a perf win
	const vec3 e1 = v1.xyz - v0.xyz; // edge1 = vertex1 - vertex0
	const vec3 e2 = v2.xyz - v0.xyz; // edge2 = vertex2 - vertex0

	const vec3 r = cross( direction.xyz, e1 );
	const float a = dot( e2, r );
	if ( abs( a ) < 0.0000001f )
		return false;
	const float f = 1.0f / a;
	const vec3 s = origin.xyz - v0.xyz;
	const float u = f * dot( s, r );
	if ( u < 0 || u > 1 )
		return false;
	const vec3 q = cross( s, e2 );
	const float v = f * dot( direction.xyz, q );
	if ( v < 0 || u + v > 1 )
		return false;
	const float d = f * dot( e1, q );
	if ( d <= 0.0f || d >= tmax )
		return false;
	uv = vec2( u, v ), tmax = d;
	grassColorLeaf = vec3( v0.w, v1.w, v2.w );
	return true;
}
#define CUSTOMLEAFTEST leafTestFunc

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC
#undef CUSTOMLEAFTEST

#include "random.h"
#include "normalEncodeDecode.h"
//=============================================================================================================================

uniform float time;
uniform float blendAmount;
uniform ivec2 blueNoiseOffset;
uniform float terrainBrightnessScalar;
uniform int debugDrawMode;

// volumetrics enable/disable
uniform bool volumetricsEnable;

// volumetrics color and density
uniform vec4 volumeColor;

// light level applied to all geometry
uniform vec4 ambientLightLevel;

// enable flags for the three lights
uniform bvec3 lightEnable;

// Key Light
uniform vec3 lightDirections0[ 16 ];
uniform vec4 lightColor0;

// Fill Light
uniform vec3 lightDirections1[ 16 ];
uniform vec4 lightColor1;

// Back Light
uniform vec3 lightDirections2[ 16 ];
uniform vec4 lightColor2;

//=============================================================================================================================
// bayer matrix for indexing into the queues
const int bayerMatrix[ 16 ] = int [] (
	 0,  8,  2, 10,
	12,  4, 14,  6,
	 3, 11,  1,  9,
	15,  7, 13,  5
);
//=============================================================================================================================

// vector axis/angle rotation, from https://suricrasia.online/blog/shader-functions/
vec3 erot( vec3 p, vec3 ax, float ro ) {
	return mix( dot( ax, p ) * ax, p, cos( ro ) ) + cross( ax, p ) * sin( ro );
}

// blue noise helper
vec4 blue() {
	return vec4( imageLoad( blueNoiseTexture, ivec2( blueNoiseOffset + ivec2( gl_GlobalInvocationID.xy ) ) % ivec2( imageSize( blueNoiseTexture ).xy ) ) ) / 255.0f;
}

// terrain trace helper
vec4 terrainTrace ( vec3 origin, vec3 direction ) {
	return traverse_cwbvh_terrain( origin, direction, tinybvh_safercp( direction ), 1e30f );
}

// grass trace helper
vec4 grassTrace ( vec3 origin, vec3 direction ) {
	return traverse_cwbvh_grass( origin, direction, tinybvh_safercp( direction ), 1e30f );
}

// sphere trace helper
vec4 sphereTrace ( vec3 origin, vec3 direction ) {
	vec3 normal;
	float d = iSphere( origin, direction, normal, 1.0f );
	// same as the grass/terrain, x is distance, yzw in this case is normal
	return vec4( d, normal );
}

// SDF geometry helpers
float deSrc ( vec3 p ) {
	vec3 pOriginal = p;
	float s = 2.0f, l = 0.0f;
	p = abs( p );
	for ( int j = 0; j++ < 8; )
		p =- sign( p ) * ( abs( abs( abs( p ) - 2.0f ) - 1.0f ) - 1.0f ),
		p *= l = -1.3f / dot( p, p ),
		p -= 0.15f, s *= l;
	return max( length( p ) / s, ( length( pOriginal ) - 1.0f ) );
}

float de ( vec3 p ) {
	// offset, scaling, masking
	float scale = 2.0f;
	p.z -= 0.3f;
	return deSrc( p * scale ) / scale;
}

const float epsilon = 0.0001f;
vec3 SDFNormal( in vec3 position ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );
}

vec4 SDFTrace ( vec3 origin, vec3 direction ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	vec3 pQuery = origin;
	for ( int steps = 0; steps < 300; steps++ ) {
		pQuery = origin + dTotal * direction;
		dQuery = de( pQuery );
		dTotal += dQuery * 0.9f; // small understep
		if ( dTotal > 5.0f || abs( dQuery ) < epsilon ) {
			break;
		}
	}
	// matching interface, x is distance, yzw is normal
	return vec4( dTotal, SDFNormal( origin + dTotal * direction ) );
}

//==Surface=ID=Values==========================================================================================================
#define NOHIT	0
#define SKIRTS	1
#define TERRAIN	2
#define GRASS	3
#define SDF		4
#define SPHERE	5
//=============================================================================================================================

void main () {
	// solve for jittered pixel uv, aspect ratio adjust
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// we're now ready to load the the GBuffer data

		// result 1
			// .x is 4-byte encoded normal
			// .y is 4-byte encoded post-refract ray direction
			// .z is the uint primitive ID
			// .w is signalling the surface ID ( really only using 3 bits right now, 0-5 )

		// result 2
			// .x is depth ( floatBitsToUint() encoded, need to unapply )
			// .yzw is worldspace position, half floats were not sufficient

		// result 3
			// .x is packed UV

	uvec4 Gbuffer1 = imageLoad( deferredResult1, writeLoc );
	uvec4 Gbuffer2 = imageLoad( deferredResult2, writeLoc );
	uvec4 Gbuffer3 = imageLoad( deferredResult3, writeLoc );

	vec3 color = vec3( 0.0f );

	// set base colors
	switch ( Gbuffer1.w ) {
	case NOHIT:
	// rays that have not even hit the snowglobe
		color = vec3( 0.0f );
		break;

	case SKIRTS:
	// need to figure out the particulars on skirts color
		color = vec3( 0.5f );
		break;

	case TERRAIN: {
	// needs to load terrain color
		uint index = 3 * Gbuffer1.z;
		color = vec3( triangleData[ index + 0 ].w, triangleData[ index + 1 ].w, triangleData[ index + 2 ].w ) * terrainBrightnessScalar;
		break;
	}

	case GRASS: {
	// needs to load grass color + V component of triangle UV, to fade to black at the base of the blade
		uint index = 4 * Gbuffer1.z;
		color = vec3( triangleData2[ index + 0 ].w, triangleData2[ index + 1 ].w, triangleData2[ index + 2 ].w ) * ( 1.0f - unpackHalf2x16( Gbuffer3.x ).y );
		break;
	}

	case SDF:
	// SDF coloration
		color = iron;
		break;

	case SPHERE:
	// this needs to basically be clear
		color = vec3( 0.0f, 0.3f, 0.8f );
		break;

	default:
		break;
	}

	// small amount of ambient light
	vec3 overallLightContribution = ambientLightLevel.a * ambientLightLevel.rgb;

	// these are surfaces that need to calculate lighting
	if ( Gbuffer1.w == TERRAIN || Gbuffer1.w == GRASS || Gbuffer1.w == SDF ) {

		// based on the x and y pixel locations, index into the list of light directions
		const int idx = bayerMatrix[ ( writeLoc.x % 4 ) + ( writeLoc.y % 4 ) * 4 ];

		// surface normal
		const vec3 normal = decode( Gbuffer1.x );

		// test shadow rays in the light direction
		vec3 rayOrigin = vec3( uintBitsToFloat( Gbuffer2.y ), uintBitsToFloat( Gbuffer2.z ), uintBitsToFloat( Gbuffer2.w ) );

		if ( lightEnable.x ) { // first light - "key light"
			vec4 terrainShadowHit = terrainTrace( rayOrigin, lightDirections0[ idx ] );				// terrain
			vec4 SDFShadowHit = SDFTrace( rayOrigin + epsilon * normal, lightDirections0[ idx ] );	// SDF
			vec4 grassShadowHit = grassTrace( rayOrigin, lightDirections0[ idx ] );					// grass
			vec4 sphereShadowHit = sphereTrace( rayOrigin, lightDirections0[ idx ] );				// sphere

			// resolve whether we hit an occluder before leaving the sphere
			bool inShadow = ( terrainShadowHit.x < sphereShadowHit.x ) || ( grassShadowHit.x < sphereShadowHit.x ) || ( SDFShadowHit.x < sphereShadowHit.x );

			// resolve color contribution ( N dot L diffuse term * shadow term )
			overallLightContribution += lightColor0.rgb * lightColor0.a * ( ( inShadow ) ? 0.005f : 1.0f ) * clamp( dot( normal, lightDirections0[ idx ] ), 0.01f, 1.0f );
		}

		if ( lightEnable.y ) { // same for second light - "fill light"
			vec4 terrainShadowHit = terrainTrace( rayOrigin, lightDirections1[ idx ] );				// terrain
			vec4 SDFShadowHit = SDFTrace( rayOrigin + epsilon * normal, lightDirections0[ idx ] );	// SDF
			vec4 grassShadowHit = grassTrace( rayOrigin, lightDirections1[ idx ] );					// grass
			vec4 sphereShadowHit = sphereTrace( rayOrigin, lightDirections1[ idx ] );				// sphere

			bool inShadow = ( terrainShadowHit.x < sphereShadowHit.x ) || ( grassShadowHit.x < sphereShadowHit.x ) || ( SDFShadowHit.x < sphereShadowHit.x );
			overallLightContribution += lightColor1.rgb * lightColor1.a * ( ( inShadow ) ? 0.005f : 1.0f ) * clamp( dot( normal, lightDirections1[ idx ] ), 0.01f, 1.0f );
		}

		if ( lightEnable.z ) { // same for third light - "back light"
			vec4 terrainShadowHit = terrainTrace( rayOrigin, lightDirections2[ idx ] );				// terrain
			vec4 SDFShadowHit = SDFTrace( rayOrigin + epsilon * normal, lightDirections0[ idx ] );	// SDF
			vec4 grassShadowHit = grassTrace( rayOrigin, lightDirections2[ idx ] );					// grass
			vec4 sphereShadowHit = sphereTrace( rayOrigin, lightDirections2[ idx ] );				// sphere

			bool inShadow = ( terrainShadowHit.x < sphereShadowHit.x ) || ( grassShadowHit.x < sphereShadowHit.x ) || ( SDFShadowHit.x < sphereShadowHit.x );
			overallLightContribution += lightColor2.rgb * lightColor2.a * ( ( inShadow ) ? 0.005f : 1.0f ) * clamp( dot( normal, lightDirections2[ idx ] ), 0.01f, 1.0f );
		}
	}

	// get the shaded color, based on the contribution of up to three lights
	color = overallLightContribution * color;

	if ( Gbuffer1.w != NOHIT && volumetricsEnable ) {
		// add the volumetric light inscatter contribution
			// direction for phase function could be average of the 16 jittered positions... tbd, starting with Voraldo style blending
		vec3 startingPosition = vec3( uintBitsToFloat( Gbuffer2.y ), uintBitsToFloat( Gbuffer2.z ), uintBitsToFloat( Gbuffer2.w ) );
		vec3 direction = -decode( Gbuffer1.y );

		// stepping from texel's worldspace position, in the negative post-refract ray direction, accumulating like Voraldo
		vec4 blueNoiseValues = blue();

		// compute step size
		const int numSteps = 400;
		const float maxDistance = uintBitsToFloat( Gbuffer2.x );
		const float stepSize = max( maxDistance / numSteps, 0.001f );

		float tCurrent = blue().x;

		// get some initial samples
		vec3 samplePosition = ( startingPosition + tCurrent * direction ) / 2.0f + vec3( 0.5f );
		vec3 shadowReads = vec3(
			texture( lightCache1, samplePosition ).r,
			texture( lightCache2, samplePosition ).r,
			texture( lightCache3, samplePosition ).r
		);
		vec3 colorSample = lightColor0.rgb * lightColor0.a * shadowReads.x + lightColor1.rgb * lightColor1.a * shadowReads.y + lightColor2.rgb * lightColor2.a * shadowReads.z;

		// assuming the incoming alpha is 1.0, can this be simplified?
		vec4 colorComposite = vec4( color, 1.0f );

		for ( int i = 0; i < numSteps; i++ ) {
			if ( tCurrent < maxDistance ) {
				colorComposite.a = max( volumeColor.a + colorComposite.a * ( 1.0f - volumeColor.a ), 0.001f );
				colorComposite.rgb = ( colorSample.rgb * volumeColor.rgb * volumeColor.a + colorComposite.rgb * colorComposite.a * ( 1.0f - volumeColor.a ) ) / colorComposite.a;
				tCurrent += stepSize;
				samplePosition = ( startingPosition + tCurrent * direction ) / 2.0f + vec3( 0.5f );
				shadowReads = vec3(
					texture( lightCache1, samplePosition ).r,
					texture( lightCache2, samplePosition ).r,
					texture( lightCache3, samplePosition ).r
				);
				colorSample = lightColor0.rgb * lightColor0.a * shadowReads.x + lightColor1.rgb * lightColor1.a * shadowReads.y + lightColor2.rgb * lightColor2.a * shadowReads.z;
			}
		}

		// need to see if there is room on this for some simplification
		color = colorComposite.rgb;
	}

	switch ( debugDrawMode ) {
	case 0: // normal functioning
		// load previous color and blend with the result, write back to accumulator
		vec4 previousColor = imageLoad( accumulatorTexture, writeLoc );
		imageStore( accumulatorTexture, writeLoc, mix( vec4( color, 1.0f ), previousColor, blendAmount ) );
	break;

	case 1: // Gbuffer normal vector visualization
		imageStore( accumulatorTexture, writeLoc, vec4( 0.5f * decode( Gbuffer1.x ) + 0.5f, 1.0f ) );
	break;

	case 2: // Gbuffer sphere-to-first-opaque distance
		imageStore( accumulatorTexture, writeLoc, vec4( vec3( uintBitsToFloat( Gbuffer2.x ) ), 1.0f ) );
	break;

	case 3: // worldspace position
		imageStore( accumulatorTexture, writeLoc, vec4( uintBitsToFloat( Gbuffer2.y ), uintBitsToFloat( Gbuffer2.z ), uintBitsToFloat( Gbuffer2.w ), 1.0f ) );
	break;

	case 4:
		switch ( Gbuffer1.w ) {
		case NOHIT:
			color = vec3( 0.0f );
			break;

		case SKIRTS:
			color = vec3( 0.1f );
			break;

		case TERRAIN:
			color = vec3( 0.1f, 0.03f, 0.0f );
			break;

		case GRASS:
			color = vec3( 0.2f, 1.0f, 0.0f );
			break;

		case SDF:
			color = vec3( 0.4f, 0.1f, 0.4f );
			break;

		case SPHERE:
			color = vec3( 0.0f, 0.3f, 0.8f );
			break;

		default:
			break;
		}
		imageStore( accumulatorTexture, writeLoc, vec4( color, 1.0f ) );
	break;

	default: // draw nothing
	break;
	}
}
