#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;

// utils
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// results of the precompute step
layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D dataCacheBuffer;

// breaking up the work
uniform ivec3 offset;

// sphere data
const int numSpheres = 65535;
struct sphere_t {
	vec4 positionRadius;
	vec4 colorMaterial;
};
layout( binding = 0, std430 ) buffer sphereData {
	sphere_t spheres[];
};

float blueNoiseRead ( ivec2 loc, int idx ) {
	ivec2 wrappedLoc = loc % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	switch ( idx ) {
	case 0:
		return sampleValue.r / 255.0f;
		break;
	case 1:
		return sampleValue.g / 255.0f;
		break;
	case 2:
		return sampleValue.b / 255.0f;
		break;
	case 3:
		return sampleValue.a / 255.0f;
		break;
	}
}

void main () {
	// voxel location
	const ivec3 writeLoc = ivec3( gl_GlobalInvocationID.xyz ) * 8 + offset;
	vec4 color = imageLoad( dataCacheBuffer, writeLoc );

	// do a traversal through the image, accumulate optical depth
		// shadowing comes from exp( -depth )
	
	const vec3 Direction = normalize( vec3( 1.0f, 1.0f, 0.85f ) );
	// const vec3 Direction = normalize( vec3( 1280.0f, 720.0f, 128.0f ) / 2.0f - writeLoc );
	const vec3 subvoxelOffset = vec3(
		blueNoiseRead( writeLoc.xy, 0 ),
		blueNoiseRead( writeLoc.xy, 1 ),
		blueNoiseRead( writeLoc.xy, 2 )
	);

	// DDA traversal
	vec3 deltaDist = 1.0f / abs( Direction );
	ivec3 rayStep = ivec3( sign( Direction ) );
	bvec3 mask0 = bvec3( false );
	ivec3 mapPos0 = ivec3( floor( writeLoc + 0.0f ) );
	vec3 sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - writeLoc + subvoxelOffset ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;

	float opticalDepthSum = 0.0f;

	#define MAX_RAY_STEPS 1200
	for ( int i = 0; i < MAX_RAY_STEPS; i++ ) {
		// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
		bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
		vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
		ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

		vec4 read = imageLoad( dataCacheBuffer, mapPos0 );
		opticalDepthSum += clamp( read.a, 0.0f, 1.0f );

		// if ( read.a != 0.0f ) { // this should be the hit condition
		// 	col = vec3( 1.0f - exp( -i ) ) * read.rgb;
		// 	// col = read.rgb;
		// 	break;
		// }

		sideDist0 = sideDist1;
		mapPos0 = mapPos1;
	}

	// scale with the computed shadow and store it back
	color.rgb *= exp( -opticalDepthSum * 0.1f ) + 0.1f;

	// colored lights, valid
	// color.rgb *= vec3( 0.675f, 0.5f, 0.1f ) * ( exp( -opticalDepthSum * 0.1f ) + 0.1f );

	imageStore( dataCacheBuffer, writeLoc, color );
}
