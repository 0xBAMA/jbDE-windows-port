#version 430
#include "mathUtils.h"

out flat uint vofiIndex;
out vec3 vofiPosition;

flat out vec2 center;
flat out float radiusPixels;

uniform vec2 viewportSize;
uniform mat4 viewTransform;
uniform mat4 projTransform;

struct parameters_t {
	float data[ 16 ];
};
layout( binding = 2, std430 ) buffer parametersBuffer {
	parameters_t parameters[];
};

void main() {
	vofiIndex = gl_VertexID;
	vec4 position = vec4( parameters[ vofiIndex ].data[ 1 ], parameters[ vofiIndex ].data[ 2 ], parameters[ vofiIndex ].data[ 3 ], 1.0f );
	vofiPosition = position.xyz;
	gl_Position = viewTransform * position;

	const float radius = parameters[ vofiIndex ].data[ 4 ];

	// https://stackoverflow.com/questions/25780145/gl-pointsize-corresponding-to-world-space-size
	center = ( 0.5f * gl_Position.xy / gl_Position.w + 0.5f ) * viewportSize;
	gl_PointSize = viewportSize.y * projTransform[ 1 ][ 1 ] * radius / gl_Position.w;
	radiusPixels = gl_PointSize / 2.0f;

}