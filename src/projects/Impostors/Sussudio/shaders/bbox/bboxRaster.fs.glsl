#version 430

#define CAPSULE		0
#define CAPPEDCONE	1
#define ROUNDEDBOX	2
#define ELLIPSOID	3

// ===================================================================================================
// input to the bounds stage, used again by fragment raytracing
// ===================================================================================================
uniform mat4 viewTransform;
// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 0, std430 ) buffer parametersBuffer {
	parameters_t parametersList[];
};

// ===================================================================================================
// output from the bounds stage
// ===================================================================================================
layout( binding = 1, std430 ) buffer transformsBuffer {
	mat4 transforms[];
};

// vertex shader needs to output a primitive ID, to pull parameters from parametersList[]
	// this is used to get the raytraced intersection

in flat uint vofiIndex;
in vec3 vofiPosition;
out vec4 outColor;

uniform vec3 eyePosition;	// location of the viewer... what about ortho case? I think the atlas is ortho
uniform float numPrimitives;

void main () {
	// going to do pixel-stratified jitter for now because I think it resolves nicer...
		// if I do decide to do more TAA stuff in the future, I'll want to make it view-stratified
	
	// I think the thing to do here is to look at the screenspace derivatives, since I'll have them
		// this will be on the interpolated vofiPosition, so we can get that to jitter the far end
		// of the view ray, across the pixel footprint

		// it might be a good thing to be able to do this multiple times and get an alpha result...
			// view stratified jitter makes the occlusion problem simpler, too, I think

	parameters_t parameters = parametersList[ vofiIndex ];

	// switch on contained primitive...
	switch( int( parameters[ 0 ] ) ) {
	case CAPSULE:
		break;

	case CAPPEDCONE:
		break;

	case ROUNDEDBOX:
		break;

	case ELLIPSOID:
		break;

	default:
		break;
	}

	// write deferred surface results (depth, id, normal... what else?)

	outColor = vec4( 1.0f, 0.0f, 0.0f, 1.0f );
}