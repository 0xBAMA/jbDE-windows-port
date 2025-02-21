#version 430

uniform sampler2D heightmap;
uniform sampler2D waterHeight;
uniform float time;
uniform float heightScale;
uniform vec3 groundColor;

in vec2 texCoord;
in vec3 position;

layout( location = 0 ) out vec4 glFragColor;
layout( location = 1 ) out vec4 normalResult;
layout( location = 2 ) out vec4 positionResult; // do we need this? tbd

void main() {
	vec4 tRead = texture( heightmap, texCoord );
	vec4 tReadWater = texture( waterHeight, texCoord * 4.51f + vec2( 0.8f * time ) );

	const bool belowGround = position.z < ( ( tRead.r * heightScale ) - heightScale );
	const bool belowWater = position.z < ( ( tReadWater.r * 0.03f ) - ( heightScale / 2.0f ) );

	// if ( gl_FrontFacing ) // backdrop behavior - maybe interesting to play with later

	if ( belowGround ) { // ground gradient

		float grad = clamp( 0.1f, 1.0f, position.z / 0.4f + 0.2f / 0.4f );
		glFragColor = vec4( grad, grad, grad, 1.0f );
		glFragColor.rgb *= groundColor;
		// glFragColor.a = grad * 10.0f;
		positionResult = vec4( position, 1.0f );
		normalResult = vec4( 1.0f, 1.0f, 1.0f, 1.0f );

	} else if ( belowWater ) { // water color

		// do we want to include some effect of the roving lights here?
		glFragColor = vec4( 0.1f, 0.2f, 0.4f, 0.3f );
		positionResult = vec4( position, 1.0f );
		normalResult = vec4( 1.0f, 1.0f, 1.0f, 1.0f );

	} else {

		discard;

	}

	// //fcymod3 is used to draw something along the lines of scanlines
	// int fcxmod2 = int( gl_FragCoord.x ) % 2;
	// int fcymod2 = int( gl_FragCoord.y ) % 2;
	// if ( ( fcymod2 == 1 ) || ( fcxmod2 == 1 ) ) {
	// 	glFragColor.rgb /= 1.618f;
	// }

}