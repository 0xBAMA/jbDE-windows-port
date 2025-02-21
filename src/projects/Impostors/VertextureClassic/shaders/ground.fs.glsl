#version 430

uniform float scale;
uniform mat3 trident;

// moving lights state
uniform int lightCount;
struct light_t {
	vec4 position;
	vec4 color;
};
layout( binding = 4, std430 ) buffer lightDataBuffer {
	light_t lightData[];
};

in vec3 color;
in vec3 normal;
in vec3 worldPosition;

layout( location = 0 ) out vec4 glFragColor;
layout( location = 1 ) out vec4 normalResult;
layout( location = 2 ) out vec4 positionResult;

const float roughness = 0.1f;

void main () {
	glFragColor = vec4( color, 1.0f ); // alpha channel should be available
	// normalResult = vec4( normal, roughness );
	normalResult = vec4( normal, 1.0f );
	positionResult = vec4( worldPosition, 1.0f ); // alpha channel available
}