#version 430
void main() {
	switch( gl_VertexID ) {
		case 0: gl_Position = vec4( -1.0f, -1.0f, 0.9f, 1.0f ); break;
		case 1: gl_Position = vec4(  3.0f, -1.0f, 0.9f, 1.0f ); break;
		case 2: gl_Position = vec4( -1.0f,  3.0f, 0.9f, 1.0f ); break;
	}
}
