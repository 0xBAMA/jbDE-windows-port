#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// physarum buffer (float 1)	// will have mipchain with autoexposure information, as well. Contains prepared sim data for present

uniform float time;

void main () {

}
