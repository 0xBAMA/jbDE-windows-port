#version 430
layout( local_size_x = 8, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D tridentStorage;
layout( binding = 1, rgba8ui ) uniform uimage2D writeTarget;

uniform ivec2 basePt;

uvec4 alphaBlend ( uvec4 a, uvec4 b ) {
	// blend a over b
	vec4 aa = vec4( a ) / 255.0f;
	vec4 bb = vec4( b ) / 255.0f;
	vec4 result;

	result.rgb = aa.rgb * aa.a + bb.rgb * ( 1.0f - aa.a );
	result.a = 1.0f;

	return uvec4( result * 255 );
}

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	uvec4 loadedCol = imageLoad( tridentStorage, loc );

	if ( loadedCol.a > 0 ) {
		if ( loadedCol.a != 255 ) { // alpha blend
			uvec4 existingBackground = imageLoad( writeTarget, loc + basePt );
			loadedCol = alphaBlend( loadedCol, existingBackground );
		}
		imageStore( writeTarget, loc + basePt, loadedCol );
	}
}
