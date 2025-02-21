#version 430

uniform float AR;
uniform float time;
uniform mat3 trident;
uniform float scale;
uniform float frameWidth;
uniform mat4 perspectiveMatrix;
uniform mat4 lookatMatrix;

// points SSBO
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
flat out uint index;

void main () {
	index = gl_VertexID;

	vec4 positionRead = data[ index ].position;
	vec4 colorRead = data[ index ].color;

	color = colorRead.rgb;
	worldPosition = positionRead.xyz;

	vec3 position = scale * trident * ( worldPosition );
	radius = gl_PointSize = clamp( ( scale * positionRead.a * AR ) / max( position.z + 1.0f, 0.01f ), 0.0f, 100.0f );
	position.z += radius / frameWidth; // precompensate for depth offset in fragment shader
	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ), max( position.z + 1.0f, 0.01f ) );
}
