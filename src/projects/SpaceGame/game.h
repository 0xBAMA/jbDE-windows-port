#include "../../engine/includes.h"

#define HIGH 0
#define MEDIUM 1
#define LOW 2
struct logEvent {
	uint64_t timestamp; // millisecond-level accuracy from SDL_GetTicks()
	int priority = LOW; // currently only showing HIGH priority on the readout
	string message = string( "Error: No message" ); // keep this short... should not overflow one line
	string additionalData = string( "N/A" ); // e.g. more extensive YAML description of an event, etc
	ivec3 color = WHITE;
};
static deque< logEvent > logEvents;
void submitEvent( logEvent lE ) {
	logEvents.push_front( lE );
}
logEvent logLowPriority ( string s, ivec3 color = GREY_D ) {
	logEvent l;
	l.timestamp = SDL_GetTicks();
	l.priority = LOW;
	l.message = s;
	l.color = color;
	submitEvent( l );
	return l;
}
logEvent logMediumPriority ( string s, ivec3 color = WHITE ) {
	logEvent l;
	l.timestamp = SDL_GetTicks();
	l.priority = MEDIUM;
	l.message = s;
	l.color = color;
	submitEvent( l );
	return l;
}
logEvent logHighPriority ( string s, ivec3 color = RED ) {
	logEvent l;
	l.timestamp = SDL_GetTicks();
	l.priority = HIGH;
	l.message = fixedWidthTimeString() + ": " + s; // add current time
	l.color = color;
	submitEvent( l );
	return l;
}

void DrawMenuBasics ( layerManager &textRenderer, textureManager_t &textureManager ) {
	textRenderer.Clear();
	textRenderer.layers[ 0 ].DrawDoubleFrame( uvec2( 0, textRenderer.numBinsHeight - 1 ), uvec2( textRenderer.numBinsWidth - 1, 0 ), GOLD );
	string s( "[SpaceGame]" );
	textRenderer.layers[ 0 ].WriteString( glm::uvec2( textRenderer.layers[ 1 ].width - s.length() - 2, 0 ), glm::uvec2( textRenderer.layers[ 1 ].width, 0 ), s, WHITE );
	textRenderer.Draw( textureManager.Get( "Display Texture" ) );
}

void DrawBlockMenu ( string label, layerManager &textRenderer, textureManager_t &textureManager ) {
	// solid color background
	textRenderer.Clear();
	textRenderer.layers[ 0 ].DrawRectConstant( uvec2( 5, 3 ), uvec2( textRenderer.numBinsWidth - 6, textRenderer.numBinsHeight - 4 ), cChar( GREY_DD, FILL_100 ) );
	textRenderer.Draw( textureManager.Get( "Display Texture" ) );
	// frame and top label
	textRenderer.Clear();
	textRenderer.layers[ 0 ].DrawDoubleFrame( uvec2( 5, textRenderer.numBinsHeight - 4 ), uvec2( textRenderer.numBinsWidth - 6, 3 ), GOLD );
	textRenderer.layers[ 0 ].WriteString( uvec2( 8, textRenderer.numBinsHeight - 4 ), uvec2( textRenderer.layers[ 1 ].width, textRenderer.numBinsHeight - 5 ), label, WHITE );
	textRenderer.Draw( textureManager.Get( "Display Texture" ) );
}
struct spaceshipStats {
public:
	// maximum speeds ( forward and back )
	float maxSpeedForward = 2.0f;
	float maxSpeedBackward = 0.2f;

	// turn rate
	float turnRate = 0.003f;

	// acceleration rate
	float accelerationRate = 0.001f;

	// deceleration rate
	float decelerationRate = 0.0005f;
};

class spaceshipController {
public:

	spaceshipController () {

		logHighPriority( "Ship Spawned" );
	}

	float angle = 0.0f;
	float velocity = 0.0f;

	vec2 position = vec2( 0.0f );

	spaceshipStats stats;

	void Update ( inputHandler_t &input ) {
		// solve for deltaT
		const float deltaT = clamp( input.millisecondsSinceLastUpdate(), 1.0f, 100.0f );

	// consider user inputs
		// rotation
		if ( input.getState( KEY_A ) ) { angle += stats.turnRate * deltaT * input.getState_soft( KEY_A ); }
		if ( input.getState( KEY_D ) ) { angle -= stats.turnRate * deltaT * input.getState_soft( KEY_D ); }

		// clamping angle
		if ( angle < 0.0f ) { angle += tau; }
		if ( angle > tau ) { angle -= tau; }

		// acceleration/deceleration
		if ( input.getState( KEY_W ) ) { velocity += stats.accelerationRate * deltaT * input.getState_soft( KEY_W ); logHighPriority( "Ship Accelerating" ); } // todo: YAML descriptions
		if ( input.getState( KEY_S ) ) { velocity -= stats.decelerationRate * deltaT * input.getState_soft( KEY_S ); logHighPriority( "Ship Decelerating" ); }

		// clamping large magnitude velocity
		velocity = glm::clamp( velocity, -stats.maxSpeedBackward, stats.maxSpeedForward );

		// apply velocity times deltaT to get new position
		position += deltaT * GetVelocityVector();
	}

	vec2 GetPositionVector() {
		return position;
	}

	vec2 GetVelocityVector() {
		return glm::mat2(
			cos( angle ), -sin( angle ),
			sin( angle ), cos( angle )
		) * vec2( velocity, 0.0f );
	}

};

class universeController {
public:
	ivec2 sectorID = ivec2( 10 );

	spaceshipController ship;

};
