#pragma once
#ifndef GAME_H
#define GAME_H

#include "../../engine/includes.h"
#include "lineDraw.h"

#include <mutex>
#include "stb_rect_pack.h"

//=============================================================================
//==== std::chrono Wrapper - Simplified Tick() / Tock() Interface =============
//=============================================================================
// no nesting, but makes for a very simple interface
	// could probably do something stack based, have Tick() push and Tock() pop
#define NOW std::chrono::steady_clock::now()
#define TIMECAST(x) (std::chrono::duration_cast<std::chrono::microseconds>(x).count()/1000.0f)
static std::chrono::time_point<std::chrono::steady_clock> tStart_spacegame = std::chrono::steady_clock::now();
static std::chrono::time_point<std::chrono::steady_clock> tCurrent_spacegame = std::chrono::steady_clock::now();
inline void Tick()			{ tCurrent_spacegame = NOW; }
inline float Tock()			{ return TIMECAST( NOW - tCurrent_spacegame ); }
inline float TotalTime()	{ return TIMECAST( NOW - tStart_spacegame ); }
#undef NOW
#undef TIMECAST

#define HIGH 0
#define MEDIUM 1
#define LOW 2
struct logEvent {
	uint64_t timestamp = { SDL_GetTicks() }; // millisecond-level accuracy from SDL_GetTicks()
	string message = string( "Error: No message" ); // keep this short... should not overflow one line
	int priority = LOW; // currently only showing HIGH priority on the readout
	string additionalData = string( "N/A" ); // e.g. more extensive YAML description of an event, etc
	ivec3 color = WHITE;

	logEvent () = default;
	logEvent ( const string &s, const ivec3 &color = WHITE ) : message( s ), color( color ) {}
	logEvent ( const string &s, const int &priority, const ivec3 &color = WHITE ) : message( s ), priority( priority ), color( color) {}
};
static deque< logEvent > logEvents;

inline void submitEvent( const logEvent &lE ) {
	logEvents.push_front( lE );
}

inline void logLowPriority ( const string &s, const ivec3 &color = GREY_D ) {
	submitEvent( { s, LOW, color } );
}

inline void logMediumPriority ( const string &s, const ivec3 &color = WHITE ) {
	submitEvent( { s, MEDIUM, color } );
}

inline void logHighPriority ( const string& s, const ivec3 &color = GREY_D ) {
	submitEvent( { fixedWidthTimeString() + ": " + s, HIGH, color } );
}

inline void DrawMenuBasics ( layerManager &textRenderer, textureManager_t &textureManager ) {
	textRenderer.Clear();
	textRenderer.layers[ 0 ].DrawDoubleFrame( uvec2( 0, textRenderer.numBinsHeight - 1 ), uvec2( textRenderer.numBinsWidth - 1, 0 ), GOLD );
	string s( "[SpaceGame]" );
	textRenderer.layers[ 0 ].WriteString( glm::uvec2( textRenderer.layers[ 1 ].width - s.length() - 2, 0 ), glm::uvec2( textRenderer.layers[ 1 ].width, 0 ), s, WHITE );
	textRenderer.Draw( textureManager.Get( "Display Texture" ) );
}

