#version 430 core
uniform sampler2D current;
// layout( binding = 0 ) uniform sampler2D current;
uniform vec2 resolution;
out vec4 fragmentOutput;
void main () {
	fragmentOutput = texture( current, ( gl_FragCoord.xy + 0.5f ) / resolution ); // half pixel offset
}
