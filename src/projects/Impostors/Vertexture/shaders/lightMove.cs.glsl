#version 430
layout( local_size_x = 16, local_size_y = 1, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;

// moving lights state
uniform int lightCount;
struct light_t {
	vec4 position;
	vec4 color;
};
layout( binding = 4, std430 ) buffer movingLightState {
	light_t lightData[];
};

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
	const uint index = gl_GlobalInvocationID.x;
	const ivec2 loc = ivec2( ( ( lightData[ index ].position.xy + 1.0f ) / 2.0f ) * vec2( 512.0f ) );
	seed = index + uint( inSeed ); // initialize the rng state to use the std::random uniformly generated value passed in

	lightData[ index ].position.xyz = lightData[ index ].position.xyz + randomUnitVector() * 0.01f + vec3( 0.0f, 0.0f, 0.001f );

	// wrap
	if ( lightData[ index ].position.x >  worldY / 2.0f ) lightData[ index ].position.x -= worldY;
	if ( lightData[ index ].position.x < -worldY / 2.0f ) lightData[ index ].position.x += worldY;
	if ( lightData[ index ].position.y >  worldX / 2.0f ) lightData[ index ].position.y -= worldX;
	if ( lightData[ index ].position.y < -worldX / 2.0f ) lightData[ index ].position.y += worldX;
	if ( lightData[ index ].position.z >  worldY / 2.0f ) lightData[ index ].position.z -= worldY;
	if ( lightData[ index ].position.z < -worldY / 2.0f ) lightData[ index ].position.z += worldY;
}