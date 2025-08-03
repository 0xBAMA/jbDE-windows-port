#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// physarum buffer (float 1) - mip 0 is populated by the sim, and we propagate max value up the mipchain to texel (0,0) in mip N
	// once the process completes, you can sample this max value during the draw shader, to inform color scaling

void main () {
	// load the four pixels in mip N

	// store the maximum value back in mip N+1

}
