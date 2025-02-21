#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform sampler2D color;
uniform sampler2D normal;
uniform sampler2D position;
uniform sampler2D depth;

uniform vec2 resolution;
// potentially add offset, to 

// points SSBO
struct point_t {
	vec4 position;
	vec4 color;
};

layout( binding = 3, std430 ) buffer pointData {
	point_t data[];
};

void main () {
	const ivec2 loc				= ivec2( gl_GlobalInvocationID.xy );
	const vec2 sampleLocation	= ( vec2( loc ) + vec2( 0.5f ) ) / resolution;

	// only accept writes within bounds
	if ( loc.x < int( resolution.x ) && loc.y < int( resolution.y ) ) {
		const vec3 colorSample		= texture( color, sampleLocation ).xyz;
		const vec3 normalSample		= texture( normal, sampleLocation ).xyz;
		const vec3 positionSample	= texture( position, sampleLocation ).xyz;
		const vec3 depthSample		= texture( depth, sampleLocation ).xyz;

		point_t newPoint;
		newPoint.position			= vec4( positionSample, 10.0f );
		newPoint.color				= vec4( colorSample, 0.0f );
		// newPoint.color				= vec4( clamp( normalSample / 2.0f + vec3( 1.0f ), vec3( 0.0f ), vec3( 1.0f ) ), 0.0f );

		const int bufferOffset		= loc.x + loc.y * int( resolution.x );
		data[ bufferOffset ]		= newPoint;
	}
}