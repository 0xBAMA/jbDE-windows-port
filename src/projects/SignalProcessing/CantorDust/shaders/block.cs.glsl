#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// the 2D and 3D histogram images
layout( binding = 2, r32ui ) uniform uimage2D dataTexture2D;
layout( binding = 3, r32ui ) uniform uimage3D dataTexture3D;

// target for the DDA draw
layout( binding = 4, r16f ) uniform image2D blockImage;

// ssbo for data bytes
layout( binding = 0, std430 ) buffer dataBuffer { uint data[]; };

// ssbo for 2d histogram max
layout( binding = 1, std430 ) buffer histogram2DMax { uint max2D; };

// ssbo for 3d histogram max
layout( binding = 2, std430 ) buffer histogram3DMax { uint max3D; };

// need to unpack uint8s from full fat uint32s
uint getByte ( uint index ) {
	// since we pack 4 bytes into each uint...
	uint base = index / 4;

	// get the initial uint value (4 bytes)
	uint value = data[ base ];

	// looking at the remainder, to determine how to mask
	switch ( index - ( base * 4 ) ) {
		case 0:
		value = 0xFF000000u & value;
		value = value >> 24;
		break;

		case 1:
		value = 0x00FF0000u & value;
		value = value >> 16;
		break;

		case 2:
		value = 0x0000FF00u & value;
		value = value >> 8;
		break;

		case 3:
		value = 0x000000FFu & value;
		break;
	}
	return value;
}

uniform float time;

#include "consistentPrimitives.glsl.h"
#include "mathUtils.h"
#include "colorRamps.glsl.h"

bool inBounds ( ivec3 pos ) { // helper function for bounds checking
	return ( all( greaterThanEqual( pos, ivec3( 0 ) ) ) && all( lessThan( pos, imageSize( dataTexture3D ) ) ) );
}

void main () {
	// byte index
	ivec2 index = ivec2( gl_GlobalInvocationID.xy );

// DDA traversal
	float aspectRatio = float( imageSize( blockImage ).x ) / float( imageSize( blockImage ).y );
	vec2 uv = vec2(
		RangeRemapValue( float( index.x ), 0, imageSize( blockImage ).x, -aspectRatio, aspectRatio ) - 0.3f,
		RangeRemapValue( float( index.y ), 0, imageSize( blockImage ).y, 0.85f, -1.1f ) );

	// Camera
	vec3 ro = vec3( 3.0f * sin( time ), 2.0f, -3.0f * cos( time ) );
	vec3 lookat = vec3( 0.0f );
	vec3 f = normalize( lookat - ro );
	vec3 r = normalize( cross( vec3( 0.0f, 1.0f, 0.0f ), f ) );
	vec3 u = normalize( cross( f, r ) );
	float zoom = 2.0f;
	vec3 c = ro + f * zoom;
	vec3 i = c + uv.x * r + uv.y * u;
	vec3 rd = i - ro;

	// intersect
	float sum = 0.0f;
	vec3 normal = vec3( 0.0f );
	float boxDist = iBoxOffset( ro, rd, normal, vec3( 1.0f ), vec3( 0.0f ) );
	vec3 color = vec3( 0.0f );

	if ( boxDist < MAX_DIST_CP ) {
	// DDA traversal for the sum
		vec3 hitPos = ro + rd * ( boxDist + 0.001f );
		hitPos.x = RangeRemapValue( hitPos.x, -1.0f, 1.0f, 0.0f, imageSize( dataTexture3D ).x );
		hitPos.y = RangeRemapValue( hitPos.y, -1.0f, 1.0f, 0.0f, imageSize( dataTexture3D ).y );
		hitPos.z = RangeRemapValue( hitPos.z, -1.0f, 1.0f, 0.0f, imageSize( dataTexture3D ).z );

		// from https://www.shadertoy.com/view/7sdSzH
		vec3 deltaDist = 1.0f / abs( rd );
		ivec3 rayStep = ivec3( sign( rd ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( hitPos ) );
		vec3 sideDist0 = ( sign( rd ) * ( vec3( mapPos0 ) - hitPos ) + ( sign( rd ) * 0.5f ) + 0.5f ) * deltaDist;

		for ( int i = 0; inBounds( mapPos0 ); i++ ) {

			bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
			vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
			ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

			// add to the sum
			// sum += float( imageLoad( dataTexture3D, mapPos0 ).r ) / float( max3D );
			sum += float( imageLoad( dataTexture3D, mapPos0 ).r );

			sideDist0 = sideDist1;
			mapPos0 = mapPos1;
		}

		// getting a color from the sum
		sum = sqrt( sum / float( max3D ) );
	}

	// store to the buffer, which gets sampled during output
	imageStore( blockImage, index, vec4( sum, 0.0f, 0.0f, 1.0f ) );
}
