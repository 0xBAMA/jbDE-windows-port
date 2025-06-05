#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform ivec2 noiseOffset;
vec4 blueNoiseRef( ivec2 pos ) {
    pos.x = ( pos.x + noiseOffset.x ) % imageSize( blueNoiseTexture ).x;
    pos.y = ( pos.y + noiseOffset.y ) % imageSize( blueNoiseTexture ).y;
    return imageLoad( blueNoiseTexture, pos ) / 255.0f;
}

uniform float globalZoom;
uniform float sectorSize;
uniform float time;
uniform vec2 positionVector;

#include "consistentPrimitives.glsl.h";

#define PI 3.1415926535897932
#define TAU (PI * 2.0)
#define BASE_COLOR ( vec3( 1.0f ) )

float random_at ( vec2 uv, float offset ) { return fract( sin( uv.x * 113.0f + uv.y * 412.0f + offset ) * 6339.0f - offset ); }
vec2 mod_space ( in vec2 id, in float subdivisions ) { return mod( id, subdivisions ); }
float draw_star ( in vec2 where, in float size, in float flare ) {
    float d = length( where );
    float m = ( 0.01f * size ) / d;
    float ray_exp = 9000.0f / size;
    float rays = max( 0.0f, 1.0f - abs( where.x * where.y * ray_exp ) );
    m += rays * flare;
    m *= smoothstep( 0.1f * size, 0.05f * size, d );
    return m;
}

vec3 stars_layer ( in vec2 where, float subdivisions ) {
	vec3 col = vec3( 0.0f );
	vec2 in_chunck_pos = fract( where ) - 0.5f;
	vec2 chunck_pos = floor( where );
    vec3 baseColor = BASE_COLOR;
	for ( int y = -1; y <= 1; y++ ) {
		for ( int x = -1; x <= 1; x++ ) {
			vec2 offset = vec2( float( x ), float( y ) );
			float n = random_at( mod_space( chunck_pos + offset, subdivisions ), 39.392f * subdivisions );
            float size = fract( n * 3263.9f );
            size *= size;
            float flare = smoothstep( 0.8f, 1.0f, size );
            float d = draw_star( in_chunck_pos - offset - vec2( n, fract( n * 41.0f ) ), size, flare );
            vec3 randomColor = vec3( fract( n * 931.45f ), fract( n * 2345.2f ), fract( n * 231.2f ) );
			vec3 color = mix( BASE_COLOR, randomColor, 0.3f );
            col += color * d;
		}
	}
	return col;
}

vec3 debris_layer ( in vec2 where, in float subdivisions ) {
	vec3 col = vec3( 0.0f );
	vec2 in_chunck_pos = fract( where ) - 0.5f;
	vec2 chunck_pos = floor( where );
	for ( int y = -1; y <= 1; y++ ) {
		for ( int x = -1; x <= 1; x++ ) {
			vec2 offset = vec2( float( x ), float( y ) );
			float n = random_at( mod_space( chunck_pos + offset, subdivisions ), 23.93f );
            float tt = ( n + time * ( 2.0f * + n - 1.0f ) ) * TAU * 2.0f;
            vec2 time_offset = vec2( sin( tt ), cos( tt ) ) * 0.0025f * ( 0.5f + fract( n * 1450.23f ) );
            float d = smoothstep( 0.013f, 0.012f, length( in_chunck_pos - offset - vec2( n, fract( n * 34.0f ) ) + time_offset ) );
			vec3 randomColor = vec3( fract( n * 931.45f ), fract( n * 2345.2f ), fract( n * 231.2f ) );
			vec3 color = mix( BASE_COLOR, randomColor, 0.5f ) * 0.3f * ( sin( tt ) * 0.2f + 1.1f );
            col += color * d;
		}
	}
	return col;
}

