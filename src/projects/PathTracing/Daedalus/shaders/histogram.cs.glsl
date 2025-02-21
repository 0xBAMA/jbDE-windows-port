#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( location = 0, rgba8 ) writeonly uniform image2D histogram;
layout( binding = 1, std430 ) readonly buffer colorHistograms {
	uint valuesR[ 256 ];
	uint maxValueR;
	uint valuesG[ 256 ];
	uint maxValueG;
	uint valuesB[ 256 ];
	uint maxValueB;
	uint valuesL[ 256 ];
	uint maxValueL;
};

void main () {
	// partition y into 4
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	const uint yBin = loc.y / 16;
	const float yFract = ( 16 - ( loc.y % 16 ) ) / 16.0f;

	const float power = 0.3f;
	vec4 color = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	switch ( yBin ) {
		// red
		case 0: if ( yFract <= pow( valuesR[ loc.x ] / float( maxValueR ), power ) ) color.r = 1.0f; break;
		// green
		case 1: if ( yFract <= pow( valuesG[ loc.x ] / float( maxValueG ), power ) ) color.g = 1.0f; break;
		// blue
		case 2: if ( yFract <= pow( valuesB[ loc.x ] / float( maxValueB ), power ) ) color.b = 1.0f; break;
		// luma
		case 3: if ( yFract <= pow( valuesL[ loc.x ] / float( maxValueL ), power ) ) color.rgb = vec3( 1.0f ); break;
	}

	imageStore( histogram, loc, color );
}