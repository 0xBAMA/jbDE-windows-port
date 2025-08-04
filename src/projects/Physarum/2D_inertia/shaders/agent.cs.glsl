#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// physarum buffer (uint)		// used to resolve atomic writes from this shader
layout ( binding = 0, r32ui ) uniform uimage2D atomicImage;

// physarum buffer (float 1)	// used as source information for the agent sense taps
layout ( binding = 1, r32f ) uniform image2D floatImage;

struct agentRecord_t {
	float mass;
	float senseDistance;
	float senseAngle;
	float turnAngle;
	float forceAmount; // replacing stepsize
	float depositAmount;
	vec2 position;
	vec2 velocity;
};

layout( binding = 0, std430 ) buffer agentData {
	agentRecord_t data[];
};

void main () {
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;
	agentRecord_t parameters = data[ index ];

	// sense taps...

	// movement decision

	// apply impulse

	// movement update

	//

}