vec3 smooth_noise ( in vec2 where, in float subdivisions ) {
    vec2 chunck_pos = floor( mod_space( where, subdivisions ) );
    vec2 in_chunck_pos = smoothstep( 0.0f, 1.0f, fract( where ) );
    return vec3(
        mix(
            mix(
                random_at( mod_space( chunck_pos, subdivisions ), 0.0f ),
                random_at( mod_space( chunck_pos + vec2( 1.0f, 0.0f ), subdivisions ), 0.0f ),
                in_chunck_pos.x
            ),
            mix(
                random_at( mod_space( chunck_pos + vec2( 0.0f, 1.0f ), subdivisions ), 0.0f ),
                random_at( mod_space( chunck_pos + vec2( 1.0f, 1.0f ), subdivisions ), 0.0f ),
                in_chunck_pos.x
            ),
            in_chunck_pos.y
        )
    );
}

vec3 cloud_layer ( in vec2 where, in float subdivisions ) {
    vec3 col = vec3( 0.0f );
    // col += smooth_noise(where * 0.5, subdivisions);
    col += smooth_noise( where, subdivisions );
    col += smooth_noise( where * 2.0f, subdivisions ) * 0.5f * vec3( 0.6f, 0.6f, 1.0f );
    col += smooth_noise( where * 4.0f, subdivisions ) * 0.25f * vec3( 1.0f, 0.6f, 0.6f );
    col += smooth_noise( where * 8.0f, subdivisions ) * 0.125f * vec3( 1.0f, 0.8f, 1.0f );
    // return col * col;
    return col;
}

// from https://www.shadertoy.com/view/lXj3zc
vec3 nebulaBG ( in vec2 fragCoord, vec2 offset ) {
    const float zoom = 6.18f;
    const float subdivisions = 40.0f;
    vec2 where = zoom * ( ( fragCoord - imageSize( accumulatorTexture ).xy * 0.5f ) / imageSize( accumulatorTexture ).y - 0.5f );
    vec3 col = vec3( 0.0f );
    col += cloud_layer( where + offset / 16.0f, subdivisions) * 0.05f - 0.03f;
    col += stars_layer( where + offset / 16.0f, subdivisions / 16.0f ) * 0.3f;
    col += stars_layer( where + offset / 8.0f, subdivisions / 8.0f ) * 0.6f;
    col += stars_layer( where + offset / 4.0f, subdivisions / 4.0f );
    col += debris_layer( where + offset, subdivisions );
    return col;
}

#include "srgbConvertMini.h"

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec2 iS = imageSize( accumulatorTexture ).xy;
	vec2 centeredUV = ( writeLoc + vec2( 0.5f ) ) / iS - vec2( 0.5f );
	centeredUV.x *= ( iS.x / iS.y );
	centeredUV.y *= -1.0f;
    // centeredUV *= 1.0f;

    // get a couple samples of the background
    vec4 offset1 = blueNoiseRef( writeLoc );
    vec4 offset2 = blueNoiseRef( writeLoc + ivec2( 256 ) );
    vec2 basePt = writeLoc + vec2( time );
    vec2 offset = positionVector;
	vec3 col = (
        nebulaBG( basePt + offset1.xy, offset ) +
        nebulaBG( basePt + offset1.zw, offset ) +
        nebulaBG( basePt + offset2.xy, offset ) +
        nebulaBG( basePt + offset2.zw, offset ) ) / 4.0f;

    // small reticle
    float dCenter = distance( vec2( 0.0f ), centeredUV );
    if ( dCenter < 0.01f ) {
        col = mix( col, vec3( 1.0f, 0.0f, 0.0f ), smoothstep( dCenter, 0.01f, 0.005f ) );
    }

    // fudged sector borders
    offset.y = -offset.y;
    vec2 pTest = 1.0f * ( globalZoom * centeredUV ) + 50.0f * offset;
    if ( ( pTest.x < ( -sectorSize / 2.0f ) ) || ( pTest.x > ( sectorSize / 2.0f ) )
        || ( pTest.y < ( -sectorSize / 2.0f ) ) || ( pTest.y > ( sectorSize / 2.0f ) ) ) {
        col.bg *= 0.25f;
    }

    col = srgb_to_rgb( col );

    // mix with history
    // col = mix( col, imageLoad( accumulatorTexture, writeLoc ).rgb, 0.5f );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
