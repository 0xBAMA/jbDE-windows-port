#include "../../engine/includes.h"

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

	vec2 position = vec2( 0.5f );

	spaceshipStats stats;

	void Update ( inputHandler_t &input ) {
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
		position -= deltaT * GetVelocityVector() / ( 20000.0f );
	}

	void turn ( const float &amount ) {
		angle += amount;
	}

	void accelerate ( const float &amount ) {
		velocity += amount;
	}

	vec2 GetPositionVector() const {
		return vec2(
			RangeRemap( position.x - floor( position.x ), 0.0f, 1.0f, -10000.0f, 10000.0f ),
			RangeRemap( position.y - floor( position.y ), 0.0f, 1.0f, -10000.0f, 10000.0f )
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
		// create the initial data for points and texcoords
		for ( int i = 0; i < numPointsBBox; i++ ) {
			points[ i ] = CubeVert( i );
			texcoords[ i ] = glm::clamp( CubeVert( i ), 0.0f, 1.0f );
			texcoords[ i ].z = float( texID );
		}
	}
};

// high-level ID
#define OBJECT	(0)
#define FRIEND	(1)
#define FOE		(2)

// FSM logic states
#define INACTIVE	(-1)
#define INITIAL		(0)
// ...

// forward declare for entity access to sector info
class universeController;

struct entity {
	// since this is kind of a tagged union... some extra data
	int type = OBJECT;
	float shipHeading = 0.0f;
	float shipSpeed = 0.0f;

	// todo
	int FSMState = INITIAL;

	// keeping an Image_4U here might be a good solution to unique entity appearances?
		// this allows for ongoing update of the model
	Image_4U entityImage;
		// note that updates will require that you revert to the base model, so any time this updates, you need to fetch the base image for your ship from the universe controller
	string baseEntityImageLabel; // and this should come in during object construction

	// so that e.g. ships can query what other objects are in the sector
	universeController *universe = nullptr;

	// for the display primitive
	vec2 location = vec2( 0.0f );
	vec2 scale = vec2( 1.0f, 0.618f );
	float rotation = 0.0f;

	// kept in the third coordinate of the texcoord - we need to know this when we create the entity
		// this indexes into SSBO with atlased texture info (1D index -> texture info)
	int textureIndex = -1;

	// constructor for entity
	entity () = default;
	entity ( int type, vec2 location, float rotation, universeController *universe, vec2 scale = vec2 ( 1.0f ) )
		: type ( type ), location ( location ), rotation ( rotation ), universe ( universe ), scale ( scale ) {}

