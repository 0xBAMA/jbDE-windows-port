#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// ===================================================================================================
// input to the bounds stage
// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 1, std430 ) buffer parametersBuffer {
	parameters_t parametersList[];
};
// ===================================================================================================
uniform float time;
// ===================================================================================================
void main () {

	// getting this object's parameters
	uint index = gl_GlobalInvocationID.x;
	parameters_t parameters = parametersList[ index ];

	// wiggle it around - this is very half assed, but definite proof of concept here
	// parameters.data[ 1 ] += 0.001f * sin( time );
	// parameters.data[ 2 ] += 0.0003f * cos( time * 0.8f );
	// parameters.data[ 3 ] += 0.0001f * sin( time * 0.6f );

	// write the result to the other SSBO
	// parametersList[ index ] = parameters;
}