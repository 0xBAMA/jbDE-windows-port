#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform sampler2D adamColor;
uniform usampler2D adamCount;

uniform float time;
uniform float percentDone;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 col = vec3( 0.0f );

	if ( writeLoc.x < 1024 ) {
		// textureQueryLevels gives number of mipmaps... not sure if that's really useful right now
		const int numLevels = textureQueryLevels( adamCount );

		// so we want to iterate through the mip chain, till we find... what?
		ivec2 loc = writeLoc;
		float value, valuePrevious;

		// initializing...
		value = valuePrevious = texelFetch( adamCount, loc, 0 ).r;

		// proceed from finest level of refinement, till we find the first nonzero
		for ( int i = 0; i <= numLevels; i++ ) {
			value = texelFetch( adamCount, loc, i ).r;
			if ( value != 0.0f ) {
				col = texelFetch( adamColor, loc, i ).rgb;
				break;
			}
			loc /= 2;
			valuePrevious = value;
		}
	} else {
		// col = vec3( 1.0f );
		col = texelFetch( adamColor, writeLoc - ivec2( 1024, 0 ), 0 ).rgb;
	}

	if ( writeLoc.y > 1028 && writeLoc.y < 1060 ) {
		col = vec3( ( ( writeLoc.x / 2048.0f ) < percentDone ) ? 1.0f : 0.0f );
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
