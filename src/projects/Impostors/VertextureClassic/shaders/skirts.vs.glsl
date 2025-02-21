#version 430

uniform float AR;
uniform sampler2D heightmap;
uniform float time;
uniform float scale;
uniform mat3 trident;
uniform float heightScale;

in vec3 vPosition;

out vec2 texCoord;
out vec3 position;

void main () {
	texCoord = vPosition.xy / 2.0f;
	vec4 tRead = texture( heightmap, texCoord );
	position = vPosition + vec3( 0.0f, 0.0f, ( tRead.r * heightScale ) - heightScale );
	vec3 vPosition_local = scale * trident * position;
	gl_Position = vec4( vPosition_local * vec3( 1.0f, AR, 1.0f ), 1.0f );
}
