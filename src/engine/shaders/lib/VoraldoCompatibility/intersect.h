float tMin, tMax; // global state tracking
bool Intersect ( const vec3 rO, vec3 rD ) {
// Intersect() code adapted from:
//    Amy Williams, Steve Barrus, R. Keith Morley, and Peter Shirley
//    "An Efficient and Robust Ray-Box Intersection Algorithm"
//    Journal of graphics tools, 10(1):49-54, 2005
	const float minDistance = -100.0;
	const float maxDistance =  100.0;
	int s[ 3 ]; // sign toggle
	// inverse of ray direction
	const vec3 iD = vec3( 1.0 ) / rD;
	s[ 0 ] = ( iD[ 0 ] < 0 ) ? 1 : 0;
	s[ 1 ] = ( iD[ 1 ] < 0 ) ? 1 : 0;
	s[ 2 ] = ( iD[ 2 ] < 0 ) ? 1 : 0;
	const vec3 min = vec3( -1.0, -1.0, -1.0 );
	const vec3 max = vec3(  1.0,  1.0,  1.0 );
	const vec3 b[ 2 ] = { min, max }; // bounds
	tMin = ( b[ s[ 0 ] ][ 0 ] - rO[ 0 ] ) * iD[ 0 ];
	tMax = ( b[ 1 - s[ 0 ] ][ 0 ] - rO[ 0 ] ) * iD[ 0 ];
	const float tYMin = ( b[ s[ 1 ] ][ 1 ] - rO[ 1 ] ) * iD[ 1 ];
	const float tYMax = ( b[ 1 - s[ 1 ] ][ 1 ] - rO[ 1 ] ) * iD[ 1 ];
	if ( ( tMin > tYMax ) || ( tYMin > tMax ) ) return false;
	if ( tYMin > tMin ) tMin = tYMin;
	if ( tYMax < tMax ) tMax = tYMax;
	const float tZMin = ( b[ s[ 2 ] ][ 2 ] - rO[ 2 ] ) * iD[ 2 ];
	const float tZMax = ( b[ 1 - s[ 2 ] ][ 2 ] - rO[ 2 ] ) * iD[ 2 ];
	if ( ( tMin > tZMax ) || ( tZMin > tMax ) ) return false;
	if ( tZMin > tMin ) tMin = tZMin;
	if ( tZMax < tMax ) tMax = tZMax;
	return ( ( tMin < maxDistance ) && ( tMax > minDistance ) );
}
