#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba32f ) uniform image2D steepnessTex;    // steepness texture for scaling movement speed
layout( binding = 2, rgba32f ) uniform image2D distanceDirTex; // distance + direction to nearest obstacle

// moving points state
struct point_t {
	vec4 position;
	vec4 color;
};
layout( binding = 3, std430 ) buffer pointData {
	point_t data[];
};

uniform int dimension;
uniform int inSeed;
uniform float time;

// random utilites
uint seed = 0;
uint wangHash () {
	seed = uint( seed ^ uint( 61 ) ) ^ uint( seed >> uint( 16 ) );
	seed *= uint( 9 );
	seed = seed ^ ( seed >> 4 );
	seed *= uint( 0x27d4eb2d );
	seed = seed ^ ( seed >> 15 );
	return seed;
}

float normalizedRandomFloat () {
	return float( wangHash() ) / 4294967296.0f;
}

const float pi = 3.14159265358979323846f;
vec3 randomUnitVector () {
	float z = normalizedRandomFloat() * 2.0f - 1.0f;
	float a = normalizedRandomFloat() * 2.0f * pi;
	float r = sqrt( 1.0f - z * z );
	float x = r * cos( a );
	float y = r * sin( a );
	return vec3( x, y, z );
}

vec2 randomInUnitDisk () {
	return randomUnitVector().xy;
}

void main () {
	const uint index = gl_GlobalInvocationID.x + dimension * gl_GlobalInvocationID.y;
	const ivec2 loc = ivec2( ( ( data[ index ].position.xy + 1.0f ) / 2.0f ) * vec2( 512.0f ) );
	const vec4 steepnessRead = imageLoad( steepnessTex, loc );
	const vec4 distDirRead = imageLoad( distanceDirTex, loc );

	seed = index + uint( inSeed );

	if ( distDirRead.z <= 0.0f ) {
		data[ index ].position.xy += distDirRead.xy * 0.02f;
	}

	// do something to measure how much we have moved since last frame ...
		// if this value stays low for multiple updates, it is probably stuck on the concave area between
		// two obstacles, and we want to reset the position to a random location on the simulattion plane

	data[ index ].position.xy = data[ index ].position.xy + randomInUnitDisk() * 0.002f + vec2( 0.0001f ) * ( 1.0f / steepnessRead.r );
	data[ index ].position.z = 0.04f * sin( time * 10.0f + data[ index ].color.a ) + 0.05f;

	// wrap
	if ( data[ index ].position.x > 1.0f ) data[ index ].position.x -= 2.0f;
	if ( data[ index ].position.x < -1.0f ) data[ index ].position.x += 2.0f;
	if ( data[ index ].position.y > 1.0f ) data[ index ].position.y -= 2.0f;
	if ( data[ index ].position.y < -1.0f ) data[ index ].position.y += 2.0f;
}