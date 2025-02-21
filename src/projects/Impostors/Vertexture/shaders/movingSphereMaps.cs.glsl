#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba32f ) uniform image2D steepnessTex;    // steepness texture for scaling movement speed
layout( binding = 2, rgba32f ) uniform image2D distanceDirTex; // distance + direction to nearest obstacle
uniform sampler2D heightmap;
uniform int dimension;
uniform int inSeed;
uniform float time;

// list of obstacles
uniform int numObstacles;
uniform vec3 obstacles[ 50 ];

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
	uvec2 loc = gl_GlobalInvocationID.xy;
	seed = inSeed + loc.x + loc.y * 10256;
	vec2 location = ( vec2( loc.xy ) / 512.0f ) - vec2( 0.5f );

	float minValue = 1000.0f;
	float maxValue = -1000.0f;
	for ( int i = 0; i < 20; i++ ) {
		float heightRead = texture( heightmap, location + randomInUnitDisk() * 0.03f ).r;
		minValue = min( heightRead, minValue );
		maxValue = max( heightRead, maxValue );
	}

	// update steepness map - value is the span from local minima to local maxima
	imageStore( steepnessTex, ivec2( loc ), vec4( maxValue - minValue, texture( heightmap, location ).r, 0.0f, 1.0f ) );

	float minD = 1000.0f;
	vec2 dir = vec2( 0.0f );
	for ( int i = 0; i < numObstacles; i++ ) {
		float d = distance( location * 2.0f, obstacles[ i ].xy );
		float radius = obstacles[ i ].z;
		minD = min( d - radius, minD );
		if ( minD == ( d - radius ) ) {
			// update direction
			dir = normalize( ( location * 2.0f ) - obstacles[ i ].xy );
		}
	}

	// update distance / direction map
	imageStore( distanceDirTex, ivec2( loc ), vec4( dir.xy, minD, 1.0f ) );
}