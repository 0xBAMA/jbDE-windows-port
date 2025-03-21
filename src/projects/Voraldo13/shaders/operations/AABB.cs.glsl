#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;

uniform ivec3 minCoords;
uniform ivec3 maxCoords;
uniform vec4 color;
uniform bool draw;
uniform int mask;

bool inBounds ( int x, int lo, int hi ) {
	return x == clamp( x, lo, hi );
}

bool inShape () {
	const ivec3 location = ivec3( gl_GlobalInvocationID.xyz );
	if ( inBounds( location.x, minCoords.x, maxCoords.x ) &&
		inBounds( location.y, minCoords.y, maxCoords.y ) &&
		inBounds( location.z, minCoords.z, maxCoords.z ) ) {
		return true;
	} else {
		return false;
	}
}

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const uint previousMask = imageLoad( maskBlockBack, blockLocation ).x;
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );
	if ( inShape() ) {
		vec4 blendedColor = mix( color, previousColor, float( previousMask ) / 255.0 );
		imageStore( colorBlockFront, blockLocation, draw ? blendedColor : previousColor );
		imageStore( maskBlockFront, blockLocation, uvec4( max( uint( mask ), previousMask ) ) );
	} else {
		imageStore( colorBlockFront, blockLocation, previousColor );
		imageStore( maskBlockFront, blockLocation, uvec4( previousMask ) );
	}
}
