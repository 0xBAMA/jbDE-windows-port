#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// physarum buffer (uint)		// used to resolve atomic writes from this shader
layout ( binding = 0, r32ui ) uniform uimage2D atomicImage;

// physarum buffer (float 1)	// used as source information for the agent sense taps
layout ( binding = 1 ) uniform sampler2D floatTex;

struct agentRecord_t {
	float mass;
	float pad;
	float drag;
	float senseDistance;
	float senseAngle;
	float turnAngle;
	float forceAmount; // replacing stepsize
	float depositAmount;
	vec2 position;
	vec2 velocity;
};

#include "random.h"
uniform int wangSeed;

// triggering randomized reset
uniform int resetSeed;
uniform float spread;

agentRecord_t getRandomAgent () {
	// deterministic seeding on RNG
	uint seedCache = seed;
	seed = resetSeed;

	// populate the deterministic portion of the seeding ( seed informs all agents, exactly )
	float offsetValues[ 7 ];
	for ( int i = 0; i < 7; i++ ) {
		offsetValues[ i ] = NormalizedRandomFloat();
	}

	// apply the agent-specific jitter of the parameters... ( non-deterministic, varies by agent )
	seed = seedCache;
	for ( int i = 0; i < 7; i++ ) {
		offsetValues[ i ] += spread * NormalDistributionRand();
	}

	// evaluating the offsets on a known range
	agentRecord_t temp;
	temp.mass = mix( 1.5f, 20.0f, offsetValues[ 0 ] );
	temp.drag = mix( 0.5f, 1.0f, offsetValues[ 1 ] );
	temp.senseDistance = mix( 5.0f, 20.0f, offsetValues[ 2 ] );
	temp.senseAngle = mix( 0.0f, tau, offsetValues[ 3 ] );
	temp.turnAngle = mix( 0.0f, tau, offsetValues[ 4 ] );
	temp.forceAmount	= mix( 0.1f, 2.0f, offsetValues[ 5 ] );
	temp.depositAmount	= mix( 10.0f, 1000.0f, offsetValues[ 6 ] );

	// note that position and velocity needs to be uniformly random in all cases
	temp.position = vec2( imageSize( atomicImage ).xy ) * vec2( NormalizedRandomFloat(), NormalizedRandomFloat() );
	temp.velocity = normalize( RandomInUnitDisk() );
	return temp;
}

uniform int numAgents;
layout( binding = 0, std430 ) buffer agentData {
	agentRecord_t data[];
};

// takes argument in radians
vec2 rotate ( const vec2 v, const float a ) {
	const float s = sin( a );
	const float c = cos( a );
	const mat2 m = mat2( c, -s, s, c );
	return m * v;
}

vec2 wrap ( vec2 pos ) {
	const ivec2 iS = imageSize( atomicImage ).xy;
	if ( pos.x >= iS.x ) pos.x -= iS.x;
	if ( pos.x < 0.0f ) pos.x += iS.x;
	if ( pos.y >= iS.y ) pos.y -= iS.y;
	if ( pos.y < 0.0f ) pos.y += iS.y;
	return pos;
}

float pheremone ( vec2 pos ) {
	// need to remap to texturespace... consider adding a jitter here as another agent parameter
	pos = pos / textureSize( floatTex, 0 ).xy;
	return texture( floatTex, pos.xy ).r;
}

vec2 randomInUnitDisk () {
	return RandomUnitVector().xy;
//	return rnd_disc_cauchy();
}

void main () {
	const uvec3 globalDims = gl_NumWorkGroups * gl_WorkGroupSize;
	const int index = int(
		gl_GlobalInvocationID.z * globalDims.x * globalDims.y +
		gl_GlobalInvocationID.y * globalDims.x +
		gl_GlobalInvocationID.x );

	if ( index < numAgents ) {
		// loading the agent record
		agentRecord_t agent = data[ index ];

		// init rng
		seed = wangSeed + 69 * index;

		// user wants to reset the agent
		if ( resetSeed != 0 ) {
//			data[ index ] == getRandomAgent();
			agentRecord_t temp = getRandomAgent();
			data[ index ].mass = temp.mass;
			data[ index ].pad = temp.pad;
			data[ index ].drag = temp.drag;
			data[ index ].senseDistance = temp.senseDistance;
			data[ index ].senseAngle = temp.senseAngle;
			data[ index ].turnAngle = temp.turnAngle;
			data[ index ].forceAmount = temp.forceAmount;
			data[ index ].depositAmount = temp.depositAmount;
			data[ index ].position = temp.position;
			data[ index ].velocity = temp.velocity;
		} else {
		// do the regular update
			// sense taps...
			const vec2 avDir			= normalize( agent.velocity );
			const vec2 rightVec			= agent.senseDistance * rotate( avDir, -agent.senseAngle );
			const vec2 middleVec		= agent.senseDistance * avDir;
			const vec2 leftVec			= agent.senseDistance * rotate( avDir,  agent.senseAngle );
			const float rightSample		= pheremone( agent.position + rightVec );
			const float middleSample	= pheremone( agent.position + middleVec );
			const float leftSample		= pheremone( agent.position + leftVec );

			// make a decision on whether to turn left, right, go straight, or a random direction
				// this can be generalized and simplified, as some sort of weighted sum thing - will bear revisiting
			vec2 impulseVector = middleVec;
			if ( middleSample > leftSample && middleSample > rightSample ) {
				// just retain the existing direction
			} else if ( middleSample < leftSample && middleSample < rightSample ) { // turn a random direction
				impulseVector = randomInUnitDisk();
			} else if ( rightSample > middleSample && middleSample > leftSample ) { // turn right (positive)
				impulseVector = rotate( middleVec, agent.turnAngle );
			} else if ( leftSample > middleSample && middleSample > rightSample ) { // turn left (negative)
				impulseVector = rotate( middleVec, -agent.turnAngle );
			}

			// apply impulse to an object of known mass + store back to SSBO
			vec2 acceleration = impulseVector * agent.forceAmount / agent.mass;	// get the resulting acceleration
			data[ index ].velocity = agent.velocity = agent.drag * agent.velocity + acceleration;					// compute the new velocity
			data[ index ].position = agent.position = wrap( agent.position + agent.velocity );				// get the new position

			// deposit
			imageAtomicAdd( atomicImage, ivec2( agent.position ), uint( agent.depositAmount ) );
		}
	}
}
