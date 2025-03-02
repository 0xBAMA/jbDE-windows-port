// #define CWBVH_COMPRESSED_TRIS // sync with tiny_bvh.h
// #define BVH4_GPU_COMPRESSED_TRIS // sync with tiny_bvh.h

// BVH traversal stack size 
#define STACK_SIZE 32

// ============================================================================
//
//        T R A V E R S E _ C W B V H
// 
// ============================================================================

// preliminaries
#ifndef TINYBVH_SUPPORT_FUNCS
#define TINYBVH_SUPPORT_FUNCS
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
	#define float3or4 vec3
#endif

// kernel
// based on CUDA code by AlanWBFT https://github.com/AlanIWBFT

vec4 TRAVERSALFUNC( const vec3 O, const vec3 D, const vec3 rD, const float t )
{
	// initialize ray
	vec4 hit;
	hit.x = t; // not fetching this from ray data to avoid one memory operation.
	// prepare traversal
	uvec2 stack[ STACK_SIZE ];
	uint hitAddr, stackPtr = 0;
	vec2 uv;
	float tmax = t;
	const uint octinv4 = ( 7 - ( ( D.x < 0 ? 4 : 0 ) | ( D.y < 0 ? 2 : 0 ) | ( D.z < 0 ? 1 : 0 ) ) ) * 0x1010101;
	uvec2 ngroup = uvec2( 0, 0x80000000 ), tgroup = uvec2( 0 );
	do {
		if ( ngroup.y > 0x00FFFFFF ) {
			const uint hits = ngroup.y, imask = ngroup.y;
			const uint child_bit_index = bfind( hits );
			const uint child_node_base_index = ngroup.x;
			ngroup.y &= ~( 1 << child_bit_index );
			if ( ngroup.y > 0x00FFFFFF ) { STACK_PUSH( ngroup ); }
			{
				const uint slot_index = ( child_bit_index - 24 ) ^ ( octinv4 & 255 );
				const uint relative_index = popc( imask & ~( 0xFFFFFFFF << slot_index ) );
				const uint child_node_index = ( child_node_base_index + relative_index ) * 5;
				vec4 n0 = NODEBUFFER[ child_node_index + 0 ], n1 = NODEBUFFER[ child_node_index + 1 ];
				vec4 n2 = NODEBUFFER[ child_node_index + 2 ], n3 = NODEBUFFER[ child_node_index + 3 ];
				vec4 n4 = NODEBUFFER[ child_node_index + 4 ];
				const ivec4 e = as_char4( n0.w );
				ngroup.x = floatBitsToUint( n1.x ), tgroup = uvec2( floatBitsToUint( n1.y ), 0 );
				uint hitmask = 0;

				const float idirx = uintBitsToFloat( uint( ( e.x + 127 ) << 23 ) ) * rD.x;
				const float idiry = uintBitsToFloat( uint( ( e.y + 127 ) << 23 ) ) * rD.y;
				const float idirz = uintBitsToFloat( uint( ( e.z + 127 ) << 23 ) ) * rD.z;
				const float origx = ( n0.x - O.x ) * rD.x;
				const float origy = ( n0.y - O.y ) * rD.y;
				const float origz = ( n0.z - O.z ) * rD.z;
		
				{	// first 4
					const uint meta4 = floatBitsToUint( n1.z ), is_inner4 = ( meta4 & ( meta4 << 1 ) ) & 0x10101010;
					const uint inner_mask4 = sign_extend_s8x4( is_inner4 << 3 );
					const uint bit_index4 = ( meta4 ^ ( octinv4 & inner_mask4 ) ) & 0x1F1F1F1F;
					const uint child_bits4 = ( meta4 >> 5 ) & 0x07070707;
					const vec4 lox4 = vec4( as_uchar4( rD.x < 0 ? n3.z : n2.x ) ), hix4 = vec4( as_uchar4( rD.x < 0 ? n2.x : n3.z ) );
					const vec4 loy4 = vec4( as_uchar4( rD.y < 0 ? n4.x : n2.z ) ), hiy4 = vec4( as_uchar4( rD.y < 0 ? n2.z : n4.x ) );
					const vec4 loz4 = vec4( as_uchar4( rD.z < 0 ? n4.z : n3.x ) ), hiz4 = vec4( as_uchar4( rD.z < 0 ? n3.x : n4.z ) );
					{
						float tminx0 = _native_fma( lox4.x, idirx, origx ), tminx1 = _native_fma( lox4.y, idirx, origx );
						float tminy0 = _native_fma( loy4.x, idiry, origy ), tminy1 = _native_fma( loy4.y, idiry, origy );
						float tminz0 = _native_fma( loz4.x, idirz, origz ), tminz1 = _native_fma( loz4.y, idirz, origz );
						float tmaxx0 = _native_fma( hix4.x, idirx, origx ), tmaxx1 = _native_fma( hix4.y, idirx, origx );
						float tmaxy0 = _native_fma( hiy4.x, idiry, origy ), tmaxy1 = _native_fma( hiy4.y, idiry, origy );
						float tmaxz0 = _native_fma( hiz4.x, idirz, origz ), tmaxz1 = _native_fma( hiz4.y, idirz, origz );
						n0.x = max( fmax_fmax( tminx0, tminy0, tminz0 ), 0 );
						n0.y = min( fmin_fmin( tmaxx0, tmaxy0, tmaxz0 ), tmax );
						n1.x = max( fmax_fmax( tminx1, tminy1, tminz1 ), 0 );
						n1.y = min( fmin_fmin( tmaxx1, tmaxy1, tmaxz1 ), tmax );
						if ( n0.x <= n0.y ) UPDATE_HITMASK;
						if ( n1.x <= n1.y ) UPDATE_HITMASK1;
						tminx0 = _native_fma( lox4.z, idirx, origx ), tminx1 = _native_fma( lox4.w, idirx, origx );
						tminy0 = _native_fma( loy4.z, idiry, origy ), tminy1 = _native_fma( loy4.w, idiry, origy );
						tminz0 = _native_fma( loz4.z, idirz, origz ), tminz1 = _native_fma( loz4.w, idirz, origz );
						tmaxx0 = _native_fma( hix4.z, idirx, origx ), tmaxx1 = _native_fma( hix4.w, idirx, origx );
						tmaxy0 = _native_fma( hiy4.z, idiry, origy ), tmaxy1 = _native_fma( hiy4.w, idiry, origy );
						tmaxz0 = _native_fma( hiz4.z, idirz, origz ), tmaxz1 = _native_fma( hiz4.w, idirz, origz );
						n0.x = max( fmax_fmax( tminx0, tminy0, tminz0 ), 0 );
						n0.y = min( fmin_fmin( tmaxx0, tmaxy0, tmaxz0 ), tmax );
						n1.x = max( fmax_fmax( tminx1, tminy1, tminz1 ), 0 );
						n1.y = min( fmin_fmin( tmaxx1, tmaxy1, tmaxz1 ), tmax );
						if ( n0.x <= n0.y ) UPDATE_HITMASK2;
						if ( n1.x <= n1.y ) UPDATE_HITMASK3;
					}
				}
				{	// second 4
					const uint meta4 = floatBitsToUint( n1.w ), is_inner4 = ( meta4 & ( meta4 << 1 ) ) & 0x10101010;
					const uint inner_mask4 = sign_extend_s8x4( is_inner4 << 3 );
					const uint bit_index4 = ( meta4 ^ ( octinv4 & inner_mask4 ) ) & 0x1F1F1F1F;
					const uint child_bits4 = ( meta4 >> 5 ) & 0x07070707;
					const vec4 lox4 = vec4( as_uchar4( rD.x < 0 ? n3.w : n2.y ) ), hix4 = vec4( as_uchar4( rD.x < 0 ? n2.y : n3.w ) );
					const vec4 loy4 = vec4( as_uchar4( rD.y < 0 ? n4.y : n2.w ) ), hiy4 = vec4( as_uchar4( rD.y < 0 ? n2.w : n4.y ) );
					const vec4 loz4 = vec4( as_uchar4( rD.z < 0 ? n4.w : n3.y ) ), hiz4 = vec4( as_uchar4( rD.z < 0 ? n3.y : n4.w ) );
					{
						float tminx0 = _native_fma( lox4.x, idirx, origx ), tminx1 = _native_fma( lox4.y, idirx, origx );
						float tminy0 = _native_fma( loy4.x, idiry, origy ), tminy1 = _native_fma( loy4.y, idiry, origy );
						float tminz0 = _native_fma( loz4.x, idirz, origz ), tminz1 = _native_fma( loz4.y, idirz, origz );
						float tmaxx0 = _native_fma( hix4.x, idirx, origx ), tmaxx1 = _native_fma( hix4.y, idirx, origx );
						float tmaxy0 = _native_fma( hiy4.x, idiry, origy ), tmaxy1 = _native_fma( hiy4.y, idiry, origy );
						float tmaxz0 = _native_fma( hiz4.x, idirz, origz ), tmaxz1 = _native_fma( hiz4.y, idirz, origz );
						n0.x = max( fmax_fmax( tminx0, tminy0, tminz0 ), 0 );
						n0.y = min( fmin_fmin( tmaxx0, tmaxy0, tmaxz0 ), tmax );
						n1.x = max( fmax_fmax( tminx1, tminy1, tminz1 ), 0 );
						n1.y = min( fmin_fmin( tmaxx1, tmaxy1, tmaxz1 ), tmax );
						if ( n0.x <= n0.y ) UPDATE_HITMASK0;
						if ( n1.x <= n1.y ) UPDATE_HITMASK1;
						tminx0 = _native_fma( lox4.z, idirx, origx ), tminx1 = _native_fma( lox4.w, idirx, origx );
						tminy0 = _native_fma( loy4.z, idiry, origy ), tminy1 = _native_fma( loy4.w, idiry, origy );
						tminz0 = _native_fma( loz4.z, idirz, origz ), tminz1 = _native_fma( loz4.w, idirz, origz );
						tmaxx0 = _native_fma( hix4.z, idirx, origx ), tmaxx1 = _native_fma( hix4.w, idirx, origx );
						tmaxy0 = _native_fma( hiy4.z, idiry, origy ), tmaxy1 = _native_fma( hiy4.w, idiry, origy );
						tmaxz0 = _native_fma( hiz4.z, idirz, origz ), tmaxz1 = _native_fma( hiz4.w, idirz, origz );
						n0.x = max( fmax_fmax( tminx0, tminy0, tminz0 ), 0 );
						n0.y = min( fmin_fmin( tmaxx0, tmaxy0, tmaxz0 ), tmax );
						n1.x = max( fmax_fmax( tminx1, tminy1, tminz1 ), 0 );
						n1.y = min( fmin_fmin( tmaxx1, tmaxy1, tmaxz1 ), tmax );
						if ( n0.x <= n0.y ) UPDATE_HITMASK2;
						if ( n1.x <= n1.y ) UPDATE_HITMASK3;
					}
				}
				ngroup.y = ( hitmask & 0xFF000000 ) | ( floatBitsToUint( n0.w ) >> 24 ), tgroup.y = hitmask & 0x00FFFFFF;
			}
		} else tgroup = ngroup, ngroup = uvec2( 0u );
		while ( tgroup.y != 0 ) {
			// Moller-Trumbore intersection; triangles are stored as 3x16 bytes,
			// with the original primitive index in the (otherwise unused) w
			// component of vertex 0.
			const uint triangleIndex = bfind( tgroup.y ), triAddr = tgroup.x + triangleIndex * 3;
			const vec3 e1 = TRIBUFFER[ triAddr ].xyz;
			const vec3 e2 = TRIBUFFER[ triAddr + 1 ].xyz;
			const vec4 v0 = TRIBUFFER[ triAddr + 2 ];
			tgroup.y -= 1 << triangleIndex;
			#ifndef CUSTOMLEAFTEST
				const vec3 r = cross( D.xyz, e1 );
				const float a = dot( e2, r );
				if ( abs( a ) < 0.0000001f )
					continue;
				const float f = 1 / a;
				const vec3 s = O.xyz - v0.xyz;
				const float u = f * dot( s, r );
				if ( u < 0 || u > 1 )
					continue;
				const vec3 q = cross( s, e2 );
				const float v = f * dot( D.xyz, q );
				if ( v < 0 || u + v > 1 )
					continue;
				const float d = f * dot( e1, q );
				if ( d <= 0.0f || d >= tmax )
					continue;
				uv = vec2( u, v ), tmax = d;
				hitAddr = floatBitsToUint( v0.w );
			#else
				// blade index, max distance, and solved UV... todo
				// CUSTOMLEAFTEST( floatBitsToUint( v0.w ), tmax, uv );

				// testing with ray-box from consistentPrimitives.h.glsl
				vec3 mins, maxs;

				// edges are precomputed, above - restore
				const vec3 v1 = e1 + v0.xyz; // edge1 = vertex1 - vertex0
				const vec3 v2 = e2 + v0.xyz; // edge2 = vertex2 - vertex0

				mins.x = min( min( v0.x, v1.x ), v2.x );
				mins.y = min( min( v0.y, v1.y ), v2.y );
				mins.z = min( min( v0.z, v1.z ), v2.z );

				maxs.x = max( max( v0.x, v1.x ), v2.x );
				maxs.y = max( max( v0.y, v1.y ), v2.y );
				maxs.z = max( max( v0.z, v1.z ), v2.z );

				const vec3 boxSize = abs( maxs - mins );
				const vec3 center = ( mins + maxs ) / 2.0f;

				vec3 normal;
				float d = iBoxOffset( O, D, normal, boxSize, center );
				if ( d <= 0.0f || d >= tmax )
					continue;
				uv = vec2( 0.0f ), tmax = d;
				hitAddr = floatBitsToUint( v0.w );
			#endif
		}
		if ( ngroup.y <= 0x00FFFFFF ) {
			if ( stackPtr > 0 ) {
				STACK_POP( ngroup );
			} else {
				hit = vec4( tmax, uv.x, uv.y, uintBitsToFloat( hitAddr ) );
				break;
			}
		}
	} while ( true );
	return hit;
}