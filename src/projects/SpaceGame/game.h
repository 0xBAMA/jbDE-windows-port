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

void DrawInfoLog ( layerManager &textRenderer, textureManager_t &textureManager ) {
// drawing a list of strings
	// a low density background
	/*
	textRenderer.Clear();
	textRenderer.layers[ 0 ].DrawRectConstant( uvec2( 10, 3 ), uvec2( 24, 8 ), cChar( GREY_DD, FILL_25 ) );
	textRenderer.Draw( textureManager.Get( "Display Texture" ) );
	*/

	// the entries themselves
	textRenderer.Clear();
	int offset = 6;
	for ( auto& entry : logEvents ) {
		if ( entry.priority == HIGH ) {
			textRenderer.layers[ 0 ].WriteString( uvec2( 1, offset ), uvec2( 1 + entry.message.length() + 1, 1 + offset ), entry.message, entry.color );
			if ( !( offset-- - 1 ) ) {
				break;
			}
		}
	}
	textRenderer.Draw( textureManager.Get( "Display Texture" ) );
}

struct spaceshipStats {
public:
	// maximum speeds ( forward and back )
	float maxSpeedForward = 2.5f;
	float maxSpeedBackward = 0.2f;

	// turn rate
	float turnRate = 0.003f;

	// acceleration rate
	float accelerationRate = 0.001f;

	// deceleration rate
	float decelerationRate = 0.0005f;

	// size of the ship
	float size = 0.1f;
};

class spaceshipController {
public:

	spaceshipController () {
		logHighPriority( "Player Ship Spawned" );
	}

	float angle = 0.0f;
	float velocity = 0.0001f;

	vec2 position = vec2( 0.0f );

	spaceshipStats stats;

	void Update ( inputHandler_t &input ) {
		// solve for deltaT
		const float deltaT = clamp( input.millisecondsSinceLastUpdate(), 1.0f, 100.0f );

		// velocity slowly decays
		velocity *= 0.99f;

	// consider user inputs - softstate usage should go through a biasGain transform to give characteristic behaviors for different ships/equipment
		// rotation
		if ( input.getState( KEY_A ) ) { angle -= stats.turnRate * deltaT * input.getState_soft( KEY_A ); }
		if ( input.getState( KEY_D ) ) { angle += stats.turnRate * deltaT * input.getState_soft( KEY_D ); }

		// clamping angle
		if ( angle < 0.0f ) { angle += tau; }
		if ( angle > tau ) { angle -= tau; }

		// acceleration/deceleration
		if ( input.getState( KEY_W ) ) { velocity += stats.accelerationRate * deltaT * input.getState_soft( KEY_W ); logLowPriority( "Ship Accelerating" ); } // todo: YAML descriptions
		if ( input.getState( KEY_S ) ) { velocity -= stats.decelerationRate * deltaT * input.getState_soft( KEY_S ); logLowPriority( "Ship Decelerating" ); }

		// clamping large magnitude velocity
		velocity = glm::clamp( velocity, -stats.maxSpeedBackward, stats.maxSpeedForward );

		// apply velocity times deltaT to get new position
		position -= deltaT * GetVelocityVector();
	}

	void turn ( float amount ) {
		angle += amount;
	}

