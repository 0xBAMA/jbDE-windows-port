#version 430
#include "mathUtils.h"
#include "cubeVerts.h"

out flat uint vofiIndex;
out vec3 vofiPosition;

uniform mat4 viewTransform;
layout( binding = 0, std430 ) buffer transformsBuffer {
	mat4 transforms[];
};

void main() {
	vofiIndex = gl_VertexID / 36;
	vec4 position = transforms[ vofiIndex ] * vec4( CubeVert( gl_VertexID % 36 ), 1.0f );
	vofiPosition = position.xyz;
	gl_Position = viewTransform * position;
}