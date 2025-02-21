#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 1 ) uniform sampler2D outputImage;

uniform float time;

// #include "spaceFillingCurves.h.glsl"
// #include "colorRamps.glsl.h"
#include "mathUtils.h"

// // distance to line segment
// float sdSegment2 ( vec2 p, vec2 a, vec2 b ) {
// 	vec2 pa = p - a, ba = b - a;
// 	float h = clamp( dot( pa, ba ) / dot( ba, ba ), 0.0f, 1.0f );
// 	return length( pa - ba * h );
// }

uniform float scale;
uniform vec2 offset;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	vec3 color = vec3( 0.0f );

	// we got the click and drag much nicer this time
	const vec2 is = vec2( imageSize( accumulatorTexture ).xy );
	vec2 offsetLoc = vec2( writeLoc ) + vec2( 0.5f );
	vec2 normalizedPosition =  2.0f * offsetLoc / is.xy - vec2( 1.0f );
	const float aspectRatio = is.x / is.y;
	normalizedPosition.x *= aspectRatio;
	vec2 uv = scale * ( normalizedPosition ) + offset;

	const vec2 ts = textureSize( outputImage, 0 );
	vec2 texUV = uv * 1000.0f;
	if ( texUV.x >= 0 && texUV.y >= 0 && texUV.x < ts.x && texUV.y < ts.y ) {
		// this is were we sample the prepared image
		color = texture( outputImage, texUV / ts ).rgb;
	} else {
		// vertical and horizontal falloff factors
		const float vertical = max( -texUV.y, texUV.y - ts.y );
		const float horizontal = max( -texUV.x, texUV.x - ts.x );

		// midpoints
		const float xStepMid = smoothstep( 10.0f, 0.0f, abs( texUV.x - ts.x / 2.0f ) );
		const float yStepMid = smoothstep( 10.0f, 0.0f, abs( texUV.y - ts.y / 2.0f ) );

		// rule of thirds
		const float xStepThirds = smoothstep( 10.0f, 0.0f, abs( texUV.x - ts.x / 3.0f ) ) + smoothstep( 10.0f, 0.0f, abs( texUV.x - 2.0f * ts.x / 3.0f ) );
		const float yStepThirds = smoothstep( 10.0f, 0.0f, abs( texUV.y - ts.y / 3.0f ) ) + smoothstep( 10.0f, 0.0f, abs( texUV.y - 2.0f * ts.y / 3.0f ) );

		// golden ratio
		const float xStepGolden = smoothstep( 10.0f, 0.0f, abs( texUV.x - ts.x / 2.618f ) ) + smoothstep( 10.0f, 0.0f, abs( texUV.x - 1.618f * ts.x / 2.618f ) );
		const float yStepGolden = smoothstep( 10.0f, 0.0f, abs( texUV.y - ts.y / 2.618f ) ) + smoothstep( 10.0f, 0.0f, abs( texUV.y - 1.618f * ts.y / 2.618f ) );

		color = vec3(
			vec3( 0.1618f, 0.0618f, 0.0f ) * exp( -0.01f * max( vertical, horizontal ) ) * ( xStepMid + yStepMid ) + // midpoints
			vec3( 0.1618f, 0.0f, 0.0f ) * exp( -0.02f * max( vertical, horizontal ) ) * 0.75f * ( xStepGolden + yStepGolden ) + // golden ratio
			vec3( 0.0f, 0.0618f, 0.0618f ) * exp( -0.03f * max( vertical, horizontal ) ) * 0.75f * ( xStepThirds + yStepThirds ) // rule of thirds
		);

		// float biasLight = exp( -0.2f * max( vertical, horizontal ) );
		// color += vec3( biasLight );

	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( color, 1.0f ) );
}
