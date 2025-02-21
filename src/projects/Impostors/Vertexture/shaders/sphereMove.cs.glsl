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
uniform float worldX;
uniform float worldY;

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
	seed = index + uint( inSeed );

	data[ index ].position.xy = data[ index ].position.xy + randomInUnitDisk() * 0.01f;
	data[ index ].position.z = data[ index ].position.z + normalizedRandomFloat() * 0.01f;

	// wrap
	if ( data[ index ].position.x >  worldY / 2.0f ) data[ index ].position.x -= worldY;
	if ( data[ index ].position.x < -worldY / 2.0f ) data[ index ].position.x += worldY;
	if ( data[ index ].position.y >  worldX / 2.0f ) data[ index ].position.y -= worldX;
	if ( data[ index ].position.y < -worldX / 2.0f ) data[ index ].position.y += worldX;
	if ( data[ index ].position.z >  worldY / 2.0f ) data[ index ].position.z -= worldY;
	if ( data[ index ].position.z < -worldY / 2.0f ) data[ index ].position.z += worldY;
}