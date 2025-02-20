#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

layout( rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( rgba8ui ) uniform uimage2D map; // convert to sampler? tbd
layout( rgba16f ) uniform image2D target;

// parameters
uniform ivec2 resolution;	// current screen resolution
uniform vec2 viewPosition;	// starting point for rendering
uniform int viewerHeight;	// viewer height
uniform float viewAngle;	// view direction angle
uniform float maxDistance;	// maximum traversal
uniform int horizonLine;	// horizon line position
uniform float heightScalar;	// scale value for height
uniform float offsetScalar;	// scale the side-to-side spread
uniform float fogScalar;	// scalar for fog distance
uniform float stepIncrement;// increase in step size as you get farther from the camera
uniform float FoVScalar;	// adjustment for the FoV

// uvec4 blueNoiseRef ( ivec2 location ) {
// 	location.x = location.x % imageSize( blueNoiseTexture ).x;
// 	location.y = location.y % imageSize( blueNoiseTexture ).y;
// 	return imageLoad( blueNoiseTexture, location );
// }

void DrawVerticalLine ( const uint x, const int yBottom, const int yTop, const vec4 col ) {
	// const float blueNoiseScale = 255.0f * 127.0f;
	const int yMin = clamp( yBottom, 0, imageSize( target ).y );
	const int yMax = clamp( yTop, 0, imageSize( target ).y );
	if ( yMin > yMax ) return;
	for ( int y = yMin; y < yMax; y++ ) {
		// float jitter = blueNoiseRef( ivec2( x, y ) ).r / blueNoiseScale;
		// imageStore( target, ivec2( x, y ), vec4( col.rgb, col.a + jitter ) );
		imageStore( target, ivec2( x, y ), col );
	}
}

mat2 Rotate2D ( float r ) {
	return mat2( cos( r ), sin( r ), -sin( r ), cos( r ) );
}

void main () {
	const float wPixels		= imageSize( target ).x;
	const float hPixels		= imageSize( target ).y;
	const uint myXIndex		= uint( gl_GlobalInvocationID.x );
	float yBuffer			= 0.0f;

	// initial clear
	DrawVerticalLine( myXIndex, 0, int( hPixels ), vec4( 0.0f ) );

	// FoV considerations
		// mapping [0..wPixels] to [-1..1]
	float FoVAdjust			= -1.0f + float( myXIndex ) * ( 2.0f ) / float( wPixels );
	const mat2 rotation		= Rotate2D( viewAngle + FoVAdjust * FoVScalar );
	const vec2 direction	= rotation * vec2( 1.0f, 0.0f );

	// need a side-to-side adjust to give some spread - CPU side adjustment scalar needs to be added
	vec3 sideVector			= cross( vec3( direction.x, 0.0f, direction.y ), vec3( 0.0f, 1.0f, 0.0f ) );
	vec2 viewPositionLocal	= viewPosition + sideVector.xz * FoVAdjust * offsetScalar;

	for ( float dSample = 1.0f, dz = 0.2f; dSample < maxDistance && yBuffer < hPixels; dSample += dz, dz += stepIncrement ) {
		const ivec2 samplePosition	= ivec2( viewPositionLocal + dSample * direction );
		const uvec4 dataSample		= imageLoad( map, samplePosition );
		const float heightOnScreen	= ( ( float( dataSample.a ) - viewerHeight ) * ( 1.0f / dSample ) * heightScalar + horizonLine );
		if ( heightOnScreen > yBuffer ) {
			uint depthTerm = uint( max( 0, 255 - dSample * fogScalar ) );
			DrawVerticalLine( myXIndex, int( yBuffer ), int( heightOnScreen ), vec4( dataSample.xyz, depthTerm ) / 255.0f );
			yBuffer = uint( heightOnScreen );
		}
	}
}