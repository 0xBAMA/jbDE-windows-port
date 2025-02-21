#version 430

uniform sampler2D colorMap;
uniform sampler2D normalMap;
uniform sampler2D heightMap;
uniform float scale;
uniform float heightScale;
uniform float time;
uniform float AR;
uniform mat3 trident;

in vec3 vPosition;

out vec3 position;
out vec3 color;
out vec3 normal;

void main () {
	const vec2 texCoord = ( vPosition.xy / 2.0f ) * 4.51f + vec2( 0.8f * time );
	vec4 cRead = texture( colorMap, texCoord );
	vec4 nRead = texture( normalMap, texCoord );
	vec4 hRead = texture( heightMap, texCoord );

	color = cRead.xyz / 2.0f;
	normal = nRead.xyz;

	position = vPosition;
	const vec3 vPosition_local = scale * trident * ( vPosition + vec3( 0.0f, 0.0f, ( hRead.r * 0.01f ) - ( heightScale / 2.0f ) ) );
	gl_Position = vec4( vPosition_local * vec3( 1.0f, AR, 1.0f ), 1.0f );
}
