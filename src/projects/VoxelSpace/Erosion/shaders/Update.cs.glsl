#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( r32f ) uniform image2D sourceData;
layout( rgba8ui ) uniform uimage2D map;

const float sqrt2 = sqrt( 2.0f );
const float scale = 60.0f;

vec3 GetNormal ( ivec2 location ) {
	const float cache00 = imageLoad( sourceData, location + ivec2( 0, 0 ) ).r;
	const float cachep0 = imageLoad( sourceData, location + ivec2(  1,  0 ) ).r;
	const float cachen0 = imageLoad( sourceData, location + ivec2( -1,  0 ) ).r;
	const float cache0p = imageLoad( sourceData, location + ivec2(  0,  1 ) ).r;
	const float cache0n = imageLoad( sourceData, location + ivec2(  0, -1 ) ).r;
	const float cachepp = imageLoad( sourceData, location + ivec2(  1,  1 ) ).r;
	const float cachepn = imageLoad( sourceData, location + ivec2(  1, -1 ) ).r;
	const float cachenp = imageLoad( sourceData, location + ivec2( -1,  1 ) ).r;
	const float cachenn = imageLoad( sourceData, location + ivec2( -1, -1 ) ).r;

	vec3 n = vec3( 0.15f ) * normalize( vec3( scale * ( cache00 - cachep0 ), 1.0f, 0.0f ) );  // Positive X
	n += vec3( 0.15f ) * normalize( vec3( scale * ( cachen0 - cache00 ), 1.0f, 0.0f ) );     // Negative X
	n += vec3( 0.15f ) * normalize( vec3( 0.0f, 1.0f, scale * ( cache00 - cache0p ) ) );     // Positive Y
	n += vec3( 0.15f ) * normalize( vec3( 0.0f, 1.0f, scale * ( cache0n - cache00 ) ) );     // Negative Y
	// diagonals
	n += vec3( 0.1f ) * normalize( vec3( scale * ( cache00 - cachepp ) / sqrt2, sqrt2, scale * ( cache00 - cachepp ) / sqrt2 ) );
	n += vec3( 0.1f ) * normalize( vec3( scale * ( cache00 - cachepn ) / sqrt2, sqrt2, scale * ( cache00 - cachepn ) / sqrt2 ) );
	n += vec3( 0.1f ) * normalize( vec3( scale * ( cache00 - cachenp ) / sqrt2, sqrt2, scale * ( cache00 - cachenp ) / sqrt2 ) );
	n += vec3( 0.1f ) * normalize( vec3( scale * ( cache00 - cachenn ) / sqrt2, sqrt2, scale * ( cache00 - cachenn ) / sqrt2 ) );

	n.y *= 0.001f;
	return normalize( n );
}

void main () {
	ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	float src = 255.0f * imageLoad( sourceData, location ).r;
	vec3 normal = GetNormal( location );

	vec3 shadingResult = vec3( dot( vec3( 0.0f, 1.0f, 0.0f ), normal ) ) * ( 1.0f / 255.0f ) * vec3( 1.618f * ( 255.0f - src * 255.0f ), 1.618f * float( src * 255.0f ), 1.0f / ( src * 255.0f ) );

	imageStore( map, location, uvec4( uvec3( shadingResult.xyz ), src ) );
}