	void accelerate ( float amount ) {
		velocity += amount;
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

constexpr size_t numPointsBBox = 36;
struct bboxData {
	vec3 points[ numPointsBBox ];
	vec3 texcoords[ numPointsBBox ];

	vec3 CubeVert( int idx ) {
	// from shader cubeVerts.h... still want to figure that LUT out + rederive the square one
		// big const array is yucky - ALU LUT impl notes from vassvik
			// https://twitter.com/vassvik/status/1730961936794161579
			// https://twitter.com/vassvik/status/1730965355663655299
		const vec3 pointsList[ numPointsBBox ] = {
			vec3( -1.0f,-1.0f,-1.0f ),
			vec3( -1.0f,-1.0f, 1.0f ),
			vec3( -1.0f, 1.0f, 1.0f ),
			vec3( 1.0f, 1.0f,-1.0f ),
			vec3( -1.0f,-1.0f,-1.0f ),
			vec3( -1.0f, 1.0f,-1.0f ),
			vec3( 1.0f,-1.0f, 1.0f ),
			vec3( -1.0f,-1.0f,-1.0f ),
			vec3( 1.0f,-1.0f,-1.0f ),
			vec3( 1.0f, 1.0f,-1.0f ),
			vec3( 1.0f,-1.0f,-1.0f ),
			vec3( -1.0f,-1.0f,-1.0f ),
			vec3( -1.0f,-1.0f,-1.0f ),
			vec3( -1.0f, 1.0f, 1.0f ),
			vec3( -1.0f, 1.0f,-1.0f ),
			vec3( 1.0f,-1.0f, 1.0f ),
			vec3( -1.0f,-1.0f, 1.0f ),
			vec3( -1.0f,-1.0f,-1.0f ),
			vec3( -1.0f, 1.0f, 1.0f ),
			vec3( -1.0f,-1.0f, 1.0f ),
			vec3( 1.0f,-1.0f, 1.0f ),
			vec3( 1.0f, 1.0f, 1.0f ),
			vec3( 1.0f,-1.0f,-1.0f ),
			vec3( 1.0f, 1.0f,-1.0f ),
			vec3( 1.0f,-1.0f,-1.0f ),
			vec3( 1.0f, 1.0f, 1.0f ),
			vec3( 1.0f,-1.0f, 1.0f ),
			vec3( 1.0f, 1.0f, 1.0f ),
			vec3( 1.0f, 1.0f,-1.0f ),
			vec3( -1.0f, 1.0f,-1.0f ),
			vec3( 1.0f, 1.0f, 1.0f ),
			vec3( -1.0f, 1.0f,-1.0f ),
			vec3( -1.0f, 1.0f, 1.0f ),
			vec3( 1.0f, 1.0f, 1.0f ),
			vec3( -1.0f, 1.0f, 1.0f ),
			vec3( 1.0f,-1.0f, 1.0f )
		};

		return pointsList[ idx ];
	}

	bboxData ( int texID ) {
		// create the initial data for points and texcoords
		for ( int i = 0; i < numPointsBBox; i++ ) {
			points[ i ] = CubeVert( i );
			texcoords[ i ] = glm::clamp( CubeVert( i ), 0.0f, 1.0f );
			texcoords[ i ].z = texID;
		}
	}
};

// high level ID
#define OBJECT 0
#define FRIEND 1
#define FOE 2

// FSM logic states
#define INACTIVE -1
#define INITIAL 0
// ...

// forward declare for entity access to sector info
class universeController;

struct entity {
	// since this is kind of a tagged union... some extra data
	int type = OBJECT;
	float shipHeading = 0.0f;
	float shipSpeed = 0.001f;

	// todo
	int FSMState = INITIAL;

	// keeping an Image_4U here might be a good solution to unique entity appearances?
		// this allows for ongoing update of the model
	Image_4U entityImage;
		// note that updates will require that you revert to the base model, so any time this updates, you need to fetch the base image for your ship from the universe controller
	string baseEntityImageLabel; // and this should come in during object construction

	// so that e.g. ships can query what other objects are in the sector
	universeController *universe;

	// for the display primitive
	vec2 location;
	vec2 scale;
	float rotation;

	// kept in the third coordinate of the texcoord - we need to know this when we create the entity
		// this indexes into SSBO with atlased texture info (1 index -> texture info)
	int textureIndex;

	bboxData getBBoxPoints () {
		// initial points
		bboxData points( textureIndex );

		// scaling the bbox - somewhat specialized... smaller ships are taller, is the plan for handling occlusion
		for ( auto& p : points.points ) {
			// apply scaling
			p = ( glm::scale( vec3( scale.x, scale.y, clamp( 1.0f / ( ( scale.x + scale.y ) / 2.0f ), 0.0f, 100.0f ) ) ) * vec4( p, 1.0f ) ).xyz();

			// apply rotation
			p = ( glm::angleAxis( rotation, vec3( 0.0f, 0.0f, 1.0f ) ) * vec4( p, 1.0f ) ).xyz();

			// apply translation
			p = ( glm::translate( vec3( location.x, location.y, 0.0f ) ) * vec4( p, 1.0f ) ).xyz();
		}

		return points;
	}

	void update () {
		switch ( type ) {
		case OBJECT: // objects do nothing
		break;

		case FRIEND: // todo - ai steering logic ( seeking FOE ships )
		break;

		case FOE: // todo - ai steering logic ( seeking FRIENDLY ships )
		break;

		default: // no
		break;
		}
	}
};
class universeController {
public:
	ivec2 sectorID = ivec2( 10 );

	spaceshipController ship;

};
