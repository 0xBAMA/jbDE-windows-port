#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, rgba32ui ) uniform uimage3D bufferTexture;

struct readoutData {
	uint radius;
	uint seed;
};

layout( binding = 0, std430 ) buffer dataBuffer { readoutData values[]; };

void main () {
	const ivec3 loc = ivec3( gl_GlobalInvocationID.xyz );
	const ivec3 iS = ivec3( imageSize( bufferTexture ).xyz );

	const uvec4 dataLoad = imageLoad( bufferTexture, loc );

	const int idx = iS.x * iS.y * loc.z + iS.x * loc.y + loc.x;
	values[ idx ].radius = dataLoad.g;	// radius comes from green channel
	values[ idx ].seed = dataLoad.b;	// seed comes from blue channel
}
