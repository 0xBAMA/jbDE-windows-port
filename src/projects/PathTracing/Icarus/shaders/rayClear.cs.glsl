#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "rayState2.h.glsl"
//=============================================================================================================================
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };
layout( binding = 2, std430 ) buffer intersectionBuffer { intersection_t intersectionScratch[]; };
//=============================================================================================================================
void main () {
	// for every ray, we need to wipe N intersection structs
	for ( int i = 0; i < NUM_INTERSECTORS; i++ ) {
		IntersectionReset( intersectionScratch[ gl_GlobalInvocationID.x * NUM_INTERSECTORS + i ] );
	}
}