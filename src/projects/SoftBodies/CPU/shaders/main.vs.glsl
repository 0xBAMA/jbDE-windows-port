#version 450

in vec4 vPosition;
in vec4 vColor;
in vec4 vtColor;

// mode 0 is for points
// mode 1 is for the colored lines
// mode 2 is to use the tension/compression colors / outlines
// mode 3 is for triangles
uniform int colorMode;

// point highlight - gl_PointSize must be set in all cases if it is set at all
uniform float defaultPointSize;
uniform int nodeSelect;

uniform float aspect_ratio;
uniform mat4 perspective;

uniform mat3 orientation;

out vec4 color;
out vec3 position;
out vec3 normal;

void main() {
	position = orientation * vPosition.xyz;

	gl_Position = vec4( position, 1.0f );
	gl_Position.x /= aspect_ratio;

	position = gl_Position.xyz;

	normal = vec3( 0.0f );
	switch ( colorMode ) {
		case 0: // points
			gl_Position.z -= 0.001f;
			// if ( gl_VertexID == nodeSelect ) { // highlight
			//   gl_PointSize = 1.618 * defaultPointSize;
			//   color = vec4( 1.0, 0.0, 0.0, 1.0 );
			// } else {
			gl_PointSize = vPosition.a;
			color = vColor;
			color.a *= 1.5f - distance( vec3( 0.0f ), vPosition.xyz );
			// }
			break;

		case 1: // regular lines
			gl_Position.z -= 0.001f;
			color = vColor;
			break;

		case 2: // outline lines
			color = vtColor;
			break;

		case 3: // triangles
			normal = orientation * vtColor.xyz;
			color = vColor;

		// color = vPosition;
			gl_Position.z += 0.001f;
			break;
	}


	gl_Position.z *= 0.3;
}
