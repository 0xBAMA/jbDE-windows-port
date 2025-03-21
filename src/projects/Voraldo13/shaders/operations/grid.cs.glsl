#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;

uniform vec3 rotation;
uniform ivec3 spacing;
uniform ivec3 offsets;
uniform ivec3 width;
uniform vec4 color;
uniform bool draw;
uniform int mask;

mat3 RotationMatrix ( vec3 axis, float angle ) {
	axis = normalize( axis );
	float s = sin( angle );
	float c = cos( angle );
	float oc = 1.0 - c;
	return mat3( oc * axis.x * axis.x + c,          oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
				oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
				oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c );
}

bool inShape () {
	vec3 location = vec3( gl_GlobalInvocationID.xyz );

	location *= RotationMatrix( vec3( 1.0, 0.0, 0.0 ), rotation.x );
	location *= RotationMatrix( vec3( 0.0, 1.0, 0.0 ), rotation.y );
	location *= RotationMatrix( vec3( 0.0, 0.0, 1.0 ), rotation.z );

	bool x = ( ( int( location.x ) + offsets.x ) % spacing.x ) <= width.x;
	bool y = ( ( int( location.y ) + offsets.y ) % spacing.y ) <= width.y;
	bool z = ( ( int( location.z ) + offsets.z ) % spacing.z ) <= width.z;
	return ( ( x && y ) || ( x && z ) || ( y && z ) );
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