inline void DrawBlockMenu ( const string &label, layerManager &textRenderer, textureManager_t &textureManager ) {
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

inline void DrawInfoLog ( layerManager &textRenderer, textureManager_t &textureManager ) {
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
	float maxSpeedForward = 1.0f;
	float maxSpeedBackward = 0.1f;

	// turn rate
	float turnRate = 0.003f;

	// acceleration rate
	float accelerationRate = 0.0002f;

	// deceleration rate
	float decelerationRate = 0.00007f;

	// how far from the center can the ship go, at max speed
	float maxThrustDisplacement = 50.0f;

	// vector of positions of the engines...
		// this would be pixel locations, that we can translate into worldspace positions...
	// I basically want to use this to draw something (vectors/particles) indicating engine thrust
};

class spaceshipController {
public:

	spaceshipController () {
		logHighPriority( "Player Ship Spawned" );
	}

	float angle = 0.0f;
	float velocity = 0.0001f;
	float sectorSize = 2000.0f;

	vec2 position = vec2( 0.5f );

	spaceshipStats stats;

	void Update ( inputHandler_t &input ) {
		ZoneScoped;
		// solve for deltaT
		const float deltaT = clamp( input.millisecondsSinceLastUpdate(), 1.0f, 100.0f );

		// velocity slowly decays
		velocity *= 0.99f;

	// consider user inputs - softstate usage should go through a biasGain transform to give characteristic behaviors for different ships/equipment
		// rotation - I think this should be more "applying an impulse" than directly operating on the rotation itself... this way it can do first and second order effects
		if ( input.getState( KEY_A ) ) { angle -= stats.turnRate * deltaT * input.getState_soft( KEY_A ); logLowPriority( "Ship Turning Left" ); }
		if ( input.getState( KEY_D ) ) { angle += stats.turnRate * deltaT * input.getState_soft( KEY_D ); logLowPriority( "Ship Turning Right" ); }

		// clamping angle
		if ( angle < 0.0f ) { angle += tau; }
		if ( angle > tau ) { angle -= tau; }

		// acceleration/deceleration
		if ( input.getState( KEY_W ) ) { velocity += stats.accelerationRate * deltaT * input.getState_soft( KEY_W ); logLowPriority( "Ship Accelerating" ); } // todo: YAML descriptions
		if ( input.getState( KEY_S ) ) { velocity -= stats.decelerationRate * deltaT * input.getState_soft( KEY_S ); logLowPriority( "Ship Decelerating" ); }

		// clamping large magnitude velocity
		velocity = glm::clamp( velocity, -stats.maxSpeedBackward, stats.maxSpeedForward );

		// apply velocity times deltaT to get new position - scaled for the 0..1 extents of sector
		position -= deltaT * GetVelocityVector() / ( sectorSize );
	}

	void turn ( const float &amount ) {
		angle += amount;
	}

	void accelerate ( const float &amount ) {
		velocity += amount;
	}

	vec2 GetPositionVector() const {
		return vec2(
			RangeRemap( position.x - floor( position.x ), 0.0f, 1.0f, -sectorSize / 2.0f, sectorSize / 2.0f ),
			RangeRemap( position.y - floor( position.y ), 0.0f, 1.0f, -sectorSize / 2.0f, sectorSize / 2.0f )
		);
	}

	vec2 GetVelocityVector() const {
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

	static vec3 CubeVert( const int &idx ) {
	// from shader cubeVerts.h... still want to figure that LUT out + rederive the square one
		// big const array is yucky - ALU LUT impl notes from vassvik
			// https://twitter.com/vassvik/status/1730961936794161579
			// https://twitter.com/vassvik/status/1730965355663655299
		constexpr vec3 pointsList[ numPointsBBox ] = {
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

	explicit bboxData ( int texID ) {
		ZoneScoped;
		// create the initial data for points and texcoords
		for ( int i = 0; i < numPointsBBox; i++ ) {
			points[ i ] = CubeVert( i );
			texcoords[ i ] = glm::clamp( CubeVert( i ), 0.0f, 1.0f ).yxz();
			texcoords[ i ].z = float( texID );
		}
	}
};

// high-level ID
#define PLAYER		(0)
#define ASTEROID	(1)
#define STATION		(2)
#define FRIEND		(3)
#define FOE			(4)

// FSM logic states
#define INACTIVE	(-1)
#define INITIAL		(0)
// ...

// forward declare for entity access to sector info
class universeController;

struct entity {
	// since this is kind of a tagged union... some extra data
	int type = PLAYER;
	float shipHeading = 0.0f;
	float shipSpeed = 0.0f;

	// todo
	int FSMState = INITIAL;

	// keeping an Image_4U here might be a good solution to unique entity appearances?
		// this allows for ongoing update of the model
	Image_4U entityImage;
		// note that updates will require that you revert to the base model, so any time this updates, you need to fetch the base image for your ship from the universe controller
	int baseEntityImage; // and this should come in during object construction

	// I have an atlas index... I got it during atlas creation
	int atlasIndex = -1;

	// so that e.g. ships can query what other objects are in the sector
	universeController *universe = nullptr;

	// for the display primitive
	vec2 position = vec2( 0.0f );
	vec2 scale = vec2( 1.0f );
	float rotation = 0.0f;
	float sectorSize;

	// constructor for entity
	entity () = default;
	entity ( int type, vec2 location, float rotation, universeController *universeP, vec2 scale = vec2 ( 1.0f ), int indexOfTexture = 1, float sectorSize = 1.0f );

	bboxData getBBoxPoints () const {
		ZoneScoped;
		// initial points
		bboxData points( atlasIndex );

		// scaling the bbox - somewhat specialized... smaller ships are taller, is the plan for handling occlusion
		for ( auto& p : points.points ) {
			// apply scaling, small offset to avoid z fighting
			p = ( glm::scale( vec3( scale.x, scale.y, clamp( 1.0f / ( ( scale.x + scale.y + 0.001f * atlasIndex ) / 2.0f ), 0.001f, 1000.0f ) ) ) * vec4( p, 1.0f ) ).xyz();

			// apply rotation
			p = ( glm::angleAxis( -rotation, vec3( 0.0f, 0.0f, 1.0f ) ) * vec4( p, 1.0f ) ).xyz();

			// apply translation - accounting for the scaling that needs to be applied to the stored location value
			p = ( glm::translate( vec3(
				RangeRemap( position.x, 0.0f, 1.0f, -sectorSize / 2.0f, sectorSize / 2.0f ),
				RangeRemap( position.y, 0.0f, 1.0f, -sectorSize / 2.0f, sectorSize / 2.0f ),
				0.0f ) ) * vec4( p, 1.0f ) ).xyz();
		}

		return points;
	}

	void update () {
		ZoneScoped;
		switch ( type ) {

			// quite a bit of shit to do here

		case FRIEND: // todo - ai steering logic ( seeking FOE ships )
		break;

		case FOE: // todo - ai steering logic ( seeking FRIENDLY ships )
		break;

		default: // no
		break;
		}
	}
};

class AtlasManager;

class universeController {
public:
	// scaling the uv of the display
	float globalZoom = 30.0f;

	float sectorSize = 20000.0f;

	// a list of images of the ships, for use on the CPU
	vector < Image_4U > entitySprites;
	void LoadSprites () {
		ZoneScoped;
		entitySprites.emplace_back( "../src/projects/SpaceGame/station1.png" );	// space station
		entitySprites.emplace_back( "../src/projects/SpaceGame/ship1.png" );		// medium ship
		entitySprites.emplace_back( "../src/projects/SpaceGame/ship2.png" );		// larger ship
		entitySprites.emplace_back( "../src/projects/SpaceGame/asteroid1.png" );	// asteroid
	}
	// managing the atlas
	AtlasManager * atlas;

	// for drawing lines
	LineDrawer lines;

	// will become more relevant later
	ivec2 sectorID = ivec2( 0 );

	// for player control
	spaceshipController ship;

	// the runtime list of entities, with attached logic
	vector< entity > entityList;

	// BVH
	tinybvh::BVH8_CWBVH entityBVH;

	// GPU buffers associated with the BVH
	GLuint cwbvhNodesDataBuffer, cwbvhTrisDataBuffer, triangleDataBuffer;

	universeController () {
		ZoneScoped;
		// create the list of ships
			// get the Image_4U for the base entity on the CPU

		// load the sprites from disk
		LoadSprites();

		// create the sector contents
		handleSectorChange( sectorID );
	}

	void clearSector () {
		ZoneScoped;
		// put the player in the first element, before populating the sector
		entityList.resize( 1 );
		entityList[ 0 ].position = ship.position;
		entityList[ 0 ].rotation = ship.angle;
		entityList[ 0 ].entityImage = entitySprites[ 1 ];
		// entityList[ 0 ].scale = vec2( entitySprites[ 0 ].Width(), entitySprites[ 0 ].Height() );
		entityList[ 0 ].type = PLAYER;
		entityList[ 0 ].sectorSize = sectorSize;
	}

	void spawnSector () {
		ZoneScoped;
		// requires atlas rebuild
		entityListDirty = true;
		vec2 sector = vec2( sectorID );

		// chance to spawn station at the center of the sector
		static rng stationChance( 0.0f, 1.0f );
		// if ( stationChance() < 0.45f ) {
		// if ( stationChance() < 0.99f ) {
			entityList.emplace_back( STATION, sector + vec2 ( 0.5f, 0.5f ), 0.0f, this, 0.02f, 0, sectorSize );
		// }

		rngi countGenerator( 1, 10 );
		const int numFriends = countGenerator();
		const int numFoes = countGenerator();
		const int numAsteroids = countGenerator() * 2 + 5;

		// RNG distributions
		rng friendPosition ( 0.45f, 0.55f ), rotation ( 0.0f, tau );
		rngi foeEdgeSelector ( 0, 3 );
		rng foeEdgePosition ( 0.0f, 1.0f );
		rng asteroidPosition ( 0.0f, 1.0f );

		// Spawn friendly ships - consistent order of parameters
		for ( int i = 0; i < numFriends; ++i ) {
			entityList.emplace_back( FRIEND, sector + vec2( friendPosition(), friendPosition() ), rotation(), this, 0.02f, 1, sectorSize );
		}

		// Spawn enemy ships along the edges of the sector
		for ( int i = 0; i < numFoes; ++i ) {
			const int edge = foeEdgeSelector();
			const float x = ( edge == 2 ) ? 0.01f : ( edge == 3 ) ? 0.99f : foeEdgePosition();
			const float y = ( edge == 0 ) ? 0.99f : ( edge == 1 ) ? 0.01f : foeEdgePosition();
			entityList.emplace_back( FOE, sector + vec2( x, y ), rotation(), this, 0.02f, 2, sectorSize );
		}

		// Spawn asteroids across the entire sector
		for ( int i = 0; i < numAsteroids; ++i ) {
			entityList.emplace_back( ASTEROID, sector + vec2( asteroidPosition(), asteroidPosition() ), rotation(), this, 0.02f, 3, sectorSize );
		}
	}

	bool entityListDirty = true;

	void handleSectorChange ( ivec2 newSector ) {
		ZoneScoped;
		if ( newSector != sectorID ) {
			// sector change
			logHighPriority( "Leaving Sector " + to_string( sectorID.x ) + ", " + to_string( sectorID.y ) );
			sectorID = newSector;
			// no sector to clear - this is the state on program startup
		}
		logHighPriority( "Entering Sector " + to_string( sectorID.x ) + ", " + to_string( sectorID.y ) );
		clearSector(); // zero out list, create entry for player
		spawnSector(); // create entries for the rest of the entities in the sector
	}

	void init () {
		ZoneScoped;
		// create the buffers for the CWBVH8 nodes data
		glCreateBuffers( 1, &cwbvhNodesDataBuffer );
		glObjectLabel( GL_BUFFER, cwbvhNodesDataBuffer, -1, string( "CWBVH Node Data" ).c_str() );

		// allocate some initial footprint
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhNodesDataBuffer );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, cwbvhNodesDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, maxTris * 2 * sizeof( tinybvh::bvhvec4 ), nullptr, GL_DYNAMIC_DRAW );

		// for the CWBVH8 triangle data
		glCreateBuffers( 1, &cwbvhTrisDataBuffer );
		glObjectLabel( GL_BUFFER, cwbvhTrisDataBuffer, -1, string( "CWBVH Tri Data" ).c_str() );

		// allocate the initial buffer
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhTrisDataBuffer );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, cwbvhTrisDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, maxTris * 4 * sizeof( tinybvh::bvhvec4 ), nullptr, GL_DYNAMIC_DRAW );

		// and for the texcoord data
		glCreateBuffers( 1, &triangleDataBuffer );
		glObjectLabel( GL_BUFFER, triangleDataBuffer, -1, string( "Triangle Data With Texcoords" ).c_str() );

		// initial buffer allocation
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, triangleDataBuffer );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, triangleDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, maxTris * 4 * sizeof( tinybvh::bvhvec4 ), nullptr, GL_DYNAMIC_DRAW );
	}

	// moving to a static allocation instead of using vectors... maybe?
	const uint32_t maxTris = 8196u;
	array< tinybvh::bvhvec4, 8196 > triangleDataNoTexcoords;
	array< vec4, 8196 > triangleDataWithTexcoords;

	void updateBVH () {
		ZoneScoped;
		uint32_t numTriangles = BuildTriangleList();
		BuildBVH( entityBVH, &triangleDataNoTexcoords[ 0 ], numTriangles );
		BufferUpdate( cwbvhNodesDataBuffer, 0, entityBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) entityBVH.bvh8Data );
		BufferUpdate( cwbvhTrisDataBuffer, 1, entityBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) entityBVH.bvh8Tris );
		BufferUpdate( triangleDataBuffer, 2, numTriangles * sizeof( vec4 ), ( GLvoid* ) &triangleDataWithTexcoords[ 0 ] );
	}

	uint32_t BuildTriangleList () {
		ZoneScoped;
		uint32_t pushIndex = 0;
		uint32_t numTriangles = 0;
		for ( auto& e : entityList ) {
			bboxData bbox = e.getBBoxPoints();

			// 12 triangles, 3 points each - triangles only for basic traversal, texcoords needed for alpha test
			for ( int i = 0; i < 12; i++ ) {
				const int idx = i * 3;
				for ( int j = 0; j < 3; j++ ) {
					tinybvh::bvhvec4 p( 0.0f );
					p.x = bbox.points[ idx + j ].x;
					p.y = bbox.points[ idx + j ].y;
					p.z = bbox.points[ idx + j ].z;

					// put it into the arrays...
					triangleDataNoTexcoords[ pushIndex ] = p;
					triangleDataWithTexcoords[ pushIndex ] = vec4( bbox.texcoords[ idx + j ].xyz, 0.0f );
					pushIndex++;
				}
				numTriangles++;
			}
		}
		return numTriangles;
	}

	void BuildBVH ( tinybvh::BVH8_CWBVH &bvh, tinybvh::bvhvec4 *data, uint32_t triangleCount ) {
		ZoneScoped;
		bvh.BuildHQ( data, triangleCount );
	}

	void BufferUpdate ( GLuint bufferName, uint32_t bufferNumber, uint32_t bufferSize, const GLvoid *data ) {
		ZoneScoped;
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, bufferName );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, bufferNumber, bufferName );
		glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, static_cast< GLsizeiptr >( bufferSize ), data );
	}

	void update () {
	// primary update work
		ZoneScoped;

		// update player location - rounding applied for sector
		entityList[ 0 ].position = ship.position - glm::floor( ship.position );
		entityList[ 0 ].rotation = ship.angle;

		// check whether we have left the sector
		if ( ivec2 sector = ivec2( floor( ship.position.x ), floor( ship.position.y ) ); sector != sectorID ) {
			handleSectorChange( sector );
		}

		// call everyone's update() function (dummy right now)
		for ( int i = 1; i < entityList.size(); i++ ) {
			const float angle = entityList[ i ].rotation;
			const float velocity = entityList[ i ].shipSpeed;
			entityList[ i ].position +=
				glm::mat2(
					cos( angle ), -sin( angle ),
					sin( angle ), cos( angle )
				) * vec2( velocity, 0.0f );
		}
	}

	GLuint drawShader;
	textureManager_t *textureManager = nullptr;
	void tinyTextDrawString ( string s, ivec2 basePoint ) const {
		ZoneScoped;
		const int width = 1280;
		const int height = 720;

		vector< uint32_t > stringBytes;
		for ( auto& c : s ) {
			stringBytes.push_back( uint32_t( c ) );
		}

		GLuint shader = drawShader;
		glUseProgram( shader );
		glUniform1uiv( glGetUniformLocation( shader, "text" ), stringBytes.size(), &stringBytes[ 0 ] );
		glUniform1ui( glGetUniformLocation( shader, "numChars" ), stringBytes.size() );
		glUniform2i( glGetUniformLocation( shader, "basePointOffset" ), basePoint.x, basePoint.y );

		textureManager->BindImageForShader( "Accumulator", "writeTarget", shader, 1 );
		glBindImageTexture( 1, textureManager->Get( "Accumulator" ), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F );
		textureManager->BindImageForShader( "TinyFont", "fontAtlas", shader, 2 );

		// it'll make sense to do this for only the affected pixels, rather than the whole buffer, that's not super important right now
		glDispatchCompute( ( width + 15 ) / 16, ( height + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}

	void tinyTextDrawing ( textureManager_t &textureManager ) const {
		ZoneScoped;

	// sector and ship stats
		ivec2 p = ivec2( 16, 16 );
		// count up number of friends, foes, asteroids
		int numFriends = 0, numFoes = 0, numAsteroids = 0;
		for ( auto &entity : entityList ) {
			if ( entity.type == FRIEND ) numFriends++;
			else if ( entity.type == FOE ) numFoes++;
			else if ( entity.type == ASTEROID ) numAsteroids++;
		}
		tinyTextDrawString( " Sector " + to_string( sectorID.x ) + " , " + to_string( sectorID.y ), p );
		p.y += 8;
		tinyTextDrawString( "---------------", p );
		p.y += 8;
		tinyTextDrawString( " Friends:   " + fixedWidthNumberString( numFriends, 2, ' ' ), p );
		p.y += 8;
		tinyTextDrawString( " Enemies:   " + fixedWidthNumberString( numFoes, 2, ' ' ), p );
		p.y += 8;
		tinyTextDrawString( " Asteroids: " + fixedWidthNumberString( numAsteroids, 2, ' ' ), p );
		p.y += 8;
		tinyTextDrawString( "---------------", p );
		p.y += 8;
		tinyTextDrawString( " HDG  " + fixedPointNumberStringF( glm::degrees( ship.angle ), 3, 4 ) + "\'", p );
		p.y += 8;
		tinyTextDrawString( " SPD  " + fixedPointNumberStringF( ship.velocity, 3, 4 ) + "m/s", p );
		p.y += 8;
		tinyTextDrawString( "---------------", p );
		p.y += 8;
		tinyTextDrawString( " POS  X: " + fixedWidthNumberString( int( RangeRemap( ship.position.x - floor( ship.position.x ), 0.0f, 1.0f, -sectorSize / 2.0f, sectorSize / 2.0f ) ), 5, ' ' ), p );
		p.y += 8;
		tinyTextDrawString( "      Y: " + fixedWidthNumberString( int( RangeRemap( ship.position.y - floor( ship.position.y ), 0.0f, 1.0f, -sectorSize / 2.0f, sectorSize / 2.0f ) ), 5, ' ' ), p );
	}

	ivec2 screenPos ( vec2 worldXY ) {
		return ivec2( // mapping to pixel coords
			RangeRemap( worldXY.x, -( 1280.0f / 720.0f ), ( 1280.0f / 720.0f ), 0.0f, 1280.0f ),
			RangeRemap( worldXY.y, 1.0f, -1.0f, 0.0f, 720.0f )
		);
	}

	void lineUIDraw () {
		const vec2 shipPosition = ship.GetVelocityVector() * ( ship.velocity / ship.stats.maxSpeedForward ) * ship.stats.maxThrustDisplacement * ( 0.0618f );

		vector< vec2 > stations;
		vector< vec2 > enemies;
		vector< vec2 > asteroids;
		vector< vec2 > sectorBoundaries;

		// populate lists...
		for ( auto& entity : entityList ) {
			if ( entity.type == STATION ) {
				stations.push_back( entity.position );
			} else if ( entity.type == FOE ) {
				enemies.push_back( entity.position );
			} else if ( entity.type == ASTEROID ) {
				asteroids.push_back( entity.position );
			}
		}

		if ( glm::fract( ship.position.x ) < 0.01f || glm::fract( ship.position.y ) < 0.01f
			|| glm::fract( ship.position.x ) > 0.99f || glm::fract( ship.position.y ) > 0.99f ) {
			if ( glm::fract( ship.position.x ) < 0.01f ) {
				sectorBoundaries.push_back( vec2( -glm::fract( ship.position.x ), 0.0f ) );
			} else if ( glm::fract( ship.position.y ) < 0.01f ) {
				sectorBoundaries.push_back( vec2( 0.0f, -glm::fract( ship.position.y ) ) );
			} else if ( glm::fract( ship.position.x ) > 0.99f ) {
				sectorBoundaries.push_back( vec2( 1.0f - glm::fract( ship.position.x ), 0.0f ) );
			} else if ( glm::fract( ship.position.y ) > 0.99f ) {
				sectorBoundaries.push_back( vec2( 0.0f, 1.0f - glm::fract( ship.position.y ) ) );
			}
		}

		// we want to express a couple pieces of information on the UI here...
			// if station is in sector, point to it
		if ( stations.size() > 0 ) {
			// max one station per sector, draw the line
			vec2 dirStation = normalize( stations[ 0 ] - ship.position );
			lines.AddLine( 0, screenPos( shipPosition + 0.15f * dirStation ), screenPos( shipPosition + 0.2f * dirStation ) );
			tinyTextDrawString( " STATION " + std::to_string( int( glm::length( stations[ 0 ] - ship.position ) * sectorSize ) ), screenPos( shipPosition + 0.25f * dirStation ) );
		}

		// if there are enemies in the sector, point to the nearest one
		if ( enemies.size() > 0 ) {
			// sort by distance, take the closest
			std::sort( enemies.begin(), enemies.end(), [=] ( vec2 a, vec2 b ) { return distance( a, ship.position ) < distance( b, ship.position ); } );
			vec2 dirEnemy = normalize( enemies[ 0 ] - ship.position );

			// can solve for direction with atan, then draw a little caret shape rotated to match - I think I prefer that to the straight lines
			lines.AddLine( 1, screenPos( shipPosition + 0.15f * dirEnemy ), screenPos( shipPosition + 0.2f * dirEnemy ) );
			tinyTextDrawString( " ENEMY " + std::to_string( int( glm::length( enemies[ 0 ] - ship.position ) * sectorSize ) ), screenPos( shipPosition + 0.25f * dirEnemy ) );
		}

		// if the player has a target locked, point to it
			// todo

		// if the player has a quest, point towards the quest target
			// todo

		// if the player is near a sector boundary, point towards it... sector is 0.0f to 1.0f
		if ( sectorBoundaries.size() > 0 ) {
			// sort by distance, take the closest
			std::sort( sectorBoundaries.begin(), sectorBoundaries.end(), [=] ( vec2 a, vec2 b ) { return distance( a, ship.position ) < distance( b, ship.position ); } );
			vec2 dirSectorBoundary = normalize( sectorBoundaries[ 0 ] );
			lines.AddLine( 4, screenPos( shipPosition + 0.15f * dirSectorBoundary ), screenPos( shipPosition + 0.2f * dirSectorBoundary ) );
			tinyTextDrawString( " SECTOR BOUNDARY " + std::to_string( int( length( sectorBoundaries[ 0 ] ) * sectorSize ) ), screenPos( shipPosition + 0.25f * dirSectorBoundary ) );
		}

		// if the player has a mining laser equipped, point to the nearest asteroid... todo: toggle
		if ( asteroids.size() > 0 ) {
			// sort by distance, take the closest
			std::sort( asteroids.begin(), asteroids.end(), [=] ( vec2 a, vec2 b ) { return distance( a, ship.position ) < distance( b, ship.position ); } );
			vec2 dirAsteroid = normalize( asteroids[ 0 ] - ship.position );
			lines.AddLine( 5, screenPos( shipPosition + 0.15f * dirAsteroid ), screenPos( shipPosition + 0.2f * dirAsteroid ) );
			tinyTextDrawString( " ASTEROID " + std::to_string( int( glm::length( asteroids[ 0 ] - ship.position ) * sectorSize ) ), screenPos( shipPosition + 0.25f * dirAsteroid ) );
		}

		// debug
			// bounding boxes... involves a bit of plumbing
			// thrust vectors from ship's engine locations
			// displacement from center...
		// lines.AddLine( 7, screenPos( vec2( 0.0f ) ), screenPos( shipPosition ) );

		// to add a line:
		// controller.lines.AddLine( layer, p1, p2 );

		lines.Update();
	}
};

class AtlasManager {
public:

    uint32_t currentAtlasDim;               // Current width/height of the atlas (power of two)
    Image_4U atlasImage;                    // The atlas image being managed

    stbrp_context ctx;                      // Rectangle packing context
    std::vector< stbrp_node > nodes;        // Nodes for rectangle packing

    std::vector< std::array< uint32_t, 4 > > entityRegions; // Packed regions for entities, SSBO prepped

    void ResizeAtlas () { // crop can be used to increase size, maybe somewhat paradoxically
        atlasImage.Crop(  // can't go bigger than the max texture size, which is usually 16384
            std::clamp( currentAtlasDim, 0u, 1u << 14 ),
            std::clamp( currentAtlasDim, 0u, 1u << 14 )
        );
    }

    textureManager_t * textureManager = nullptr;
    GLuint atlasTexture = 0;

    void UploadToGPU () {
		ZoneScoped;
        textureOptions_t opts;
        opts.dataType = GL_RGBA8;
        opts.textureType = GL_TEXTURE_2D;
        opts.minFilter = GL_NEAREST;
        opts.magFilter = GL_NEAREST;
        opts.width = currentAtlasDim;
        opts.height = currentAtlasDim;
        opts.initialData = atlasImage.GetImageDataBasePtr();

        if ( atlasTexture == 0 ) {
            atlasTexture = textureManager->Add( "AtlasTexture", opts );
        } else {
            // also need to update if the atlas texture is already in the texture manager
            glActiveTexture( GL_TEXTURE0 );
            glBindTexture( GL_TEXTURE_2D, atlasTexture );
            glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, currentAtlasDim, currentAtlasDim, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasImage.GetImageDataBasePtr() );
        }
    }

    void CreateOrUpdateSSBO () {
		ZoneScoped;
        static GLuint ssbo = 0;

    	/*
        cout << "We have some regions:" << endl;
        for ( const auto& r : entityRegions ) {
            cout << "  x:" << r[ 0 ] << " y:" << r[ 1 ] << " w:" << r[ 2 ] << " h:" << r[ 3 ] << endl;
        }
        cout << endl;
    	*/

        // Create SSBO if it does not exist
        if ( !ssbo ) {
            glGenBuffers( 1, &ssbo );
        }

        // Update SSBO data used to reference the atlas
        glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
        glBufferData( GL_SHADER_STORAGE_BUFFER, entityRegions.size() * sizeof( uint32_t ) * 4, entityRegions.data(), GL_DYNAMIC_DRAW );
        glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, ssbo );
        glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 ); // unbind
    }

