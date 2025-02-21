#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;

// results of the precompute step
layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D colorBuffer;

// forward pathtracing buffers
layout( r32ui ) uniform uimage3D redAtomic;
layout( r32ui ) uniform uimage3D greenAtomic;
layout( r32ui ) uniform uimage3D blueAtomic;
layout( r32ui ) uniform uimage3D countAtomic;

void main () {
	const ivec3 myLoc = ivec3( gl_GlobalInvocationID.xyz );
	const vec4 colorVal = imageLoad( colorBuffer, myLoc );
	const uint rVal = imageLoad( redAtomic, myLoc ).r;
	const uint gVal = imageLoad( greenAtomic, myLoc ).r;
	const uint bVal = imageLoad( blueAtomic, myLoc ).r;
	const uint cVal = imageLoad( countAtomic, myLoc ).r;
	const uint idxVal = imageLoad( idxBuffer, myLoc ).r;

	vec4 color = vec4(
		float( rVal ) / float( cVal ),
		float( gVal ) / float( cVal ),
		float( bVal ) / float( cVal ),
		cVal / 1000000.0f
	);

	imageStore( colorBuffer, myLoc, color );
}
