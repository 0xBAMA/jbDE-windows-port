#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// composited color
uniform sampler2D depthResult;
uniform sampler2D colorResult;

uniform float blendRate;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	const vec2 uv = vec2( writeLoc + 0.5f ) / vec2( imageSize( accumulatorTexture ) );
	vec3 col = texture( colorResult, uv ).rgb;

	// write the data to the image
	vec4 previous = imageLoad( accumulatorTexture, writeLoc );
	imageStore( accumulatorTexture, writeLoc, vec4( mix( col, previous.rgb, vec3( blendRate ) ), 1.0f ) );
}
