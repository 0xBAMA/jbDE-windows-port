#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// the buffer we're splatting into
layout( binding = 2, r32ui ) uniform uimage3D SplatBuffer;

// the list of points we want to look at
layout( binding = 0, std430 ) buffer pointData {
	vec4 data[];
};

uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;

uniform int n;

// transform, number of points to consider
void main () {
	// load a vec4 from the buffer...
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;
	if ( index < n ) {

		vec4 pData = data[ index ];
		vec3 p = -basisX * 0.5f * pData.x + -basisY * 0.5f * pData.y + -basisZ * 0.5f * pData.z;

		// probably do something to normalize on x
		const vec3 iS = vec3( imageSize( SplatBuffer ).xyz );
		ivec3 pWrite = ivec3( p * iS + ( iS / 2.0f ) );
	//	ivec3 pWrite = ivec3( gl_GlobalInvocationID.xyz ) + ivec3( abs( p.xyz ) );

		imageAtomicAdd( SplatBuffer, pWrite, 1 );

	}
}
