// ==============================================================================================
// ====== Old Test Chamber ======================================================================

float deOldTestChamber ( vec3 p ) {
	// init nohit, far from surface, no diffuse color
	hitSurfaceType = NOHIT;
	float sceneDist = 1000.0f;
	hitColor = vec3( 0.0f );

	const vec3 whiteWallColor = vec3( 0.618f );
	const vec3 floorCielingColor = vec3( 0.9f );

	// North, South, East, West walls
	const float dNorthWall = fPlane( p, vec3( 0.0f, 0.0f, -1.0f ), 48.0f );
	const float dSouthWall = fPlane( p, vec3( 0.0f, 0.0f, 1.0f ), 48.0f );
	const float dEastWall = fPlane( p, vec3( -1.0f, 0.0f, 0.0f ), 10.0f );
	const float dWestWall = fPlane( p, vec3( 1.0f, 0.0f, 0.0f ), 10.0f );
	const float dWalls = fOpUnionRound( fOpUnionRound( fOpUnionRound( dNorthWall, dSouthWall, 0.5f ), dEastWall, 0.5f ), dWestWall, 0.5f );
	sceneDist = min( dWalls, sceneDist );
	if ( sceneDist == dWalls && dWalls < epsilon ) {
		hitColor = whiteWallColor;
		hitSurfaceType = DIFFUSE;
	}

	const float dFloor = fPlane( p, vec3( 0.0f, 1.0f, 0.0f ), 4.0f );
	sceneDist = min( dFloor, sceneDist );
	if ( sceneDist == dFloor && dFloor < epsilon ) {
		hitColor = floorCielingColor;
		hitSurfaceType = DIFFUSE;
	}

	// balcony floor
	const float dEastBalcony = fBox( p - vec3( 10.0f, 0.0f, 0.0f ), vec3( 4.0f, 0.1f, 48.0f ) );
	const float dWestBalcony = fBox( p - vec3( -10.0f, 0.0f, 0.0f ), vec3( 4.0f, 0.1f, 48.0f ) );
	const float dBalconies = min( dEastBalcony, dWestBalcony );
	sceneDist = min( dBalconies, sceneDist );
	if ( sceneDist == dBalconies && dBalconies < epsilon ) {
		hitColor = floorCielingColor;
		hitSurfaceType = DIFFUSE;
	}

	// store point value before applying repeat
	const vec3 pCache = p;
	pMirror( p.x, 0.0f );

	// compute bounding box for the rails on both sides, using the mirrored point
	const float dRailBounds = fBox( p - vec3( 7.0f, 1.625f, 0.0f ), vec3( 1.0f, 1.2f, 48.0f ) );

	// if railing bounding box is true
	float dRails;
	if ( dRailBounds < 0.0f ) {
		// railings - probably use some instancing on them, also want to use a bounding volume
		dRails = fCapsule( p, vec3( 7.0f, 2.4f, 48.0f ), vec3( 7.0f, 2.4f, -48.0f ), 0.3f );
		dRails = min( dRails, fCapsule( p, vec3( 7.0f, 0.6f, 48.0f ), vec3( 7.0f, 0.6f, -48.0f ), 0.1f ) );
		dRails = min( dRails, fCapsule( p, vec3( 7.0f, 1.1f, 48.0f ), vec3( 7.0f, 1.1f, -48.0f ), 0.1f ) );
		dRails = min( dRails, fCapsule( p, vec3( 7.0f, 1.6f, 48.0f ), vec3( 7.0f, 1.6f, -48.0f ), 0.1f ) );
		sceneDist = min( dRails, sceneDist );
		if ( sceneDist == dRails && dRails <= epsilon ) {
			hitColor = vec3( 0.618f );
			hitSurfaceType = METALLIC;
		}
	} // end railing bounding box

	// revert to original point value
	p = pCache;

	pMod1( p.x, 14.0f );
	p.z += 2.0f;
	pModMirror1( p.z, 4.0f );
	float dArches = fBox( p - vec3( 0.0f, 4.9f, 0.0f ), vec3( 10.0f, 5.0f, 5.0f ) );
	dArches = fOpDifferenceRound( dArches, fRoundedBox( p - vec3( 0.0f, 0.0f, 3.0f ), vec3( 10.0f, 4.5f, 1.0f ), 3.0f ), 0.2f );
	dArches = fOpDifferenceRound( dArches, fRoundedBox( p, vec3( 3.0f, 4.5f, 10.0f ), 3.0f ), 0.2f );

	// if railing bounding box is true
	if ( dRailBounds < 0.0f ) {
		dArches = fOpDifferenceRound( dArches, dRails - 0.05f, 0.1f );
	} // end railing bounding box

	sceneDist = min( dArches, sceneDist );
	if ( sceneDist == dArches && dArches < epsilon ) {
		hitColor = floorCielingColor;
		hitSurfaceType = DIFFUSE;
	}

	p = pCache;

	// the bar lights are the primary source of light in the scene
	const float dCenterLightBar = fBox( p - vec3( 0.0f, 7.4f, 0.0f ), vec3( 1.0f, 0.1f, 48.0f ) );
	sceneDist = min( dCenterLightBar, sceneDist );
	if ( sceneDist == dCenterLightBar && dCenterLightBar <=epsilon ) {
		hitColor = 0.6f * GetColorForTemperature( 6500.0f );
		hitSurfaceType = EMISSIVE;
	}

	const vec3 coolColor = 0.8f * pow( GetColorForTemperature( 1000000.0f ), vec3( 3.0f ) );
	const vec3 warmColor = 0.8f * pow( GetColorForTemperature( 1000.0f ), vec3( 1.2f ) );

	const float dSideLightBar1 = fBox( p - vec3( 7.5f, -0.4f, 0.0f ), vec3( 0.618f, 0.05f, 48.0f ) );
	sceneDist = min( dSideLightBar1, sceneDist );
	if ( sceneDist == dSideLightBar1 && dSideLightBar1 <= epsilon ) {
		hitColor = coolColor;
		hitSurfaceType = EMISSIVE;
	}

	const float dSideLightBar2 = fBox( p - vec3( -7.5f, -0.4f, 0.0f ), vec3( 0.618f, 0.05f, 48.0f ) );
	sceneDist = min( dSideLightBar2, sceneDist );
	if ( sceneDist == dSideLightBar2 && dSideLightBar2 <= epsilon ) {
		hitColor = warmColor;
		hitSurfaceType = EMISSIVE;
	}

	return sceneDist;
}