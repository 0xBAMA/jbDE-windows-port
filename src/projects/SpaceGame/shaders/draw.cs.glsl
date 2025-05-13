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

uniform float time;

uniform vec2 velocityVector;
uniform vec2 positionVector;

#include "consistentPrimitives.glsl.h";

#define PI 3.1415926535897932
#define TAU (PI * 2.0)
#define BASE_COLOR (vec3(1.0, 1.0, 1.0))

float random_at2(in vec2 p, float offset) {
	p = fract(p * vec2(abs(243.13 + offset * 2.0), abs(526.14 - offset)));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float random_at(vec2 uv, float offset)
{
    return fract(sin(uv.x * 113. + uv.y * 412. + offset) * 6339. - offset);
}

vec2 mod_space(in vec2 id, in float subdivisions) {
    return mod(id, subdivisions);
}

float frame(in vec2 where) {
    vec2 in_chunck_pos = fract(where) - 0.5;
	vec2 chunck_pos = floor(where);
    
    vec2 wa = smoothstep(0.48, 0.5, abs(in_chunck_pos));
    return wa.x + wa.y;
}

float draw_star(in vec2 where, in float size, in float flare) {
    float d = length(where);
    float m = (0.01 * size) / d;
	
    float ray_exp = 9000.0 / size;
    float rays = max(0.0, 1.0 - abs(where.x * where.y * ray_exp));
    m += rays * flare;
    
    m *= smoothstep(0.1 * size, 0.05 * size, d);
    
    return m;
}

vec3 stars_layer(in vec2 where, float subdivisions) {
	vec3 col = vec3(0.0);
    
	vec2 in_chunck_pos = fract(where) - 0.5;
	vec2 chunck_pos = floor(where);
    
    vec3 baseColor = BASE_COLOR;
	
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 offset = vec2(float(x), float(y));
			float n = random_at(mod_space(chunck_pos + offset, subdivisions), 39.392 * subdivisions);
            
            float size = fract(n * 3263.9);
            size *= size;
            float flare = smoothstep(0.8, 1.0, size);
            float d = draw_star(in_chunck_pos - offset - vec2(n, fract(n * 41.0)), size, flare);
            
            vec3 randomColor = vec3(fract(n * 931.45), fract(n * 2345.2), fract(n * 231.2));
			vec3 color = mix(BASE_COLOR, randomColor, 0.3);
            
            col += color * d;
		}
	}
	return col;
}

vec3 debris_layer(in vec2 where, in float subdivisions) {
	vec3 col = vec3(0.0);
	
	vec2 in_chunck_pos = fract(where) - 0.5;
	vec2 chunck_pos = floor(where);
	
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 offset = vec2(float(x), float(y));
            
			float n = random_at(mod_space(chunck_pos + offset, subdivisions), 23.93);
            
            float tt = (n + time * (2.0 * + n - 1.0)) * TAU * 2.0;
            vec2 time_offset = vec2(sin(tt), cos(tt)) * 0.0025 * (0.5 + fract(n * 1450.23));
            
            float d = smoothstep(0.013, 0.012, length(in_chunck_pos - offset - vec2(n, fract(n * 34.0)) + time_offset));
            
			vec3 randomColor = vec3(fract(n * 931.45), fract(n * 2345.2), fract(n * 231.2));
			vec3 color = mix(BASE_COLOR, randomColor, 0.5) * 0.3 * (sin(tt) * 0.2 + 1.1);
            
            col += color * d;
		}
	}
	return col;
}

vec3 smooth_noise(in vec2 where, in float subdivisions) {
    vec2 chunck_pos = floor(mod_space(where, subdivisions));
    vec2 in_chunck_pos = smoothstep(0.0, 1.0, fract(where));
    return vec3(
        mix(
            mix(
                random_at(mod_space(chunck_pos, subdivisions), 0.0),
                random_at(mod_space(chunck_pos + vec2(1.0, 0.0), subdivisions), 0.0),
                in_chunck_pos.x
            ),
            mix(
                random_at(mod_space(chunck_pos + vec2(0.0, 1.0), subdivisions), 0.0),
                random_at(mod_space(chunck_pos + vec2(1.0, 1.0), subdivisions), 0.0),
                in_chunck_pos.x
            ),
            in_chunck_pos.y
        )
    );
}

vec3 cloud_layer(in vec2 where, in float subdivisions) {
    vec3 col = vec3(0.0);
    // col += smooth_noise(where * 0.5, subdivisions);
    col += smooth_noise(where, subdivisions);
    col += smooth_noise(where * 2.0, subdivisions) * 0.5 * vec3(0.6, 0.6, 1.0);
    col += smooth_noise(where * 4.0, subdivisions) * 0.25 * vec3(1.0, 0.6, 0.6);
    col += smooth_noise(where * 8.0, subdivisions) * 0.125 * vec3(1.0, 0.8, 1.0);
    return col * col;
}

// from https://www.shadertoy.com/view/lXj3zc
vec3 nebulaBG( in vec2 fragCoord, vec2 offset ) {
    const float zoom = 6.18;
    const float subdivisions = 40.0;

    // fragCoord = floor(fragCoord / 4.0 + 0.5) * 4.0;
    vec2 where = zoom * ( ( fragCoord - imageSize( accumulatorTexture ).xy * 0.5) / imageSize( accumulatorTexture ).y - 0.5);

   // vec2 speed = -((iMouse.xy - imageSize( accumulatorTexture ).xy * 0.5) / imageSize( accumulatorTexture ).y) * 2.0;
    vec3 col = vec3(0.0);
    col += cloud_layer( where + offset / 16.0, subdivisions) * 0.05 - 0.03;
    col += stars_layer( where + offset / 16.0, subdivisions / 16.0 ) * 0.3;
    col += stars_layer( where + offset / 8.0, subdivisions / 8.0 ) * 0.6;
    col += stars_layer( where + offset / 4.0, subdivisions / 4.0 );
    col += debris_layer( where + offset, subdivisions );

    return col;
}

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	vec3 col = vec3( 0.0f );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