public:
    AtlasManager () :  currentAtlasDim( 512 ), atlasImage( currentAtlasDim, currentAtlasDim ) {}

    void UpdateAtlas ( vector< entity > &entityList ) {
		ZoneScoped;
    	logHighPriority( "Atlas Rebuilt" );
        auto tStart = std::chrono::steady_clock::now();
    	entityRegions.clear();
        while ( true ) {
            nodes.resize( currentAtlasDim );
            stbrp_init_target( &ctx, currentAtlasDim, currentAtlasDim, nodes.data(), currentAtlasDim );

            // treating all sprites uniformly, including the user's model sprite
            std::vector< stbrp_rect > rects( entityList.size() );
            for ( size_t i = 0; i < entityList.size(); ++i ) {
                rects[ i ].id = static_cast< int >( i );
                rects[ i ].w = entityList[ i ].entityImage.Width() + 3; // add a bit of padding, for filtering purposes
                rects[ i ].h = entityList[ i ].entityImage.Height() + 3;
            }

            if ( stbrp_pack_rects( &ctx, rects.data(), rects.size() ) ) {
                atlasImage.ClearTo( color_4U( { 0, 0, 0, 0 } ) );
                entityRegions.clear();
                for ( const auto &rect : rects ) {
                    if ( rect.was_packed ) {
                        entityRegions.push_back( { uint32_t( rect.x ), uint32_t( rect.y ), uint32_t( rect.w ), uint32_t( rect.h ) } );
                        for ( uint32_t y = 0; y < rect.h; ++y ) {
                            for ( uint32_t x = 0; x < rect.w; ++x ) {
                                entityList[ rect.id ].atlasIndex = rect.id;
                                auto color = entityList[ rect.id ].entityImage.GetAtXY( x, y );
                                atlasImage.SetAtXY( rect.x + x, rect.y + y, color );
                            }
                        }
                    }
                }
                UploadToGPU();
                break;
            }

            currentAtlasDim *= 2;
            ResizeAtlas();
        }

        // create the SSBO which allows the shader to use the atlas
        CreateOrUpdateSSBO();

        // cout << "Atlas Creation took " << float( std::chrono::duration_cast< std::chrono::microseconds >( std::chrono::steady_clock::now() - tStart ).count() / 1000.0f ) << " ms" << endl;
    }
};
#endif