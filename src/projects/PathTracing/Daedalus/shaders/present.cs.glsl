#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba8ui ) readonly uniform uimage2D blueNoise;
layout( location = 1 ) uniform sampler2D preppedImage;
layout( location = 2, rgba8 ) writeonly uniform image2D outputImage;

uniform float scale;
uniform vec2 offset;

// configurable appearance
uniform bool edgeLines;
uniform vec3 edgeLinesColor;

uniform bool centerLines;
uniform vec3 centerLinesColor;

uniform bool ROTLines;
uniform vec3 ROTLinesColor;

uniform bool goldenLines;
uniform vec3 goldenLinesColor;

uniform bool vignette;

// https://www.shadertoy.com/view/wdK3Dy
float Grid( vec2 fragCoord, float space, float gridWidth ) {
	vec2 p  = fragCoord - vec2( 0.5f );
	vec2 size = vec2( gridWidth );
	vec2 a1 = mod( p - size, space );
	vec2 a2 = mod( p + size, space );
	vec2 a = a2 - a1;
	float g = min( a.x, a.y );
	return clamp( g, 0.0f, 1.0f );
}

// todo: try bgolus's thing
// https://www.shadertoy.com/view/mdVfWw

float BGolusPristineGrid( in vec2 uv, in vec2 ddx, in vec2 ddy, vec2 lineWidth ) {
	vec2 uvDeriv = vec2( length( vec2( ddx.x, ddy.x ) ), length( vec2( ddx.y, ddy.y ) ) );
	bvec2 invertLine = bvec2( lineWidth.x > 0.5f, lineWidth.y > 0.5f );
	vec2 targetWidth = vec2(
		invertLine.x ? 1.0f - lineWidth.x : lineWidth.x,
		invertLine.y ? 1.0f - lineWidth.y : lineWidth.y );
	vec2 drawWidth = clamp( targetWidth, uvDeriv, vec2( 0.5f ) );
	vec2 lineAA = uvDeriv * 1.5f;
	vec2 gridUV = abs( fract( uv ) * 2.0f - 1.0f );
	gridUV.x = invertLine.x ? gridUV.x : 1.0f - gridUV.x;
	gridUV.y = invertLine.y ? gridUV.y : 1.0f - gridUV.y;
	vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);

	grid2 *= clamp( targetWidth / drawWidth, 0.0f, 1.0f );
	grid2 = mix( grid2, targetWidth, clamp( uvDeriv * 2.0f - 1.0f, 0.0f, 1.0f ) );
	grid2.x = invertLine.x ? 1.0f - grid2.x : grid2.x;
	grid2.y = invertLine.y ? 1.0f - grid2.y : grid2.y;
	return mix( grid2.x, 1.0f, grid2.y );
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec2 pixelLocation = vec2( writeLoc ) * scale + offset;
	vec2 pixelLocation_neighbor = vec2( writeLoc + ivec2( 1, 0 ) ) * scale + offset;
	vec3 ditherValue = imageLoad( blueNoise, writeLoc % imageSize( blueNoise ) ).rgb * 0.0003f;
	bool zoomedIn = ( scale < 0.25f );

	vec2 deriv = vec2( 0.0f, abs( pixelLocation.x - pixelLocation_neighbor.x ) );
	vec3 result = vec3( zoomedIn ? BGolusPristineGrid( pixelLocation, deriv.yx, deriv.xy, vec2( 0.1f ) ) * 0.4f: 0.1f );

	// could jack in a little drop shadow, without much difficulty...

	ivec2 preppedSize = textureSize( preppedImage, 0 ).xy;
	// ivec2 preppedSize = imageSize( preppedImage ).xy;
	if ( pixelLocation.x < preppedSize.x && pixelLocation.y < preppedSize.y && pixelLocation.x >= 0 && pixelLocation.y >= 0 ) {

		ivec2 myPixelLocation = ivec2( pixelLocation );
		myPixelLocation.y = preppedSize.y - myPixelLocation.y - 1;
		vec3 myPixelColor = texture( preppedImage, ( myPixelLocation + vec2( 0.5f ) ) / vec2( preppedSize ) ).rgb;
		// vec3 myPixelColor = texelFetch( preppedImage, myPixelLocation, 0 ).rgb;
		result = clamp( myPixelColor - result, vec3( 0.0f ), vec3( 1.0f ) );
		if ( !zoomedIn ) {
			result = myPixelColor; // this may need to change to a fullscreen triangle so that mipmapped filtering Just Works tm
				// ( or else do the texture calls with explicit gradients, figure that out )
		}
	} else {
		vec2 values;

		if ( edgeLines ) { // markers at the corners, to get top, left, right, bottom outlines
			values = smoothstep( vec2( 1.25f ), vec2( 0.0f ), abs( pixelLocation ) );
			result += vec3( values.x + values.y ) * edgeLinesColor;
			values = smoothstep( vec2( 1.25f ), vec2( 0.0f ), abs( pixelLocation - preppedSize ) );
			result += vec3( values.x + values.y ) * edgeLinesColor;
		}

		if ( centerLines ) { // cross through center of the image
			values = smoothstep( vec2( 2.25f ), vec2( 0.0f ), abs( pixelLocation - ( preppedSize / 2.0f ) ) );
			result += vec3( values.x + values.y ) * centerLinesColor;
		}

		if ( ROTLines ) { // rule of thirds layout lines
			values = smoothstep( vec2( 1.25f ), vec2( 0.0f ), abs( pixelLocation - ( preppedSize / 3.0f ) ) );
			result += vec3( values.x + values.y ) * ROTLinesColor;

			values = smoothstep( vec2( 1.25f ), vec2( 0.0f ), abs( pixelLocation - ( 2.0f * preppedSize / 3.0f ) ) );
			result += vec3( values.x + values.y ) * ROTLinesColor;
		}

		if ( goldenLines ) { // golden ratio layout lines
			values = smoothstep( vec2( 1.25f ), vec2( 0.0f ), abs( pixelLocation - ( preppedSize / 2.618f ) ) );
			result += vec3( values.x + values.y ) * goldenLinesColor;

			values = smoothstep( vec2( 1.25f ), vec2( 0.0f ), abs( pixelLocation - ( 1.618f * preppedSize / 2.618f ) ) );
			result += vec3( values.x + values.y ) * goldenLinesColor;
		}

		result -= ditherValue;

		if ( vignette ) {
			// add vignetting - this should be configurable
			vec2 uv = ( vec2( writeLoc ) + vec2( 0.5f ) ) / vec2( imageSize( outputImage ) );
			uv *= 1.0f - uv.yx; result.rgb *= pow( uv.x * uv.y, 0.25f );
		}

		result -= 0.1f * ditherValue;
	}

	imageStore( outputImage, writeLoc, vec4( result, 1.0f ) );
}
