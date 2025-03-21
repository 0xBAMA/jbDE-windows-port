#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
layout( binding = 4, rgba16f ) uniform image2D heightmap;

uniform bool heightColor;
uniform float heightScalar;
uniform int upDirection;

uniform vec4 color;
uniform bool draw;
uniform int mask;

// maybe useful... tbd
// vec4 linearInterpolatedSample ( vec2 location ) {
// 	const vec2 fractionalPart = fract( location );
// 	const vec2 wholePart = floor( location );
// 	const vec4 sample0 = imageLoad( heightmap, ivec2( wholePart ) );
// 	const vec4 sample1 = imageLoad( heightmap, ivec2( wholePart ) + ivec2( 1, 0 ) );
// 	const vec4 sample2 = imageLoad( heightmap, ivec2( wholePart ) + ivec2( 0, 1 ) );
// 	const vec4 sample3 = imageLoad( heightmap, ivec2( wholePart ) + ivec2( 1, 1 ) );
// 	const vec4 xBlend0 = mix( sample0, sample1, fractionalPart.x );
// 	const vec4 xBlend1 = mix( sample2, sample3, fractionalPart.x );
// 	return mix( xBlend0, xBlend1, fractionalPart.y ) / 255.0;
// }

vec4 colorWrite = vec4( 0.0 );
bool inShape () {
	const vec3 location = vec3( gl_GlobalInvocationID.xyz );
	const vec2 blockDims = vec2( imageSize( heightmap ) );

	// const vec3 normalizedCoords = ( location + vec3( 0.5 ) ) / blockDims.xxx;
	const vec3 normalizedCoords = location / blockDims.xxx;

	vec2 xyCoord = vec2( 0.0 ); // texture uv
	float zCoord = 0.0;			// for height testing

	switch ( upDirection ) {
	case 0:	// +x is up
		xyCoord = normalizedCoords.yz;
		zCoord = normalizedCoords.x;
		break;
	case 1:	// -x is up
		xyCoord = normalizedCoords.yz;
		zCoord = 1.0 - normalizedCoords.x;
		break;
	case 2: // +y is up
		xyCoord = normalizedCoords.xz;
		zCoord = normalizedCoords.y;
		break;
	case 3:	// -y is up
		xyCoord = normalizedCoords.xz;
		zCoord = 1.0 - normalizedCoords.y;
		break;
	case 4: // +z is up
		xyCoord = normalizedCoords.xy;
		zCoord = normalizedCoords.z;
		break;
	case 5: // -z is up
		xyCoord = normalizedCoords.xy;
		zCoord = 1.0 - normalizedCoords.z;
		break;
	}

	// take a sample at the uv
	float heightVal = imageLoad( heightmap, ivec2( xyCoord * blockDims.xy ) ).x;
	colorWrite.xyz = heightColor ? ( heightVal * color.xyz ) : color.xyz;
	colorWrite.a = color.a;

	// test against the height
	heightVal *= heightScalar;
	return ( zCoord < heightVal );
	// make sure colorWrite is assigned to, here
}

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const uint previousMask = imageLoad( maskBlockBack, blockLocation ).x;
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );
	if ( inShape() ) {
		vec4 blendedColor = mix( colorWrite, previousColor, float( previousMask ) / 255.0 );
		imageStore( colorBlockFront, blockLocation, draw ? blendedColor : previousColor );
		imageStore( maskBlockFront, blockLocation, uvec4( max( uint( mask ), previousMask ) ) );
	} else {
		imageStore( colorBlockFront, blockLocation, previousColor );
		imageStore( maskBlockFront, blockLocation, uvec4( previousMask ) );
	}
}
