#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

//layout( binding = 0, rgba32f ) uniform image2D layerN;			// the larger image, being downsampled
uniform sampler2D layerN; // redoing as a sampler, so that we can do a texture gather
layout( binding = 1, r32f ) uniform image2D layerNPlusOne;	// the smaller image, being written to

uniform ivec2 dims;

void main () {
	// pixel location on layer N + 1... we are dispatching for texels in layer N + 1
	const ivec2 newLoc = ivec2( gl_GlobalInvocationID.xy );

	// half res...
	const ivec2 oldLoc = newLoc * 2;

	// bounds check
	if ( oldLoc.x < dims.x && oldLoc.y < dims.y ) {

		// find the local maximum in the red channel texels
		const vec4 gR = textureGather( layerN, oldLoc, 0 );
		const float gR_max = max( 0.0f, max( max( gR.x, gR.y ), max( gR.z, gR.w ) ) );

		// and store the result for layer N + 1
		imageStore( layerNPlusOne, newLoc, vec4( gR_max ) );

	}
}