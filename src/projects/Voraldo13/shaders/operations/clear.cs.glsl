#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;

uniform bool respectMask;
uniform vec4 color;
uniform bool draw;
uniform int mask;

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const uint previousMask = imageLoad( maskBlockBack, blockLocation ).x;
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );
	if ( previousMask > 0 && respectMask ) {
		vec4 blendedColor = mix( draw ? color : vec4( 0.0 ), previousColor, float( previousMask ) / 255.0 );
		imageStore( colorBlockFront, blockLocation, blendedColor );
		imageStore( maskBlockFront, blockLocation, uvec4( max( uint( mask ), previousMask ) ) );
	} else if ( draw ) {
		imageStore( colorBlockFront, blockLocation, color );
		imageStore( maskBlockFront, blockLocation, uvec4( max( uint( mask ), previousMask ) ) );
	} else {
		imageStore( colorBlockFront, blockLocation, vec4( 0.0 ) );
		imageStore( maskBlockFront, blockLocation, uvec4( previousMask ) );
	}
}
