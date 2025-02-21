#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform bool userClicked;
uniform ivec2 clickLocation;
uniform int clickMode;
uniform ivec2 sizeOfScreen;
uniform float brushRadius;

uniform vec3 viewerPosition;
uniform float zoom;
uniform float verticalOffset;

uniform int sliceOffset;
layout( r8ui ) uniform uimage3D CAStateBuffer;

ivec3 getOffsetPos ( ivec3 pos ) {
	pos.z += sliceOffset;
	pos.z = pos.z % ( imageSize( CAStateBuffer ).z );
	return pos;
}

uint getValue ( ivec3 pos ) {
	return imageLoad( CAStateBuffer, getOffsetPos( pos ) ).r;
}

bool getState ( ivec3 location ) {
	// compute the bitmask
	uint bitmask = 1u;

	// read state from back buffer
	uint previousState = ( getValue( location ) != 0 ) ? 1 : 0;

	uint count = 0;
	count += ( getValue( location + ivec3( -1, -1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3(  0, -1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3(  1, -1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3( -1,  0,  0 ) ) != 0 ) ? 1 : 0;
	// skip center pixel, already exists in previousState
	count += ( getValue( location + ivec3(  1,  0,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3( -1,  1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3(  0,  1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3(  1,  1,  0 ) ) != 0 ) ? 1 : 0;
	
	// determine new state - Conway's Game of Life rules
	return ( ( count == 2 || count == 3 ) && previousState == 1 ) || ( count == 3 && previousState == 0 );
}

const float maxDist = 1e10f;
const vec2 distBound = vec2( 0.0001f, maxDist );
float box( in vec3 ro, in vec3 rd, in vec3 boxSize, in vec3 boxPosition ) {
	ro -= boxPosition;

	vec3 m = sign( rd ) / max( abs( rd ), 1e-8 );
	vec3 n = m * ro;
	vec3 k = abs( m ) * boxSize;

	vec3 t1 = -n - k;
	vec3 t2 = -n + k;

	float tN = max( max( t1.x, t1.y ), t1.z );
	float tF = min( min( t2.x, t2.y ), t2.z );

	if ( tN > tF || tF <= 0.0f ) {
		return maxDist;
	} else {
		if ( tN >= distBound.x && tN <= distBound.y ) {
			return tN;
		} else if ( tF >= distBound.x && tF <= distBound.y ) {
			return tF;
		} else {
			return maxDist;
		}
	}
}

#include "mathUtils.h"

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uint state = ( getState( ivec3( writeLoc, -1 ) ) ? 255 : 0 );

	if ( userClicked ) {
		const vec3 CASimDims = imageSize( CAStateBuffer ).xyz;
		const vec3 CABlockDims = vec3( 4.0f );

		// generate eye ray
		const vec2 iS = sizeOfScreen;
		const vec2 uv = vec2( clickLocation + 0.5f ) / iS;
		const vec2 p = zoom * ( uv * 2.0f - 1.0f ) * vec2( iS.x / iS.y, 1.0f );

		// lookat
		const vec3 forwards = normalize( -viewerPosition );
		const vec3 left = normalize( cross( forwards, vec3( 0.0f, 0.0f, 1.0f ) ) );
		const vec3 up = normalize( cross( forwards, left ) );

		const vec3 location = viewerPosition + p.x * left + ( p.y - verticalOffset ) * up;

		// ray-box test
		const float d = box( location, forwards, CABlockDims, vec3( 0.0f ) );
		const vec3 hitPosition = location + forwards * d * 1.0001f;

		const vec2 hitInUVSpace = vec2(
			RangeRemapValue( hitPosition.x, -CABlockDims.x, CABlockDims.x, 0.0f, CASimDims.x ),
			RangeRemapValue( hitPosition.y, -CABlockDims.y, CABlockDims.y, 0.0f, CASimDims.y )
		);

		#define SOLID	0
		#define CLEAR	1
		#define NOISE	2
		#define LINEX	3
		#define LINEY	4
		#define CROSS	5

		if ( distance( hitInUVSpace, vec2( writeLoc ) ) < brushRadius ) {
			switch ( clickMode ) {
				case SOLID:
				state = 255;
				break;

				case CLEAR:
				state = 0;
				break;

				case NOISE:
				if ( ( pcg3d( uvec3( writeLoc.xy, sliceOffset ) ) / 4294967296.0f ).x < 0.5f ) {
					state = 255;
				} else {
					state = 0;
				}
				break;

				case LINEX:
				if ( int( hitInUVSpace.x ) == writeLoc.x ) {
					state = 255;
				}
				break;

				case LINEY:
				if ( int( hitInUVSpace.y ) == writeLoc.y ) {
					state = 255;
				}
				break;

				case CROSS:
				if ( int( hitInUVSpace.x ) == writeLoc.x || int( hitInUVSpace.y ) == writeLoc.y ) {
					state = 255;
				}
				break;
			}
		}
	}

	// write the data to the front buffer
	imageStore( CAStateBuffer, getOffsetPos( ivec3( writeLoc, 0 ) ), uvec4( state ) );
}