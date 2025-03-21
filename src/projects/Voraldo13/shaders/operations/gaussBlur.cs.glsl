#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;

uniform int radius;
uniform bool respectMask;
uniform bool touchAlpha;

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const uint previousMask = imageLoad( maskBlockBack, blockLocation ).x;
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );

	float num = 0;
	vec4 sum = vec4( 0.0 );

	for( int x = -radius; x <= radius; x++ ) {
		for( int y = -radius; y <= radius; y++ ) {
			for( int z = -radius; z <= radius; z++ ) {
				const float weight = 1.0 - ( distance( vec3( 0.0 ), vec3( x, y, z ) ) );
				sum += weight * imageLoad( colorBlockBack, blockLocation + ivec3( x, y, z ) );
				num += weight;
			}
		}
	}

	sum /= num;
	sum = respectMask ? mix( sum, previousColor, float( previousMask ) / 255.0 ) : sum;
	sum.a = touchAlpha ? sum.a : previousColor.a;

	imageStore( colorBlockFront, blockLocation, sum );
	imageStore( maskBlockFront, blockLocation, uvec4( previousMask ) );
}
