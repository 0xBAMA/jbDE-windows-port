#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
// layout( binding = 4, rgba16f ) uniform image3D lightBlock;

uniform ivec3 shiftAmount;
uniform bool wraparound;

void main () {
	// do I want to support masking with this one? tbd
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	ivec3 shiftedPosition = blockLocation - shiftAmount;
	if ( wraparound ) {
		shiftedPosition = shiftedPosition % imageSize( colorBlockBack );
	}

	uint mask = imageLoad( maskBlockBack, shiftedPosition ).x;
	vec4 color = imageLoad( colorBlockBack, shiftedPosition );
	// vec4 light = imageLoad( lightBlock, shiftedPosition );

	// because light is single buffered... still doesn't work - will need to revisit if I want to be able to do that
	// memoryBarrier();

	imageStore( maskBlockFront, blockLocation, uvec4( mask ) );
	imageStore( colorBlockFront, blockLocation, color );
	// imageStore( lightBlock, blockLocation, light );

}
