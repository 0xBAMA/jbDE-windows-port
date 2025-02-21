#version 430

in vec3 position;
in vec3 color;
in vec3 normal;

layout( location = 0 ) out vec4 glFragColor;
layout( location = 1 ) out vec4 normalResult;
layout( location = 2 ) out vec4 positionResult;

void main () {
	glFragColor = vec4( color, 1.0f );
	glFragColor *= dot( vec3( 1.0f ), normal );
	glFragColor.a *= 0.2f;

	normalResult = vec4( normal, 1.0f );

	positionResult = vec4( position, 1.0f );

	// these are used to draw something along the lines of scanlines
	int fcxmod2 = int( gl_FragCoord.x ) % 2;
	int fcymod3 = int( gl_FragCoord.y ) % 3;
	if ( ( fcymod3 == 0 ) || ( fcxmod2 == 0 ) ) {
		discard;
	}
}