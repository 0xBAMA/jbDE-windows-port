#version 430

uniform float AR;
uniform sampler2D heightmap;
uniform float time;
uniform mat3 trident;
uniform float scale;
uniform float heightScale;

layout( binding = 1, rgba32f ) uniform image2D steepnessTex;    // steepness texture for scaling movement speed
layout( binding = 2, rgba32f ) uniform image2D distanceDirTex; // distance + direction to nearest obstacle

// moving point state
struct point_t {
	vec4 position;
	vec4 color;
};
layout( binding = 3, std430 ) buffer pointData {
	point_t data[];
};

out float radius;
out float roughness;
out vec3 color;
out vec3 worldPosition;
out float height;
out mat3 rot;
flat out int index;

void main () {
	index = gl_VertexID;
	vec4 positionRead = data[ index ].position;
	vec4 colorRead = data[ index ].color;

	ivec2 loc = ivec2( ( ( positionRead.xy + 1.0f ) / 2.0f ) * vec2( 512.0f ) );
	vec4 steepnessRead = imageLoad( steepnessTex, loc );

	vec4 tRead = texture( heightmap, positionRead.xy / 2.0f );
	height = ( tRead.r * heightScale ) - heightScale;
	radius = gl_PointSize = scale * positionRead.a * AR;

	color = colorRead.rgb;
	roughness = colorRead.a;

	rot = trident;

	worldPosition = positionRead.xyz + vec3( 0.0f, 0.0f, height );

	vec3 position = scale * trident * ( worldPosition );
	position.z += radius / 1024.0f; // precompensate for depth offset in fragment shader
	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ), 1.0f );
}