	bboxData getBBoxPoints () const {
		// initial points
		bboxData points( textureIndex );

		// scaling the bbox - somewhat specialized... smaller ships are taller, is the plan for handling occlusion
		for ( auto& p : points.points ) {
			// apply scaling
			p = ( glm::scale( vec3( scale.x, scale.y, clamp( 1.0f / ( ( scale.x + scale.y ) / 2.0f ), 0.0f, 100.0f ) ) ) * vec4( p, 1.0f ) ).xyz();

			// apply rotation
			p = ( glm::angleAxis( -rotation, vec3( 0.0f, 0.0f, 1.0f ) ) * vec4( p, 1.0f ) ).xyz();

			// apply translation - accounting for the scaling that needs to be applied to the stored location value
			p = ( glm::translate( vec3(
				RangeRemap( location.x, 0.0f, 1.0f, -10000.0f, 10000.0f ),
				RangeRemap( location.y, 0.0f, 1.0f, -10000.0f, 10000.0f ),
				0.0f ) ) * vec4( p, 1.0f ) ).xyz();
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
	ivec2 sectorID = ivec2( 0 );

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

		entityList.resize( 1 );
		entityList[ 0 ].location = ship.position;
		entityList[ 0 ].rotation = ship.angle;

		handleSectorChange( sectorID );
	}

	void clearSector () {
		entityList.resize( 1 );
	}

	void spawnSector () {
		// Spawn station at the center of the sector
		entityList.emplace_back( OBJECT, vec2 ( 0.5f, 0.5f ), 0.0f, this, vec2 ( 2.0f ) );

		rngi countGenerator( 0, 3 );
		const int numFriends = 100;
		const int numFoes = countGenerator();
		const int numAsteroids = countGenerator() * 2 + 5;

		// RNG distributions
		// rng friendPosition ( 0.4f, 0.6f ), rotation ( 0.0f, tau );
		rng friendPosition ( 0.49f, 0.51f ), rotation ( 0.0f, tau );
		rngi foeEdgeSelector ( 0, 3 );
		rng foeEdgePosition ( 0.0f, 1.0f );
		rng asteroidPosition ( 0.0f, 1.0f );

		// Spawn friendly ships - consistent order of parameters
		for ( int i = 0; i < numFriends; ++i ) {
			entityList.emplace_back( FRIEND, vec2 ( friendPosition(), friendPosition() ), rotation(), this, vec2 ( 0.3f ) );
		}

		// Spawn enemy ships along the edges of the sector
		for ( int i = 0; i < numFoes; ++i ) {
			const int edge = foeEdgeSelector();
			const float x = ( edge == 2 ) ? 0.01f : ( edge == 3 ) ? 0.99f : foeEdgePosition();
			const float y = ( edge == 0 ) ? 0.99f : ( edge == 1 ) ? 0.01f : foeEdgePosition();
			entityList.emplace_back( FOE, vec2 ( x, y ), rotation(), this, vec2( 0.2f ) );
		}

		// Spawn asteroids across the entire sector
		for ( int i = 0; i < numAsteroids; ++i ) {
			entityList.emplace_back( OBJECT, vec2 ( asteroidPosition(), asteroidPosition() ), rotation(), this, vec2 ( 0.3f ) );
		}
	}

	void handleSectorChange ( ivec2 newSector ) {
		if ( newSector != sectorID ) {
			// sector change
			clearSector();
			logHighPriority( "Leaving Sector " + to_string( sectorID.x ) + ", " + to_string( sectorID.y ) );
			sectorID = newSector;
			// no sector to clear - this is the state on program startup
		}
		logHighPriority( "Entering Sector " + to_string( sectorID.x ) + ", " + to_string( sectorID.y ) );
		spawnSector();
	}

	void init () {
		// create the buffers for the BVH stuff
		glCreateBuffers( 1, &cwbvhNodesDataBuffer );
		glObjectLabel( GL_BUFFER, cwbvhNodesDataBuffer, -1, string( "CWBVH Node Data" ).c_str() );

		glCreateBuffers( 1, &cwbvhTrisDataBuffer );
		glObjectLabel( GL_BUFFER, cwbvhTrisDataBuffer, -1, string( "CWBVH Tri Data" ).c_str() );

		glCreateBuffers( 1, &triangleDataBuffer );
		glObjectLabel( GL_BUFFER, triangleDataBuffer, -1, string( "Triangle Data With Texcoords" ).c_str() );

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
		// prepare SSBO for the atlas with LUT, int texture ID -> basePoint and fractional size (base uv and size, since geo will have texcoords 0..1)
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
				const int idx = i * 3;
				for ( int j = 0; j < 3; j++ ) {
					tinybvh::bvhvec4 p( 0.0f );
					p.x = bbox.points[ idx + j ].x;
					p.y = bbox.points[ idx + j ].y;
					p.z = bbox.points[ idx + j ].z;
					triangleDataNoTexcoords.push_back( p );
					triangleDataWithTexcoords.push_back( bbox.points[ idx + j ] );
					triangleDataWithTexcoords.push_back( bbox.texcoords[ idx + j ] );
				}
			}
		}

		// Tick();
		entityBVH.BuildHQ( &triangleDataNoTexcoords[ 0 ], triangleDataNoTexcoords.size() / 3 );
		// float msTakenBVH = Tock();
		// cout << endl << "BVH built in " << msTakenBVH / 1000.0f << "s\n";

