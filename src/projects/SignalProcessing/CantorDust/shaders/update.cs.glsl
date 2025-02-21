#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

uniform uint byteOffset;
uniform uint numBytes;

// the 2D and 3D histogram images
layout( binding = 2, r32ui ) uniform uimage2D dataTexture2D;
layout( binding = 3, r32ui ) uniform uimage3D dataTexture3D;

// ssbo for data bytes
layout( binding = 0, std430 ) buffer dataBuffer { uint data[]; };

// ssbo for 2d histogram max
layout( binding = 1, std430 ) buffer histogram2DMax { uint max2D; };

// ssbo for 3d histogram max
layout( binding = 2, std430 ) buffer histogram3DMax { uint max3D; };

// need to unpack uint8s from full fat uint32s
uint getByte ( uint index ) {
	// since we pack 4 bytes into each uint...
	uint base = index / 4;

	// get the initial uint value (4 bytes)
	uint value = data[ base ];

	// looking at the remainder, to determine how to mask
	switch ( index - ( base * 4 ) ) {
		case 0:
		value = 0xFF000000u & value;
		value = value >> 24;
		break;

		case 1:
		value = 0x00FF0000u & value;
		value = value >> 16;
		break;

		case 2:
		value = 0x0000FF00u & value;
		value = value >> 8;
		break;

		case 3:
		value = 0x000000FFu & value;
		break;
	}
	return value;
}

void main () {
	// byte index
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;

	if ( index >= byteOffset && index < byteOffset + numBytes )  {
	// populating the histogram
		// load this byte, and the two following bytes
		uint bytes[ 3 ];
		bytes[ 0 ] = getByte( index );
		bytes[ 1 ] = getByte( index + 1 );
		bytes[ 2 ] = getByte( index + 2 );

		// 2d histogram considers only this byte and the next
		atomicMax( max2D, imageAtomicAdd( dataTexture2D, ivec2( bytes[ 0 ], bytes[ 1 ] ), 1 ) + 1 );

		// 3d histogram considers this byte and the two following
		atomicMax( max3D, imageAtomicAdd( dataTexture3D, ivec3( bytes[ 0 ], bytes[ 1 ], bytes[ 2 ] ), 1 ) + 1 );
	}
}
