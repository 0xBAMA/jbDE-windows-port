#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
layout( binding = 4, rgba16f ) uniform image3D lightBlock;
layout( binding = 5, rgba8ui ) uniform uimage3D blueNoise;

uniform int radius;

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const vec4 lightValue = imageLoad( lightBlock, blockLocation );

	float alphaSum = 0.0;
	float totalWeight = 0.0;

	for ( int x = -radius; x <= radius; x++ ) {
		for ( int y = -radius; y <= radius; y++ ) {
			for ( int z = -radius; z <= radius; z++ ) {
				const float currentWeight = 1.0 - distance( vec3( 0.0 ), vec3( x, y, z ) );
				totalWeight += currentWeight;
				alphaSum += imageLoad( colorBlockBack, blockLocation + ivec3( x, y, z ) ).a * currentWeight;
			}
		}
	}

	vec4 newLight = lightValue * ( 1.0 - alphaSum / totalWeight );
	imageStore( lightBlock, blockLocation, newLight );
}
