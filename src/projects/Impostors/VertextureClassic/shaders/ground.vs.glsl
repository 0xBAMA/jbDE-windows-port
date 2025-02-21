#version 430

uniform float AR;
uniform sampler2D heightmap;
uniform float time;
uniform float scale;
uniform mat3 trident;
uniform vec3 groundColor;
uniform float heightScale;

in vec3 vPosition;

out vec3 color;
out vec3 normal;
out vec3 worldPosition;

void main () {
	const float remap = 2.0f;

	vec4 tRead = texture( heightmap, vPosition.xy / remap );

	color = vec3( tRead.r );
	color.rgb *= groundColor;

	// calculating normals
	vec2 offsetPoints[ 2 ];
	offsetPoints[ 0 ] = vPosition.xy + vec2( 0.0f, 0.003f );
	offsetPoints[ 1 ] = vPosition.xy + vec2( 0.003f, 0.0f );

	vec4 offsetReads[ 2 ]; // consider: https://vec3.ca/bicubic-filtering-in-fewer-taps / https://www.shadertoy.com/view/MtVGWz - better filtering, to get rid of these stupid linear interpolation artifacts
	offsetReads[ 0 ] = texture( heightmap, offsetPoints[ 0 ] / remap );
	offsetReads[ 1 ] = texture( heightmap, offsetPoints[ 1 ] / remap );

	normal = trident * normalize( cross(
		vec3( vPosition.xy, tRead.r ) - vec3( offsetPoints[ 0 ].xy, offsetReads[ 0 ].r ),
		vec3( vPosition.xy, tRead.r ) - vec3( offsetPoints[ 1 ].xy, offsetReads[ 1 ].r )
	) );

	worldPosition = vPosition + vec3( 0.0f, 0.0f, ( tRead.r * heightScale ) - heightScale );
	vec3 position = scale * trident * worldPosition;

	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ), 1.0f );
}