		// Tick();
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhNodesDataBuffer );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, cwbvhNodesDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, entityBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) entityBVH.bvh8Data, GL_DYNAMIC_COPY );
		// cout << "CWBVH8 Node Data is " << GetWithThousandsSeparator( entityBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhTrisDataBuffer );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, cwbvhTrisDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, entityBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) entityBVH.bvh8Tris, GL_DYNAMIC_COPY );
		// cout << "CWBVH8 Triangle Data is " << GetWithThousandsSeparator( entityBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, triangleDataBuffer );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, triangleDataBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, triangleDataWithTexcoords.size() * sizeof( vec3 ), ( GLvoid* ) &triangleDataWithTexcoords[ 0 ], GL_DYNAMIC_COPY );
		// cout << "Triangle Test Data is " << GetWithThousandsSeparator( triangleDataWithTexcoords.size() * sizeof( vec3 ) ) << " bytes" << endl;

		// float msTakenBufferBVH = Tock();
		// cout << endl << "BVH passed to GPU in " << msTakenBufferBVH / 1000.0f << "s\n";
	}

	void update () {
	// primary update work

		// player position restored from storage format
		vec2 shipPosition = vec2(
			RangeRemap( ship.position.x - floor( ship.position.x ), 0.0f, 1.0f, -10000.0f, 10000.0f ),
			RangeRemap( ship.position.y - floor( ship.position.y ), 0.0f, 1.0f, -10000.0f, 10000.0f )
		);

		// update player location - rounding applied for sector
		entityList[ 0 ].location = ship.position - glm::floor( ship.position );
		entityList[ 0 ].rotation = ship.angle;

		// check whether we have left the sector
		if ( ivec2 sector = ivec2( floor( ship.position.x ), floor( ship.position.y ) ); sector != sectorID ) {
			handleSectorChange( sector );
		}

		// call everyone's update() function (dummy right now)
		for ( int i = 1; i < entityList.size(); i++ ) {
			const float angle = entityList[ i ].rotation;
			const float velocity = entityList[ i ].shipSpeed;
			entityList[ i ].location +=
				glm::mat2(
					cos( angle ), -sin( angle ),
					sin( angle ), cos( angle )
				) * vec2( velocity, 0.0f );
		}

	// is there a new entity in play? we need to rebuild the atlas
		// rebuild atlas + index SSBO
		// entities leaving, not as important to evict from the atlas

	// because of the way we're using it... we need to update the BVH and upload it to the GPU every frame

	// cleanup work
		// has the player left the sector? we have work to do
		// the 

	}

	GLuint drawShader;
	textureManager_t *textureManager = nullptr;
	void tinyTextDrawString ( string s, ivec2 basePoint ) const {
		const int width = 720;
		const int height = 480;

		vector< uint32_t > stringBytes;
		for ( auto& c : s ) {
			stringBytes.push_back( uint32_t( c ) );
		}

		GLuint shader = drawShader;
		glUseProgram( shader );
		glUniform1uiv( glGetUniformLocation( shader, "text" ), stringBytes.size(), &stringBytes[ 0 ] );
		glUniform1ui( glGetUniformLocation( shader, "numChars" ), stringBytes.size() );
		glUniform2i( glGetUniformLocation( shader, "basePointOffset" ), basePoint.x, basePoint.y );

		textureManager->BindImageForShader( "Display Texture", "writeTarget", shader, 1 );
		glBindImageTexture( 1, textureManager->Get( "Display Texture" ), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
		textureManager->BindImageForShader( "TinyFont", "fontAtlas", shader, 2 );

		// it'll make sense to do this for only the affected pixels, rather than the whole buffer, that's not super important right now
		glDispatchCompute( ( width + 15 ) / 16, ( height + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}

	void tinyTextDrawing ( textureManager_t &textureManager ) const {

	// sector and ship stats
		ivec2 p = ivec2( 16, 458 );
		// count up number of friends, foes, asteroids
		int numFriends = 0, numFoes = 0, numAsteroids = 0;
		for ( auto &entity : entityList ) {
			if ( entity.type == FRIEND ) numFriends++;
			else if ( entity.type == FOE ) numFoes++;
			else if ( entity.type == OBJECT ) numAsteroids++;
		}
		tinyTextDrawString( " Sector " + to_string( sectorID.x ) + " , " + to_string( sectorID.y ), p );
		p.y -= 8;
		tinyTextDrawString( "---------------", p );
		p.y -= 8;
		tinyTextDrawString( " Friends:   " + fixedWidthNumberString( numFriends, 2, ' ' ), p );
		p.y -= 8;
		tinyTextDrawString( " Foes:      " + fixedWidthNumberString( numFoes, 2, ' ' ), p );
		p.y -= 8;
		tinyTextDrawString( " Asteroids: " + fixedWidthNumberString( numAsteroids, 2, ' ' ), p );
		p.y -= 8;
		tinyTextDrawString( "---------------", p );
		p.y -= 8;
		tinyTextDrawString( "[HDG] " + fixedPointNumberStringF( glm::degrees( ship.angle ), 3, 4 ) + "\'", p );
		p.y -= 8;
		tinyTextDrawString( "[SPD] " + fixedPointNumberStringF( ship.velocity, 3, 4 ) + "m/s", p );
		p.y -= 8;
		tinyTextDrawString( "---------------", p );
		p.y -= 8;
		tinyTextDrawString( "[POS] X: " + fixedWidthNumberString( int( RangeRemap( ship.position.x - floor( ship.position.x ), 0.0f, 1.0f, -10000.0f, 10000.0f ) ), 5, ' ' ), p );
		p.y -= 8;
		tinyTextDrawString( "      Y: " + fixedWidthNumberString( int( RangeRemap( ship.position.y - floor( ship.position.y ), 0.0f, 1.0f, -10000.0f, 10000.0f ) ), 5, ' ' ), p );
	}

};
