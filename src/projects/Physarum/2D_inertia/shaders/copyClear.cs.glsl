#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// physarum buffer (uint)		// source data for this pass... used to resolve the atomic writes from the sim agents
layout ( binding = 0, r32ui ) uniform uimage2D atomicImage;

// physarum buffer (float 1)	// will have mipchain with autoexposure information, as well. Contains prepared sim data for present
layout ( binding = 1, r32f ) uniform image2D floatImage;

void main () {
	// load the atomic write result

	// add to the value in the float buffer

	// write back a zero value to the uint buffer, to prepare for next frame

}
