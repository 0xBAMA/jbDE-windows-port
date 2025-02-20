#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

layout( rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( rgba8ui ) uniform uimage2D map; // convert to sampler? tbd
layout( rgba16f ) uniform image2D target;

uniform ivec2 resolution;			// current screen resolution
uniform vec2 viewPosition;			// starting point for rendering
uniform float viewAngle;			// view direction angle
uniform float minimapScalar;		// sizing on the minimap
uniform float viewBump;				// bumping the viewer back a bit
const float maxDistance = 600.0f;	// maximum traversal
const int viewerHeight = 200;		// viewer height
const int horizonLine = 1200;		// horizon line position
const float heightScalar = 150.0f;	// scale value for height
const float offsetScalar = 110.0f;	// scale the side-to-side spread
const float FoVScalar = 0.275f;		// adjustment for the FoV

void DrawVerticalLine ( const uint x, const int yBottom, const int yTop, const vec4 col ) {
	const int yMin = clamp( yBottom, 0, imageSize( target ).y );
	const int yMax = clamp( yTop, 0, imageSize( target ).y );
	if ( yMin > yMax ) return;
	for ( int y = yMin; y < yMax; y++ ) {
		imageStore( target, ivec2( x, y ), col );
	}
}

mat2 Rotate2D ( float r ) {
	return mat2( cos( r ), sin( r ), -sin( r ), cos( r ) );
}

// masked map reference - circle around viewer position
vec2 globalForwards = vec2( 0.0f );
uvec4 mapReference ( ivec2 location, out bool passing ) {
	vec2 location_local = vec2( location );
	location_local = vec2( ( vec2( location_local - viewPosition ) * ( 1.0f / minimapScalar ) ) + viewPosition );
	location_local += vec2( viewBump * globalForwards );
	const bool insideMask = distance( location_local, viewPosition + vec2( viewBump * globalForwards ) ) < ( 100.0f / minimapScalar );
	const bool outerRing = distance( location_local, viewPosition + vec2( viewBump * globalForwards ) ) > ( 97.0f / minimapScalar );
	if ( insideMask ) {
		uvec4 dataSample = imageLoad( map, ivec2( location_local ) );
		passing = true;
		if ( distance( location_local, viewPosition ) < ( 1.618f / minimapScalar ) ) {
			// little taller, to show the user's location
			dataSample.rgb = uvec3( 255, 0, 0 );
			dataSample.a += 20;
		} else if ( outerRing ) {
			passing = false;
			dataSample = uvec4( 0, 0, 0, dataSample.a );
		}
		return dataSample;
	} else {
		passing = false;
		return uvec4( 0 );
	}
}

void main () {
	const float wPixels		= resolution.x;
	const float hPixels		= imageSize( target ).y;
	const uint myXIndex		= uint( gl_GlobalInvocationID.x );
	float yBuffer			= 10.0f * hPixels / 24.0f;

	// no reason to run outside the width of the minimap
	if ( myXIndex > wPixels ) return;

	// initial clear
	DrawVerticalLine( myXIndex, int( yBuffer ), int( hPixels ), vec4( 0.0f, 0.0f, 0.0f, 0.0f ) );

	// FoV considerations
		// mapping [0..wPixels] to [-1..1]
	float FoVAdjust			= -1.0f + float( myXIndex ) * ( 2.0f ) / float( wPixels );

	const mat2 rotation		= Rotate2D( viewAngle + FoVAdjust * FoVScalar );
	const vec2 direction	= rotation * vec2( 1.0f, 0.0f );
	vec2 startCenter		= viewPosition - 200.0f * direction;
	const vec2 forwards		= globalForwards = Rotate2D( viewAngle ) * vec2( 1.0f, 0.0f );
	const vec2 fixedDirection = direction * ( dot( direction, forwards ) / dot( forwards, forwards ) );
	vec3 sideVector			= cross( vec3( forwards.x, 0.0f, forwards.y ), vec3( 0.0f, 1.0f, 0.0f ) );
	vec2 viewPositionLocal	= startCenter + sideVector.xz * FoVAdjust * offsetScalar;

	// draw the vertical strip of pixels for myXIndex
	const float dz = 0.25f;
	bool passing = false;
	for ( float dSample = 1.0f; dSample < maxDistance && yBuffer < hPixels; dSample += dz ) {
		const ivec2 samplePosition	= ivec2( viewPositionLocal + dSample * direction );
		const uvec4 dataSample		= mapReference( samplePosition, passing );
		const float heightOnScreen	= ( ( float( dataSample.a ) - viewerHeight ) * ( 1.0f / dSample ) * heightScalar + horizonLine );
		if ( heightOnScreen > yBuffer ) {
			DrawVerticalLine( myXIndex, int( yBuffer ), int( heightOnScreen ), vec4( dataSample.xyz, passing ? 255.0f : 0.0f ) / 255.0f );
			yBuffer = uint( heightOnScreen );
		}
	}
}