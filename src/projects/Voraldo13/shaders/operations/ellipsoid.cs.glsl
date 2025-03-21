#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;

uniform vec3 center;
uniform vec3 radii;
uniform vec3 rotations;
uniform vec4 color;
uniform bool draw;
uniform int mask;

mat3 RotationMatrix ( vec3 axis, float angle ) {
	vec3 ax = normalize( axis );
	float s = sin( angle );
	float c = cos( angle );
	float oc = 1.0 - c;
	return mat3( oc * ax.x * ax.x + c,        oc * ax.x * ax.y - ax.z * s,  oc * ax.z * ax.x + ax.y * s,
				oc * ax.x * ax.y + ax.z * s,  oc * ax.y * ax.y + c,         oc * ax.y * ax.z - ax.x * s,
				oc * ax.z * ax.x - ax.y * s,  oc * ax.y * ax.z + ax.x * s,  oc * ax.z * ax.z + c );
}

bool inShape () {
	const ivec3 location = ivec3( gl_GlobalInvocationID.xyz );

	//rotate the result, using the rotation vector
	vec3 local = location - center.xyz;
	local *= RotationMatrix( vec3( 1.0, 0.0, 0.0 ), -rotations.x );
	local *= RotationMatrix( vec3( 0.0, 1.0, 0.0 ), -rotations.y );
	local *= RotationMatrix( vec3( 0.0, 0.0, 1.0 ), -rotations.z );

	//test the result of the rotation, against the forumula for an ellipsoid:
	//   x^2 / a^2  +  y^2 / b^2  +  z^2 / c^2  =  1 ( <= to one for our purposes )
	//where x, y and z are the components of the result of the rotation
	//and a, b and c are the x,y and z radii components
	if ( ( pow( local.x, 2 ) / pow( radii.x, 2 ) +
		pow( local.y, 2 ) / pow( radii.y, 2 ) +
		pow( local.z, 2 ) / pow( radii.z, 2 ) ) <= 1 ) {
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
