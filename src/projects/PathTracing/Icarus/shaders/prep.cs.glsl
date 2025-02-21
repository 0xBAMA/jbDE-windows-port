#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba32f ) uniform image2D outputBuffer;

uniform sampler2D adam;

void main () {
	// textureQueryLevels gives number of mipmaps... not sure if that's really useful, since I can pass it in
	const int numLevels = textureQueryLevels( adam );

	// so we want to iterate through the mip chain, till we find the first  mip that has data
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	ivec2 loc = writeLoc;
	vec3 color = vec3( 0.0f );

	// initializing...
	vec4 value = texelFetch( adam, loc, 0 );

	// proceed from coarsest to finest level of refinement, till we find the first nonzero count
		// we will go ahead and make the assumption that we will at least have data in the top mip
	for ( int i = 0; i <= numLevels; i++ ) {
		value = texelFetch( adam, loc, i );
		if ( value.a != 0.0f ) {
			color = value.rgb;
			// color = vec3( 1.0f / max( float( i ), 1.0f ) );
			break;
		}
		loc /= 2;
	}

	// now, need to do any desired postprocesing, on the loaded color...

	imageStore( outputBuffer, writeLoc, vec4( color, 1.0f ) );
}