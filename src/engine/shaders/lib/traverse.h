// #define CWBVH_COMPRESSED_TRIS // sync with tiny_bvh.h
// #define BVH4_GPU_COMPRESSED_TRIS // sync with tiny_bvh.h

// BVH traversal stack size 
#define STACK_SIZE 32

// I'm working on an AMD GPU right now
#define ISAMD

// Low-level optimizations for specific platforms
#ifdef ISINTEL // Iris Xe, Arc, ..
// #define USE_VLOAD_VSTORE
#define SIMD_AABBTEST
#elif defined ISNVIDIA // 2080, 3080, 4080, ..
#define USE_VLOAD_VSTORE
// #define SIMD_AABBTEST
#elif defined ISAMD
#define USE_VLOAD_VSTORE
// #define SIMD_AABBTEST
#else // unkown GPU
// #define USE_VLOAD_VSTORE
#define SIMD_AABBTEST
#endif

// ============================================================================
//
//        T R A V E R S E _ C W B V H
// 
// ============================================================================

// preliminaries

uint bfind( const uint v ) { // glsl alternative for CUDA's native bfind
	return findMSB( v );
}

uint popc( const uint v ) { return bitCount( v ); }
float _native_fma( const float a, const float b, const float c ) { return a * b + c; }
float fmin_fmin( const float a, const float b, const float c ) { return min( min( a, b ), c ); }
float fmax_fmax( const float a, const float b, const float c ) { return max( max( a, b ), c ); }

#define STACK_POP(X) { X = stack[--stackPtr]; }
#define STACK_PUSH(X) { stack[stackPtr++] = X; }

uint sign_extend_s8x4( const uint i ) {
	// docs: "with the given parameters, prmt will extend the sign to all bits in a byte."
	const uint b0 = ( ( i & 0x80000000u ) != 0 ) ? 0xff000000u : 0u;
	const uint b1 = ( ( i & 0x00800000u ) != 0 ) ? 0x00ff0000u : 0u;
	const uint b2 = ( ( i & 0x00008000u ) != 0 ) ? 0x0000ff00u : 0u;
	const uint b3 = ( ( i & 0x00000080u ) != 0 ) ? 0x000000ffu : 0u;
	return b0 + b1 + b2 + b3; // probably can do better than this.
}

#define UPDATE_HITMASK hitmask = (child_bits4 & 255) << (bit_index4 & 31)
#define UPDATE_HITMASK0 hitmask |= (child_bits4 & 255) << (bit_index4 & 31)
#define UPDATE_HITMASK1 hitmask |= ((child_bits4 >> 8) & 255) << ((bit_index4 >> 8) & 31);
#define UPDATE_HITMASK2 hitmask |= ((child_bits4 >> 16) & 255) << ((bit_index4 >> 16) & 31);
#define UPDATE_HITMASK3 hitmask |= (child_bits4 >> 24) << (bit_index4 >> 24);

// signed version
ivec4 as_char4 ( float x ) {
	int src = floatBitsToInt( x );
	return ivec4( // bitfieldExtract should sign extend, as appropriate
		bitfieldExtract( src, 0, 8 ),
		bitfieldExtract( src, 8, 8 ),
		bitfieldExtract( src, 16, 8 ),
		bitfieldExtract( src, 24, 8 )
	);
}

// unsigned version
uvec4 as_uchar4 ( float x ) {
	uint src = floatBitsToUint( x );
	return uvec4(
		bitfieldExtract( src, 0, 8 ),
		bitfieldExtract( src, 8, 8 ),
		bitfieldExtract( src, 16, 8 ),
		bitfieldExtract( src, 24, 8 )
	);
}

#ifdef SIMD_AABBTEST
#define float3or4 vec4
#else
#define float3or4 vec3
#endif
