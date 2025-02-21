#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// current state of the Cellular automata
layout( binding = 2 ) uniform usampler2D CAStateBuffer;
uniform vec2 resolution;

vec3 Jet ( float inputValue ) {
	const vec4 kRedVec4 = vec4( 0.13572138f, 4.61539260f, -42.66032258f, 132.13108234f );
	const vec4 kGreenVec4 = vec4( 0.09140261f, 2.19418839f, 4.84296658f, -14.18503333f );
	const vec4 kBlueVec4 = vec4( 0.10667330f, 12.64194608f, -60.58204836f, 110.36276771f );
	const vec2 kRedVec2 = vec2( -152.94239396f, 59.28637943f );
	const vec2 kGreenVec2 = vec2( 4.27729857f, 2.82956604f );
	const vec2 kBlueVec2 = vec2( -89.90310912f, 27.34824973f );
	inputValue = fract( inputValue );
	vec4 v4 = vec4( 1.0f, inputValue, inputValue * inputValue, inputValue * inputValue * inputValue );
	vec2 v2 = vec2( v4[ 2 ], v4[ 3 ] ) * v4.z;
	return vec3(
		dot( v4, kRedVec4 )   + dot( v2, kRedVec2 ),
		dot( v4, kGreenVec4 ) + dot( v2, kGreenVec2 ),
		dot( v4, kBlueVec4 )  + dot( v2, kBlueVec2 )
	);
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// current state of the cellular automata - sample the bit planes + sum a color value
	vec3 accum = vec3( 0.0f );
	uint state = texture( CAStateBuffer, ( vec2( writeLoc ) + vec2( 0.5f ) ) / resolution ).r;
	for ( uint bit = 0; bit < 32; bit++ ) {
		if ( ( ( state >> bit ) & 1u ) != 0 ) {
			accum += Jet( bit / 32.0f );
		}
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( accum / 3.0f, 1.0f ) );
}
