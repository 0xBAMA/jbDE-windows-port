#include "../../engine/includes.h"

// how many types of resources exist in the sim
#define NUM_RESOURCES	16
#define NUM_WORKERS		16
#define NUM_MARKETS		2
#define NUM_SOURCES		16

// general purpose 0..1 rng
rng gen = rng( 0.0f, 1.0f );

// a worker is a finite state machine
// it has a position in space, a target location, a ledger of held resources, and a state
enum workerState_t {
	INITIAL = 0,
	EATING,
	WORKING,
	DOING_BUSINESS,
	MOVING_TOWARDS_WORK,
	MOVING_TOWARDS_MARKET,
	ARRIVED_AT_TARGET,

	TASK_COMPLETE // can have some visual indication, flash brighter for 1 frame or something, TASK->TASK_COMPLETE->INITIAL
	// ...
};

vec3 getRandomPositionInUniverse () {
	return vec3( 1000.0f * ( gen() - 0.5f ), 1000.0f * ( gen() - 0.5f ), 1000.0f * ( gen() - 0.5f ) );
}

struct worker_t {
	worker_t () {
		// generate an initial position...
		position = getRandomPositionInUniverse();
	}

	workerState_t state = INITIAL;

	// tbd
	float hunger = 0.0f;
	float money = 0.0f;
	float speed = 1.0f; // how quickly do I move

	vec3 position; // where am I
	vec3 targetPosition; // where am I going

	float ledger[ NUM_RESOURCES ] = { 0.0f };

	void getResources ( float ledgerIncrement[ NUM_RESOURCES ], float scalar ) {
		for ( int i = 0; i < NUM_RESOURCES; i++ ) {
			ledger[ i ] += ledgerIncrement[ i ] * scalar;
		}
	}

	void moveTowardsTargetPosition() {
		// may need to update state, if position is reached
		vec3 offset = targetPosition - position;
		float distance = length( offset );
		vec3 normalized = normalize( offset );

		float amt = gen();
		if ( ( speed * amt ) < distance ) {
			position += speed * amt * normalized;
		} else {
			// overshoot, change state
			position = targetPosition;
			state = ARRIVED_AT_TARGET;
		}
	}
};

// this is a location where resources can be taken from
struct source_t {
	source_t () {
		for ( int i = 0; i < NUM_RESOURCES; i++ ) {
			dropAmounts[ i ] = gen();
		}
		amountLeft = 10000.0f * gen();
		position = getRandomPositionInUniverse();
	}

	float gather () {
		float amount = gen() * gen();
		if ( amountLeft > 0.0f ) {
			amountLeft -= amount;
		} else { // not enough left
			amount = 0.0f;
		}
		return amount;
	}

	vec3 position;
	// amount of each resource to give
	float dropAmounts[ NUM_RESOURCES ];
	// amount of "stuff" left
	float amountLeft;
};

struct market_t {
	market_t () {
		// initial location
		position = getRandomPositionInUniverse();

		for ( int i = 0; i < NUM_RESOURCES; i++ ) {
			// some amount of initial resources
			ledger[ i ] = 100.0f * gen();
			// some initial price for each resource
			ledger[ i ] = 100.0f * gen();
		}
	}
	vec3 position;
	float ledger[ NUM_RESOURCES ] = { 0.0f };
	float prices[ NUM_RESOURCES ] = { 0.0f };

	// functions to transact with the market
		// todo
};

struct spaceGameData_t {

	worker_t workers[ NUM_WORKERS ];
	market_t markets[ NUM_MARKETS ];
	source_t sources[ NUM_SOURCES ];

	spaceGameData_t () {
		// initialization
			// place markets
			// place sources
			// place workers
	}

	void update () {
		for ( int i = 0; i < NUM_WORKERS; i++ ) {
			// switch on worker state
			switch ( workers[ i ].state ) {
				case INITIAL:
					// figure out a goal (start here, and return each time a task is completed)

					break;

				// this dude is eating
				case EATING:
					// todo
					break;

				// this dude is at a source, working
				case WORKING:
					// do some work
					for ( int i = 0; i < NUM_SOURCES; i++ ) {
						if ( distance( workers[ i ].position, sources[ i ].position ) < 5.0f ) {
							// don't know what the stop criterion is yet
							workers[ i ].getResources( sources[ i ].dropAmounts, sources[ i ].gather() );
						}
					}
					break;

				// this dude is at a market
				case DOING_BUSINESS:
					// todo
					break;

				// this dude is on the move
				case MOVING_TOWARDS_WORK:
				case MOVING_TOWARDS_MARKET:
					workers[ i ].moveTowardsTargetPosition();
					break;

				// woo hoo
				case TASK_COMPLETE: // get a new goal next update
					workers[ i ].state = INITIAL;
					break;

				default:
					break;
			}
		}
	}
};