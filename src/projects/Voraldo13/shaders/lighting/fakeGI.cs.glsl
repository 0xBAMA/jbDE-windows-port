#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 1 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;
layout( binding = 4, rgba16f ) uniform image3D lightBlock;
layout( binding = 5, rgba8ui ) uniform uimage3D blueNoise;

uniform int index;
uniform int upDirection;
uniform float sFactor;
uniform float alphaThreshold;
uniform vec4 skyIntensity;

ivec3 blockSize;
bool inBounds ( ivec3 location ) {
	const ivec3 blockDim = imageSize( colorBlockBack );
	return
		location.x < blockDim.x && location.x >= 0 &&
		location.y < blockDim.y && location.y >= 0 &&
		location.z < blockDim.z && location.z >= 0;
}

void main () {
	ivec3 bases[ 3 ]; // x, y, z basis
	switch ( upDirection ) {
	case 0: // +x means we run from xmax to xmin
		bases[ 0 ] = ivec3( 0, 1, 0 );
		bases[ 1 ] = ivec3( 0, 0, 1 );
		bases[ 2 ] = ivec3( 1, 0, 0 );
		break;
	case 1: // -x means we run from xmin to xmax
		bases[ 0 ] = ivec3( 0, 1, 0 );
		bases[ 1 ] = ivec3( 0, 0, 1 );
		bases[ 2 ] = ivec3(-1, 0, 0 );
		break;
	case 2: // +y means we run from ymax to ymin
		bases[ 0 ] = ivec3( 1, 0, 0 );
		bases[ 1 ] = ivec3( 0, 0, 1 );
		bases[ 2 ] = ivec3( 0, 1, 0 );
		break;
	case 3: // -y means we run from ymin to ymax
		bases[ 0 ] = ivec3( 1, 0, 0 );
		bases[ 1 ] = ivec3( 0, 0, 1 );
		bases[ 2 ] = ivec3( 0,-1, 0 );
		break;
	case 4: // +z means we run from zmax to zmin
		bases[ 0 ] = ivec3( 1, 0, 0 );
		bases[ 1 ] = ivec3( 0, 1, 0 );
		bases[ 2 ] = ivec3( 0, 0, 1 );
		break;
	case 5: // -z means we run from zmin to zmax
		bases[ 0 ] = ivec3( 1, 0, 0 );
		bases[ 1 ] = ivec3( 0, 1, 0 );
		bases[ 2 ] = ivec3( 0, 0,-1 );
		break;
	}

	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	blockSize = imageSize( colorBlockBack ) - ivec3( 1 );

	// even ones start at max, odd start at min
	const bool even = upDirection % 2 == 0;
	const ivec3 startLocation = loc.x * bases[ 0 ] + loc.y * bases[ 1 ] + ( ( even ) ? blockSize.z - index : index ) * abs( bases[ 2 ] );

	vec4 previousLight = imageLoad( lightBlock, startLocation );
	vec4 previousColor = imageLoad( colorBlockBack, startLocation );

	vec4 newLightVal = previousLight;
	if ( previousColor.a > alphaThreshold ) { // opaque enough to participate
		for ( int dx = -1; dx <= 1; dx++ ) {
			for ( int dy = -1; dy <= 1; dy++ ) {
				ivec3 checkLoc = startLocation + dx * bases[ 0 ] + dy * bases[ 1 ] + bases[ 2 ];
				bool hit = false;
				while( inBounds( checkLoc ) ) {
					if ( imageLoad( colorBlockBack, checkLoc ).a >= alphaThreshold ) {
						// take a sample of the lighting at this location, add sFactor times that value to newLightVal
						newLightVal += imageLoad( lightBlock, checkLoc ) * sFactor;
						hit = true;
						break;
					}
					// increment checkLoc by the step
					checkLoc += dx * bases[ 0 ];
					checkLoc += dy * bases[ 1 ];
					checkLoc += bases[ 2 ];
				}
				if ( !hit ) {
					// the ray escaped, and the contribution is set by the sky color
					newLightVal += skyIntensity * skyIntensity.w;
				}
			}
		}
		imageStore( lightBlock, startLocation, newLightVal );
	}
}
