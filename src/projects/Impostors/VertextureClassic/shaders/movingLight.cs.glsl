#version 430
layout( local_size_x = 16, local_size_y = 1, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba32f ) uniform image2D steepnessTex;    // steepness texture for scaling movement speed
layout( binding = 2, rgba32f ) uniform image2D distanceDirTex; // distance + direction to nearest obstacle

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
	// const vec4 steepnessRead = imageLoad( steepnessTex, loc );
	// const vec4 distDirRead = imageLoad( distanceDirTex, loc );

	seed = index + uint( inSeed ); // initialize the rng state to use the std::random uniformly generated value passed in

	// if ( distDirRead.z <= 0.0f ) {
		// lightData[ index ].position.xy += distDirRead.xy * 0.02f;
	// }

	lightData[ index ].position.xy = lightData[ index ].position.xy + lightData[ 0 ].color.xy * 0.01f;
	// lightData[ index ].position.z = 0.2f * sin( time + lightData[ index ].color.a ) + 0.21f;

	// wrap
	if ( lightData[ index ].position.x > 1.0f ) lightData[ index ].position.x -= 2.0f;
	if ( lightData[ index ].position.x < -1.0f ) lightData[ index ].position.x += 2.0f;
	if ( lightData[ index ].position.y > 1.0f ) lightData[ index ].position.y -= 2.0f;
	if ( lightData[ index ].position.y < -1.0f ) lightData[ index ].position.y += 2.0f;
}