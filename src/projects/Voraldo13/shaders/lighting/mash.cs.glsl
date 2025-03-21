#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
layout( binding = 4, rgba16f ) uniform image3D lightBlock;
layout( binding = 5, rgba8ui ) uniform uimage3D blueNoise;

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );
	const vec4 lightValue = imageLoad( lightBlock, blockLocation );
	const vec4 color = vec4( previousColor.rgb * lightValue.rgb, previousColor.a );
	imageStore( colorBlockBack, blockLocation, color );
	imageStore( lightBlock, blockLocation, vec4( 1.0 ) );
}
