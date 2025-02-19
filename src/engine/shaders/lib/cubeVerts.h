
vec3 CubeVert( int idx ) {
// big const array is yucky - ALU LUT impl notes from vassvik
	// https://twitter.com/vassvik/status/1730961936794161579
	// https://twitter.com/vassvik/status/1730965355663655299
	const vec3 points[ 36 ] = {
		vec3( -1.0f,-1.0f,-1.0f ),
		vec3( -1.0f,-1.0f, 1.0f ),
		vec3( -1.0f, 1.0f, 1.0f ),
		vec3(  1.0f, 1.0f,-1.0f ),
		vec3( -1.0f,-1.0f,-1.0f ),
		vec3( -1.0f, 1.0f,-1.0f ),
		vec3(  1.0f,-1.0f, 1.0f ),
		vec3( -1.0f,-1.0f,-1.0f ),
		vec3(  1.0f,-1.0f,-1.0f ),
		vec3(  1.0f, 1.0f,-1.0f ),
		vec3(  1.0f,-1.0f,-1.0f ),
		vec3( -1.0f,-1.0f,-1.0f ),
		vec3( -1.0f,-1.0f,-1.0f ),
		vec3( -1.0f, 1.0f, 1.0f ),
		vec3( -1.0f, 1.0f,-1.0f ),
		vec3(  1.0f,-1.0f, 1.0f ),
		vec3( -1.0f,-1.0f, 1.0f ),
		vec3( -1.0f,-1.0f,-1.0f ),
		vec3( -1.0f, 1.0f, 1.0f ),
		vec3( -1.0f,-1.0f, 1.0f ),
		vec3(  1.0f,-1.0f, 1.0f ),
		vec3(  1.0f, 1.0f, 1.0f ),
		vec3(  1.0f,-1.0f,-1.0f ),
		vec3(  1.0f, 1.0f,-1.0f ),
		vec3(  1.0f,-1.0f,-1.0f ),
		vec3(  1.0f, 1.0f, 1.0f ),
		vec3(  1.0f,-1.0f, 1.0f ),
		vec3(  1.0f, 1.0f, 1.0f ),
		vec3(  1.0f, 1.0f,-1.0f ),
		vec3( -1.0f, 1.0f,-1.0f ),
		vec3(  1.0f, 1.0f, 1.0f ),
		vec3( -1.0f, 1.0f,-1.0f ),
		vec3( -1.0f, 1.0f, 1.0f ),
		vec3(  1.0f, 1.0f, 1.0f ),
		vec3( -1.0f, 1.0f, 1.0f ),
		vec3(  1.0f,-1.0f, 1.0f )
	};

	return points[ idx % 36 ];
}
