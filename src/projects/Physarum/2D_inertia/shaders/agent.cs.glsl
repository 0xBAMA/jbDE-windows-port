#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// physarum buffer (uint)		// used to resolve atomic writes from this shader
// physarum buffer (float 1)	// used as source information for the agent sense taps

struct agentRecord_t {
	float mass;
	float senseDistance;
	float senseAngle;
	float turnAngle;
	float forceAmount;
	float depositAmount;
	vec2 position;
	vec2 velocity;
};

layout( binding = 0, std430 ) buffer agentData {
	agentRecord_t data[];
};

void main () {

}
