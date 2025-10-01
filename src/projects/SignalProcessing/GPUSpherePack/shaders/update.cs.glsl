#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, rgba32ui ) uniform uimage3D bufferTextureSrc;
layout( binding = 3, rgba32ui ) uniform uimage3D bufferTextureDst;

/* the buffer texture is:

	red: distance
	green: radius
	blue: seed

*/

uniform int resetFlag;
uniform uint wangSeed;

#include "random.h"
#include "noise.h"
#include "mathUtils.h"
#include "wood.h"

const float minRadius = 1.5f;
const float maxRadius = 50.0f;

float distanceToNearestBoundary ( vec3 p ) {
	const ivec3 iS = ( imageSize( bufferTextureSrc ).xyz );
	return min( min( min( min( min( p.x, p.y ), p.z ), max( 0, p.x - iS.x ) ), max( 0, p.y - iS.y ) ), max( 0, p.z - iS.z ) ) + 10.0f;
}

void main () {
	// dispatch associated voxel
	const ivec3 loc = ivec3( gl_GlobalInvocationID.xyz );
	const vec3 location = vec3( loc ) + vec3( 0.5f );
	seed = wangSeed;

	const mat3 rot = Rotate3D( 8.2f, normalize( vec3( 3.0f, 2.0f, 4.0f ) ) );

	if ( resetFlag == 1 ) { // ensure we have cleared to an initial state
		imageStore( bufferTextureSrc, loc, uvec4( floatBitsToUint( maxRadius ),
			floatBitsToUint( maxRadius ), 0u, 0u ) );
	} else {
	// check several points inside the voxel? tbd

		// the seed defines a random point... all voxels share this... small amount of padding (5 vox)
		const vec3 newPointLocation = vec3( 5.0f ) + vec3( NormalizedRandomFloat(),
			NormalizedRandomFloat(), NormalizedRandomFloat() ) *
				vec3( imageSize( bufferTextureSrc ).xyz - vec3( 10.0f ) );

		// state for one iteration is a sample at the voxel location and one at the new point location
		const uvec4 oldPointSample = imageLoad( bufferTextureSrc, loc );
		const uvec4 newPointSample = imageLoad( bufferTextureSrc, ivec3( newPointLocation ) );

		// we get our new radius... if you use a noise field to inform it, that could be interesting
			// two clamps: inner clamp is for the boundary... outer clamp is enforcing a min and max radius

		const float noise0 = perlinfbm( rot * newPointLocation * 2.4f, 1.8f / imageSize( bufferTextureDst ).x, 3 ) + 0.1f;
		const bool otherRejectCriteria = noise0 > 0.0f;
		uvec4 writeValue = oldPointSample;

		if ( !otherRejectCriteria ) {
			const float noise = matWood( rot * newPointLocation * 0.001f ).r;
			const float newRadius = clamp( noise * 15.0f * saturate( 0.8f + NormalizedRandomFloat() ), minRadius, uintBitsToFloat( newPointSample.x ) );
			const float oldDistance = uintBitsToFloat( oldPointSample.x );
			const float newDistance = distance( location, newPointLocation ) - newRadius;

			// making a determination about what we want to write
			if ( newRadius < minRadius || oldDistance < newDistance || otherRejectCriteria ) {
			// THE SPHERE OVERLAPS...or else it's too small, or we just want to get rid of it. No good. Keep the old data.
				// keep value
			} else {
			// We want to take the new data
				writeValue = uvec4( floatBitsToUint( newDistance ), floatBitsToUint( newRadius ), wangSeed, 0u );
			}
		}

		// storing the image data back
		imageStore( bufferTextureDst, loc, writeValue );
	}
}
