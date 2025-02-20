#version 430 core
uniform sampler2D current;
// layout( binding = 0 ) uniform sampler2D current;
uniform vec2 resolution;
out vec4 fragmentOutput;
void main () {
	vec2 sampleLocation = ( gl_FragCoord.xy + 0.5f ) / resolution;
	sampleLocation.y = 1.0f - sampleLocation.y; // rectify flip
	fragmentOutput = texture( current, sampleLocation ); // half pixel offset
}
