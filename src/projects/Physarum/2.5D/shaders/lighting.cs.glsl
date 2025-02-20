#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;

// store the result with the color + opacity included
layout( rgba16f ) uniform image3D shadedVolume;

uniform usampler2D continuum;
uniform vec3 resolution;
uniform vec3 color;
uniform float brightness;
uniform float zOffset;
uniform vec3 lightDirection;

// DDA voxel traversal
// https://www.shadertoy.com/view/3sKXDK
// https://www.shadertoy.com/view/NstSR8
// https://www.shadertoy.com/view/ldl3DS
// https://www.shadertoy.com/view/lsS3Dm
// https://www.shadertoy.com/view/ct33Rn

void main () {
	const vec3 myLocation = ( vec3( gl_GlobalInvocationID.xyz ) + vec3( 0.5f ) ) / imageSize( shadedVolume ).xyz;
	uint myValue = texture( continuum, myLocation.xy ).r;
	float densityFraction = brightness * ( myValue / 1000000.0f );

	bool aboveGround = ( ( gl_GlobalInvocationID.z / imageSize( shadedVolume ).z ) < densityFraction );

	// first light
	uint shadowDepth = 0;
	for ( int j = 0; j < 64; j++ ) {
		// shadowDepth += texture( continuum, myLocation + 0.01f * j * lightDirection ).r;
		// uint localValue = texture( continuum, samplePosition.xy ).r;
		vec3 samplePosition = vec3( 0.5f ) - myLocation + 0.01f * j * lightDirection;
		float fractionalZcoord = brightness * ( texture( continuum, samplePosition.xy ).r / 1000000.0f );
		if ( samplePosition.z < fractionalZcoord ) {
			shadowDepth += 10000;
		} else {
			shadowDepth += 100;
		}

	}

	// shade
	vec3 colorResult = brightness * densityFraction * color;
	colorResult *= exp( -float( shadowDepth / 400000.0f ) ) + 0.0618f;
	imageStore( shadedVolume, ivec3( gl_GlobalInvocationID.xyz ), vec4( colorResult, myValue / 16000.0f ) );
}