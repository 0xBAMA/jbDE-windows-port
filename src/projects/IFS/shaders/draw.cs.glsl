#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

layout( binding = 2, r32ui ) uniform uimage2D ifsAccumulatorR;
layout( binding = 3, r32ui ) uniform uimage2D ifsAccumulatorG;
layout( binding = 4, r32ui ) uniform uimage2D ifsAccumulatorB;

layout( binding = 0, std430 ) buffer maxBuffer { uint maxCount; };

#include "mathUtils.h"

uniform float brightness;
uniform float brightnessPower;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	float maxCountF = float( maxCount );
	vec3 loadedColor = vec3(
		float( imageLoad( ifsAccumulatorR, writeLoc ).r ) / maxCountF,
		float( imageLoad( ifsAccumulatorG, writeLoc ).r ) / maxCountF,
		float( imageLoad( ifsAccumulatorB, writeLoc ).r ) / maxCountF
	);

	vec3 col = pow( brightness * loadedColor, vec3( brightnessPower ) );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
