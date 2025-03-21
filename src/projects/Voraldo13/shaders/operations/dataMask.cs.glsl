#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
layout( binding = 4, rgba16f ) uniform image3D lightBlock;

uniform bool useR;
uniform bool useG;
uniform bool useB;
uniform bool useA;
uniform vec4 color;
uniform vec4 variances;
uniform bool useLightR;
uniform bool useLightG;
uniform bool useLightB;
uniform vec3 lightValue;
uniform vec3 lightVariance;
uniform int mask;

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const uint previousMask = imageLoad( maskBlockBack, blockLocation ).x;
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );
	const vec3 previousLight = imageLoad( lightBlock, blockLocation ).xyz;

	bool doIt =
		( useR && abs( color.r - previousColor.r ) < variances.r ) ||
		( useG && abs( color.g - previousColor.g ) < variances.g ) ||
		( useB && abs( color.b - previousColor.b ) < variances.b ) ||
		( useA && abs( color.a - previousColor.a ) < variances.a ) ||
		( useLightR && abs( lightValue.r - previousLight.r ) < lightVariance.r ) ||
		( useLightG && abs( lightValue.g - previousLight.g ) < lightVariance.g ) ||
		( useLightB && abs( lightValue.b - previousLight.b ) < lightVariance.b );

	if ( doIt ) {
		imageStore( maskBlockFront, blockLocation, uvec4( max( previousMask, mask ) ) );
	} else {
		imageStore( maskBlockFront, blockLocation, uvec4( previousMask ) );
	}
	imageStore( colorBlockFront, blockLocation, previousColor );
}
