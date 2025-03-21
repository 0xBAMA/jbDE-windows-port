#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
// layout( binding = 4, rgba16f ) uniform image3D lightBlock;

uniform bool respectMask;

void main () {
	// do I want to support masking with this one? tbd
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const uvec3 blockLocationU = uvec3( gl_GlobalInvocationID.xyz );
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );
	const uint previousMask = imageLoad( maskBlockBack, blockLocation ).x;

	const vec4 color = vec4( float( blockLocationU.x ^ blockLocationU.y ^ blockLocationU.z ) / 255.0 );

	vec4 blendedColor = respectMask ? mix( color, previousColor, float( previousMask ) / 255.0 ) : color;

	imageStore( colorBlockFront, blockLocation, blendedColor );
	imageStore( maskBlockFront, blockLocation, uvec4( previousMask ) );
}
