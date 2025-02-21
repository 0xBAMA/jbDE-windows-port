#version 430

uniform float AR;
uniform sampler2D heightmap;
uniform float scale;
uniform float time;
uniform mat3 trident;
uniform float heightScale;

in vec4 vColor;
in vec4 vPosition;

out float radius;
flat out float roughness;
out vec3 color;
out vec3 worldPosition;
out float height;
flat out int index;

void main () {
	index = gl_VertexID;
	vec4 tRead = texture( heightmap, vPosition.xy / 2.0f );
	height = ( tRead.r * heightScale ) - heightScale;

	radius = gl_PointSize = scale * vPosition.a * AR;
	color = vColor.xyz;
	roughness = vColor.a;

	// temporary for debug spheres
	if ( vPosition.a == 50.0f ) {
		height = 0.0f;
	}

	worldPosition = vPosition.xyz + vec3( 0.0f, 0.0f, height );

	vec3 position = scale * trident * worldPosition;
	position.z += radius / 1024.0f; // precompensate for depth
	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ), 1.0f );
}
