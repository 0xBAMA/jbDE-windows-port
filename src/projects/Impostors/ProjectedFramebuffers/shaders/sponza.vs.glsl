#version 430

// querying GL_MAX_VERTEX_ATTRIBS reports 16
layout( location = 0 ) in vec3 vPosition;
layout( location = 1 ) in vec3 vTexCoord;
layout( location = 2 ) in vec3 vNormal;
layout( location = 3 ) in vec3 vTangent;
layout( location = 4 ) in vec3 vBitangent;

out vec3 position;
out vec3 texCoord;
out vec3 normal;
out vec3 tangent;
out vec3 bitangent;

uniform mat4 transform;

void main () {
	position	= vPosition;
	texCoord	= vTexCoord;
	normal		= normalize( ( transform * vec4( vNormal, 0.0f ) ).xyz );
	tangent		= normalize( ( transform * vec4( vTangent, 0.0f ) ).xyz );
	bitangent	= normalize( ( transform * vec4( vBitangent, 0.0f ) ).xyz );

	// translate into clip space
	gl_Position	= transform * vec4( vPosition, 1.0f );
}
