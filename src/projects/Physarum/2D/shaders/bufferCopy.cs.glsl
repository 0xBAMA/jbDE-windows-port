#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform usampler2D continuum;
uniform vec2 resolution;
uniform vec3 color;
uniform float brightness;

void main () {
	const vec2 sampleLocation = ( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.5f ) ) / resolution.xy;
	uint result = texture( continuum, sampleLocation ).r;

	// write the data to the accumulator
	imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( brightness * ( result / 1000000.0f ) * color, 1.0f ) );
}
