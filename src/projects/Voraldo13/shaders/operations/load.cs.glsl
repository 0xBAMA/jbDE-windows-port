#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
layout( binding = 4, rgba8 ) uniform image3D loadBuffer;

uniform bool respectMask;

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );
	const uint previousMask = imageLoad( maskBlockBack, blockLocation ).x;
	const vec4 loadedColor = imageLoad( loadBuffer, blockLocation );

	const vec4 blendedColor = respectMask ? mix( loadedColor, previousColor, float( previousMask ) / 255.0 ) : loadedColor;

	imageStore( maskBlockFront, blockLocation, uvec4( previousMask ) );
	imageStore( colorBlockFront, blockLocation, blendedColor );
}
