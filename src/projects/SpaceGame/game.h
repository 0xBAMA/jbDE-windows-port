#include "../../engine/includes.h"

//=============================================================================
//==== std::chrono Wrapper - Simplified Tick() / Tock() Interface =============
//=============================================================================

// no nesting, but makes for a very simple interface
	// could probably do something stack based, have Tick() push and Tock() pop
#define NOW std::chrono::steady_clock::now()
#define TIMECAST(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()/1000.0f
static std::chrono::time_point<std::chrono::steady_clock> tStart_spacegame = std::chrono::steady_clock::now();
static std::chrono::time_point<std::chrono::steady_clock> tCurrent_spacegame = std::chrono::steady_clock::now();
void Tick() {
	tCurrent_spacegame = NOW;
}

float Tock() {
	return TIMECAST( NOW - tCurrent_spacegame );
}

float TotalTime() {
	return TIMECAST( NOW - tStart_spacegame );
}

#undef NOW
#undef TIMECAST

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
	float maxSpeedForward = 0.5f;
	float maxSpeedBackward = 0.05f;

	// turn rate
	float turnRate = 0.003f;

	// acceleration rate
	float accelerationRate = 0.0001f;

	// deceleration rate
	float decelerationRate = 0.00007f;

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
	vec2 location = vec2( 0.0f );
	vec2 scale = vec2( 1.0f, 0.618f );
	float rotation = 0.0f;

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
	// a list of images of the ships, for use on the CPU
	vector < Image_4U > entitySprites;

	// will become more relevant later
	ivec2 sectorID = ivec2( 10 );

	// for player control
	spaceshipController ship;

	// the runtime list of entities, with attached logic
	vector< entity > entityList;

// then prepared data for rendering

// Object Atlas, in 2 pieces
	// Atlas Texture
	GLuint atlasTexture;
	// Atlas Texture SSBO
	GLuint atlasTextureSSBO;

	// BVH
	tinybvh::BVH8_CWBVH entityBVH;

	// GPU buffers associated with the BVH
	GLuint cwbvhNodesDataBuffer, cwbvhTrisDataBuffer, triangleDataBuffer;

	universeController () {
		// create the list of ships
			// get the Image_4U for the base entity on the CPU

		entityList.resize( 100 );
		entityList[ 0 ].location = ship.position;
		entityList[ 0 ].rotation = ship.angle;

		// some dummy positions
		rng position( -500.0f, 500.0f );
		rng rotation( 0, tau );
		for ( int i = 0; i < 100; i++ ) {
			entityList[ i ].location = vec2( position(), position() );
			entityList[ i ].rotation = rotation();
		}
	}

	void init () {
		// create the buffers for the BVH stuff
		glCreateBuffers( 1, &cwbvhNodesDataBuffer );
		glCreateBuffers( 1, &cwbvhTrisDataBuffer );
		glCreateBuffers( 1, &triangleDataBuffer );

		// and the buffer for the atlas
		glCreateBuffers( 1, &atlasTextureSSBO );
	}

	void updateAtlasTexture () {
	// Initial data
		// go through the list of entities to get their dimensions...
		// rectangle packing
			// this will scale up and allow for per-entity atlas entries, so we can pre-composite ships with all their equipment
		// create the atlas texture, keep the rectangle positioning information

	// GPU data update:
		// prepare SSBO for the altas with LUT, int texture ID -> basePoint and fractional size (base uv and size, since geo will have texcoords 0..1)
		// texture data with the atlas itself

	}

	void updateBVH () {
		// get the bounding box information from the entities
		vector< tinybvh::bvhvec4 > triangleDataNoTexcoords;
		vector< vec3 > triangleDataWithTexcoords;

		triangleDataNoTexcoords.reserve( 36 * entityList.size() );
		triangleDataWithTexcoords.reserve( 2 * 36 * entityList.size() );

		for ( auto& e : entityList ) {
			bboxData bbox = e.getBBoxPoints();

			// 12 triangles, 3 points each - triangles only for basic traversal, texcoords needed for alpha test
			for ( int i = 0; i < 12; i++ ) {
				int idx = i * 3;
				for ( int j = 0; j < 3; j++ ) {
					tinybvh::bvhvec4 p;
					p.x = bbox.points[ idx + j ].x;
					p.y = bbox.points[ idx + j ].y;
					p.z = bbox.points[ idx + j ].z;
					p.w = 1.0;
					triangleDataNoTexcoords.push_back( p );
					triangleDataWithTexcoords.push_back( bbox.points[ idx + j ] );
					triangleDataWithTexcoords.push_back( bbox.texcoords[ idx + j ] );
				}
			}
		}

		Tick();
		entityBVH.BuildHQ( &triangleDataNoTexcoords[ 0 ], triangleDataNoTexcoords.size() / 3 );
		float msTakenBVH = Tock();
		cout << endl << "BVH built in " << msTakenBVH / 1000.0f << "s\n";

		Tick();
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhNodesDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, entityBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) entityBVH.bvh8Data, GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, cwbvhNodesDataBuffer );
		glObjectLabel( GL_BUFFER, cwbvhNodesDataBuffer, -1, string( "CWBVH Node Data" ).c_str() );
		cout << "CWBVH8 Node Data is " << GetWithThousandsSeparator( entityBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhTrisDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, entityBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) entityBVH.bvh8Tris, GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, cwbvhTrisDataBuffer );
		glObjectLabel( GL_BUFFER, cwbvhTrisDataBuffer, -1, string( "CWBVH Tri Data" ).c_str() );
		cout << "CWBVH8 Triangle Data is " << GetWithThousandsSeparator( entityBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, triangleDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, triangleDataWithTexcoords.size() * sizeof( vec3 ), ( GLvoid* ) &triangleDataWithTexcoords[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, triangleDataBuffer );
		glObjectLabel( GL_BUFFER, triangleDataBuffer, -1, string( "Triangle Data With Texcoords" ).c_str() );
		cout << "Triangle Test Data is " << GetWithThousandsSeparator( triangleDataWithTexcoords.size() * sizeof( vec3 ) ) << " bytes" << endl;

		float msTakenBufferBVH = Tock();
		cout << endl << "BVH passed to GPU in " << msTakenBufferBVH / 1000.0f << "s\n";
	}

	void update () {
	// primary update work
		// player is entityList[ 0 ]
		entityList[ 0 ].location = ship.position;
		entityList[ 0 ].rotation = ship.angle;
		// call everyone's update() function

	// is there a new entity in play? we need to rebuild the atlas
		// rebuild atlas + index SSBO
		// entities leaving, not as important to evict from the atlas

	// because of the way we're using it... we need to update the BVH and upload it to the GPU every frame

	// cleanup work
		// has the player left the sector? we have work to do
		// the 

	}

};
