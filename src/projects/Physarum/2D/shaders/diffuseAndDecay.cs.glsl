#version 430 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout( binding = 1, r32ui ) uniform uimage2D previous;
layout( binding = 2, r32ui ) uniform uimage2D current;

uniform float decayFactor;
uniform float time;

#include "noise.h"
#include "mathUtils.h"

void main() {
	// gaussian kernel
	ivec2 pos = ivec2( gl_GlobalInvocationID.xy );
	uint g = (
		1 * imageLoad( previous, pos + ivec2( -1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec2( -1,  1 ) ).r +
		1 * imageLoad( previous, pos + ivec2(  1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec2(  1,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec2(  0,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec2(  0, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec2(  1,  0 ) ).r +
		2 * imageLoad( previous, pos + ivec2( -1,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec2(  0,  0 ) ).r ) / 16;

	vec2 p = Rotate2D( 0.8f ) * ( vec2( pos ) / 800.0f );
	imageStore( current, pos, uvec4( uint( saturate( pow( saturate( perlinfbm( vec3( p.xy, time ), 2.0f, 5 ) + 0.33f ) + 0.01f, 2.0f ) ) * decayFactor * g ) ) );
}
