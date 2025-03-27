#include "../../../engine/engine.h"
#include "generate.h"
#include "model.h"

struct ChorizoConfig_t {
	GLuint vao;
	GLuint shapeParametersBuffer;
	GLuint boundsTransformBuffer;
	GLuint pointSpriteParametersBuffer;
	GLuint lightsBuffer;
	GLuint primaryFramebuffer[ 2 ];

	vec3 eyePosition = vec3( -1.0f, 0.0f, -3.0f );
	mat4 viewTransform;
	mat4 viewTransformInverse;
	mat4 projTransform;
	mat4 projTransformInverse;
	mat4 combinedTransform;
	mat4 combinedTransformInverse;
	bool enableThinLens = false;

	vec3 basisX = vec3( 1.0f, 0.0f, 0.0f );
	vec3 basisY = vec3( 0.0f, 1.0f, 0.0f );
	vec3 basisZ = vec3( 0.0f, 0.0f, 1.0f );

	float nearZ = 0.1f;
	float farZ = 100.0f;

	float FoV = 45.0f;

	float volumetricStrength = 0.03f;
	vec3 volumetricColor = vec3( 0.1f, 0.2f, 0.3f );
	vec3 ambientColor = vec3( 0.0 );

	int numLights = 64;
	std::vector< vec4 > lights;

	bool progressiveBlend = false;
	int progressiveFrameCount = 0;

	float blendAmount = 0.1f;
	float focusDistance = 3.5f;
	float apertureAdjust = 0.01f;

	// separate palettes for each generator
	int lightPaletteID = 0;
	float lightPaletteMin = 0.0f;
	float lightPaletteMax = 1.0f;

	// I think a generecized generator needs:
		// some kind of type identifier... probably a string, not sure
		// palette ID
		// palette min, max
		// draw toggle
			// toggling individual trees... separate generator, per tree? maybe makes sense, actually
		// info to draw - this is also used if you need to clear out the data associated with these primitives
			// index of first bounding box impostor
			// count of bounding box impostors ( can be zero, e.g. lights )
			// index of first point sprite impostor
			// count of point sprite impostors ( debug geo for lights )

		// among other things, this lets us:
			// clear those ranges, say you wanted to regenerate e.g. only the trees, only the grass, etc
			// batching the drawing, I think it should Just Work with gl_VertexID

	int grassPaletteID = 0;
	float grassPaletteMin = 0.0f;
	float grassPaletteMax = 1.0f;

	int treePaletteID = 0;
	float treePaletteMin = 0.0f;
	float treePaletteMax = 1.0f;

	int groundPaletteID = 0;
	float groundPaletteMin = 0.0f;
	float groundPaletteMax = 1.0f;

	rngi wangSeeder = rngi( 0, 10042069 );

	geometryManager_t geometryManager;
	int numPrimitives = 0;
	int numPointSprites = 0;
	uint32_t frameCount = 0;
};

class Chorizo final : public engineBase {
public:
	Chorizo () { Init(); OnInit(); PostInit(); }
	~Chorizo () { Quit(); }

	ChorizoConfig_t ChorizoConfig;

	// SoftBody Simulation Model
	model simulationModel;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "../src/projects/Impostors/ChorizoSoftbody/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Bounds" ] = computeShader( "../src/projects/Impostors/ChorizoSoftbody/shaders/bounds.cs.glsl" ).shaderHandle;
			shaders[ "Animate" ] = computeShader( "../src/projects/Impostors/ChorizoSoftbody/shaders/animate.cs.glsl" ).shaderHandle;

			// setup raster shaders
			shaders[ "Bounding Box" ] = regularShader(
				"../src/projects/Impostors/ChorizoSoftbody/shaders/bbox.vs.glsl",
				"../src/projects/Impostors/ChorizoSoftbody/shaders/bbox.fs.glsl"
			).shaderHandle;
			shaders[ "Point Sprite" ] = regularShader(
				"../src/projects/Impostors/ChorizoSoftbody/shaders/pointSprite.vs.glsl",
				"../src/projects/Impostors/ChorizoSoftbody/shaders/pointSprite.fs.glsl"
			).shaderHandle;

			// create and bind a basic vertex array
			glCreateVertexArrays( 1, &ChorizoConfig.vao );
			glBindVertexArray( ChorizoConfig.vao );

			// single sided polys - only draw those which are facing outwards
			glEnable( GL_CULL_FACE );
			glCullFace( GL_BACK );
			glFrontFace( GL_CCW );
			glDisable( GL_BLEND );
			glEnable( GL_PROGRAM_POINT_SIZE );

			// get some initial data for the impostor shapes
			rngi pick = rngi( 0, palette::paletteListLocal.size() - 1 );
			uint32_t colorCap = 14;
			do { ChorizoConfig.treePaletteID = pick(); } while ( palette::paletteListLocal[ ChorizoConfig.treePaletteID ].colors.size() > colorCap );
			do { ChorizoConfig.grassPaletteID = pick(); } while ( palette::paletteListLocal[ ChorizoConfig.grassPaletteID ].colors.size() > colorCap );
			do { ChorizoConfig.groundPaletteID = pick(); } while ( palette::paletteListLocal[ ChorizoConfig.groundPaletteID ].colors.size() > colorCap );
			do { ChorizoConfig.lightPaletteID = pick(); } while ( palette::paletteListLocal[ ChorizoConfig.lightPaletteID ].colors.size() > colorCap );

			// regenTree();
			simulationModel.loadFramePoints();

			AddCurrentSoftbodyPoints();
			AddLights();
			PrepSSBOs();

			// == Render Framebuffer(s) ===========
			textureOptions_t opts;
			// ==== Depth =========================
			opts.dataType = GL_DEPTH_COMPONENT32;
			opts.textureType = GL_TEXTURE_2D;
			opts.width = config.width;
			opts.height = config.height;
			textureManager.Add( "Framebuffer Depth 0", opts );
			textureManager.Add( "Framebuffer Depth 1", opts );
			// ==== Normal =======================
			opts.dataType = GL_RGBA16F;
			textureManager.Add( "Framebuffer Normal 0", opts );
			textureManager.Add( "Framebuffer Normal 1", opts );
			// ==== Primitive ID ==================
			opts.dataType = GL_RG32UI;
			textureManager.Add( "Framebuffer Primitive ID 0", opts );
			textureManager.Add( "Framebuffer Primitive ID 1", opts );
			// ====================================

			// setup the buffers for the rendering process - front and back framebuffers
			glGenFramebuffers( 2, &ChorizoConfig.primaryFramebuffer[ 0 ] );
			const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 }; // both framebuffers have implicit depth + 2 color attachments

			for ( int i = 0; i < 2; i++ ) {
				// creating the actual framebuffers with their attachments
				glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ i ] );
				glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager.Get( "Framebuffer Depth " + std::to_string( i ) ), 0 );
				glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager.Get( "Framebuffer Normal " + std::to_string( i ) ), 0 );
				glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager.Get( "Framebuffer Primitive ID " + std::to_string( i ) ), 0 );
				glDrawBuffers( 2, bufs ); // how many active attachments, and their attachment locations
				if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
					cout << newline << "framebuffer " << i << " creation successful" << newline;
				}
			}

			glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ 1 ] );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager.Get( "Framebuffer Depth 1" ), 0 );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager.Get( "Framebuffer Normal 1" ), 0 );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager.Get( "Framebuffer Primitive ID 1" ), 0 );
			glDrawBuffers( 2, bufs );
			if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
				cout << "back framebuffer creation successful" << newline << newline;
			}

			// bind default framebuffer
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		}
	}

	struct recursiveTreeConfig {
		int numBranches = 3;
		float rotateJitterMin = 0.0f;
		float rotateJitterMax = 0.5f;
		float branchTilt = 0.1f;
		float branchJitterMin = 0.0f;
		float branchJitterMax = 0.5f;
		float branchLength = 0.6f;
		float branchRadius = 0.04f;
		float lengthShrink = 0.8f;
		float radiusShrink = 0.79f;
		float shrinkJitterMin = 0.8f;
		float shrinkJitterMax = 1.1f;
		float paletteJitter = 0.03f;
		float terminateChance = 0.02f;
		int levelsDeep = 0;
		int maxLevels = 10;
		// int maxLevels = 8;
		int numCopies = 30;
		vec3 basePoint = vec3( 0.0f, 0.0f, -1.0f );
		mat3 basis = mat3( 1.0f );
	};

	recursiveTreeConfig myConfig;
	rng rotateJitter = rng( myConfig.rotateJitterMin, myConfig.rotateJitterMax );
	rng shrinkJitter = rng( myConfig.shrinkJitterMin, myConfig.shrinkJitterMax );
	rng branchJitter = rng( myConfig.branchJitterMin, myConfig.branchJitterMax );
	rng paletteJitter = rng( -myConfig.paletteJitter, myConfig.paletteJitter );
	rng terminator = rng( 0.0f, 1.0f );

	void TreeRecurse ( recursiveTreeConfig config ) {
		vec3 basePointNext = config.basePoint + config.branchLength * config.basis * vec3( 0.0f, 0.0f, 1.0f );
		vec3 color = palette::paletteRef( RemapRange( float( config.levelsDeep ) / float( config.maxLevels ) + paletteJitter(), 0.0f, 1.0f, ChorizoConfig.treePaletteMin, ChorizoConfig.treePaletteMax ) );
		ChorizoConfig.geometryManager.AddCapsule( config.basePoint, basePointNext, config.branchRadius, color );
		ChorizoConfig.geometryManager.AddPointSpriteSphere( basePointNext, config.branchRadius * 1.5f, color );
		config.basePoint = basePointNext;
		if ( config.levelsDeep == config.maxLevels || terminator() < config.terminateChance ) {
			return;
		} else {
			config.levelsDeep++;
			config.branchRadius = config.branchRadius * ( config.radiusShrink * shrinkJitter() );
			config.branchLength = config.branchLength * ( config.lengthShrink * shrinkJitter() );
			vec3 xBasis = config.basis * vec3( 1.0f, 0.0f, 0.0f );
			vec3 zBasis = config.basis * vec3( 0.0f, 0.0f, 1.0f );
			config.basis = mat3( glm::rotate( config.branchTilt + branchJitter(), xBasis ) ) * config.basis;
			const float rotateIncrement = 6.28f / float( config.numBranches ); // 2 pi in a full rotation
			for ( int i = 0; i < config.numBranches; i++ ) {
				TreeRecurse( config );
				config.basis = mat3( glm::rotate( rotateIncrement + rotateJitter(), zBasis ) ) * config.basis;
			}
		}
	}

	void AddCurrentSoftbodyPoints() {
		const vec3 scale = vec3( 1.0f, -1.0f, 1.0f );
		// rng colorGenerator = rng( 0.0f, 1.0f );
		const bool tensionColor = true;
		for ( auto& e : simulationModel.edges ) {
			float radius = 0.0f;
			switch ( e.type ) {
				case SUSPENSION_INBOARD:
					radius = 0.015f;
				break;
				case SUSPENSION:
					radius = 0.015f;
				break;
				case CHASSIS:
					radius = 0.015f;
				break;
				default: break;
			}
			vec3 color = vec3( 0.0f );
			const vec3 point1 = simulationModel.nodes[ e.node1 ].position * scale;
			const vec3 point2 = simulationModel.nodes[ e.node2 ].position * scale;
			if ( tensionColor == true ) {
				const vec3 blue = vec3( 0.0f, 0.0f, 1.0f );
				const vec3 red = vec3( 1.0f, 0.0f, 0.0f );
				const vec3 white = vec3( 1.0f );
				const float currentLength = glm::distance( point1, point2 );
				const float ratio = glm::clamp( RemapRange( currentLength, 0.0f, e.baseLength, 0.0f, 1.0f ), 0.0f, 2.0f );
				// lerp to white at base length
				if ( ratio > 1.0f ) {
					// blue if they're longer ( 1.5x )
					color = glm::mix( white, blue, glm::clamp( RemapRange( ratio, 1.0f, 1.05f, 0.0f, 1.0f ), 0.0f, 1.0f ) );
				} else {
					// red if they're shorter ( 0.5x )
					color = glm::mix( white, red, glm::clamp( RemapRange( ratio, 1.0f, 0.95f, 0.0f, 1.0f ), 0.0f, 1.0f ) );
				}
			// } else {
				color = color * palette::paletteRef( e.type / 5.0f + 0.1f );
			}
			ChorizoConfig.geometryManager.AddCapsule( point1, point2, radius, color );
		}

		for ( auto& n : simulationModel.nodes ) {
			if ( n.anchored ) {
				ChorizoConfig.geometryManager.AddPointSpriteSphere( n.position * scale,
					0.1f, palette::paletteRef( 0.5f ) );
			// } else { // chassis nodes
			// 	ChorizoConfig.geometryManager.AddPointSpriteSphere( n.position * scale,
			// 		0.02f, palette::paletteRef( 0.5f ) );
			}
		}

		// the ground nodes - this could be done more efficiently, but whatever
		for ( float x = -1.0; x < 1.0f; x += 0.01f ) {
			for ( float y = -1.5f; y < 1.5f; y += 0.01f ) {
				const float scaledX = x / simulationModel.scale;
				const float scaledY = y / simulationModel.scale;
				// const float groundHeight = -simulationModel.getGroundPoint( scaledX, scaledY ) * simulationModel.scale;
				const float groundHeight = -simulationModel.getGroundPoint( scaledX, scaledY ) + 0.5f;
				ChorizoConfig.geometryManager.AddPointSpriteSphere( vec3( scaledX, groundHeight, scaledY ), 0.04f, palette::paletteRef( 0.5f ) );
			}
		}
	}

	void AddLights() {
		// add some point sprite spheres to indicate light positions
		palette::PaletteIndex = ChorizoConfig.lightPaletteID;
		rng dist = rng( ChorizoConfig.lightPaletteMin, ChorizoConfig.lightPaletteMax );
		rng radius = rng( 0.01f, 0.1f );

		const int numLightsPerSide = 4;
		const int w = 512;
		const int h = 512;
		const vec2 scale = vec2( 4.0f );

		for ( int x = 1; x <= w; x += w / ( numLightsPerSide ) ) {
			for ( int y = 1; y <= h; y += h / numLightsPerSide ) {

				// const float heightValue = -p.model.GetAtXY( x - 1, y - 1 )[ 0 ];

				vec3 position = vec3(
					( float( x - 1 ) / float( w ) - 0.5f ) * scale.x,
					( float( y - 1 ) / float( h ) - 0.5f ) * scale.y, -1.0f ).yzx();
				// vec3 position = vec3( ( float( x - 1 ) / float( w ) - 0.5f ) * scale.x, ( float( y - 1 ) / float( h ) - 0.5f ) * scale.y, ( heightValue * heightScale - 0.5f ) * 10.0f + 0.2f );
				vec3 color = palette::paletteRef( dist() );

				// const float r = -radius();
				const float r = -0.013f;
				const vec3 grey = vec3( 0.618f );
				const vec3 offset = vec3( 0.0f, 0.0f, 0.8f * r );
				ChorizoConfig.geometryManager.AddPointSpriteSphere( position, r, color );
				// ChorizoConfig.geometryManager.AddCapsule( position + offset, position + 10.0f * offset, -r * 0.618f, grey );

				ChorizoConfig.lights.push_back( vec4( position, 0.0f ) );
				ChorizoConfig.lights.push_back( vec4( color / 3.0f, 0.0f ) );
			}
		}
	}

	void PrepSSBOs () {
		static bool firstTime = true;
		
		ChorizoConfig.numPrimitives = ChorizoConfig.geometryManager.count;
		// cout << newline << "Created " << ChorizoConfig.numPrimitives << " primitives" << newline;

		// create the transforms buffer
		if ( firstTime ) {
			glGenBuffers( 1, &ChorizoConfig.boundsTransformBuffer );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.boundsTransformBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( mat4 ) * ChorizoConfig.numPrimitives, NULL, GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, ChorizoConfig.boundsTransformBuffer );

		// shape parameterization buffer
		if ( firstTime ) {
			glGenBuffers( 1, &ChorizoConfig.shapeParametersBuffer );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.shapeParametersBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * ChorizoConfig.numPrimitives * 4, ( GLvoid * ) ChorizoConfig.geometryManager.parametersList.data(), GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, ChorizoConfig.shapeParametersBuffer );

		ChorizoConfig.numPointSprites = ChorizoConfig.geometryManager.countPointSprite;
		// cout << newline << "Created " << ChorizoConfig.numPointSprites << " point sprites" << newline;

		// point sprite spheres, separate from the bounding box impostors
		if ( firstTime ) {
			glGenBuffers( 1, &ChorizoConfig.pointSpriteParametersBuffer );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.pointSpriteParametersBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * ChorizoConfig.numPointSprites * 4, ( GLvoid * ) ChorizoConfig.geometryManager.pointSpriteParametersList.data(), GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, ChorizoConfig.pointSpriteParametersBuffer );

		// setup the SSBO, with the initial data
		ChorizoConfig.numLights = ChorizoConfig.lights.size() / 2;
		if ( firstTime ) {
			glGenBuffers( 1, &ChorizoConfig.lightsBuffer );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.lightsBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * ChorizoConfig.numLights * 2, ( GLvoid * ) ChorizoConfig.lights.data(), GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, ChorizoConfig.lightsBuffer );

		firstTime = false;
	}

	vec2 UniformSampleHexagon ( vec2 u ) {
		u = 2.0f * u - vec2( 1.0f );
		float a = sqrt( 3.0f ) - sqrt( 3.0f - 2.25f * abs( u.x ) );
		return vec2( glm::sign( u.x ) * a, u.y * ( 1.0f - a / sqrt( 3.0f ) ) );
	}

	void PrepSceneParameters () {
		// const float time = SDL_GetTicks() / 10000.0f;

		static rng jitterAmount = rng( 0.0f, 1.0f );
		const vec2 pixelJitter = vec2( jitterAmount() - 0.5f, jitterAmount() - 0.5f ) * 0.001f;
		const vec2 hexJitter = UniformSampleHexagon( vec2( jitterAmount(), jitterAmount() ) ) * ChorizoConfig.apertureAdjust;
		const vec3 localEyePos = ChorizoConfig.enableThinLens ? ChorizoConfig.eyePosition +
			( hexJitter.x + pixelJitter.x ) * ChorizoConfig.basisX +
			( hexJitter.y + pixelJitter.y ) * ChorizoConfig.basisY : ChorizoConfig.eyePosition;
		const float aspectRatio = float( config.width ) / float( config.height );

		ChorizoConfig.projTransform = glm::perspective( glm::radians( ChorizoConfig.FoV ), aspectRatio, ChorizoConfig.nearZ, ChorizoConfig.farZ );
		ChorizoConfig.projTransformInverse = glm::inverse( ChorizoConfig.projTransform );

		ChorizoConfig.viewTransform = glm::lookAt( localEyePos, ChorizoConfig.eyePosition + ChorizoConfig.basisZ * ChorizoConfig.focusDistance, ChorizoConfig.basisY );
		ChorizoConfig.viewTransformInverse = glm::inverse( ChorizoConfig.viewTransform );

		ChorizoConfig.combinedTransform = ChorizoConfig.projTransform * ChorizoConfig.viewTransform;
		ChorizoConfig.combinedTransformInverse = glm::inverse( ChorizoConfig.combinedTransform );

	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		const bool * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		const SDL_Keymod k	= SDL_GetModState();
		const bool shift		= ( k & SDL_KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		const bool control	= ( k & SDL_KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		// quaternion based rotation via retained state in the basis vectors
		const float scalar = shift ? 0.1f : ( control ? 0.0005f : 0.02f );
		if ( state[ SDL_SCANCODE_W ] ) {
			glm::quat rot = glm::angleAxis( scalar, ChorizoConfig.basisX ); // basisX is the axis, therefore remains untransformed
			ChorizoConfig.basisY = ( rot * vec4( ChorizoConfig.basisY, 0.0f ) ).xyz();
			ChorizoConfig.basisZ = ( rot * vec4( ChorizoConfig.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_S ] ) {
			glm::quat rot = glm::angleAxis( -scalar, ChorizoConfig.basisX );
			ChorizoConfig.basisY = ( rot * vec4( ChorizoConfig.basisY, 0.0f ) ).xyz();
			ChorizoConfig.basisZ = ( rot * vec4( ChorizoConfig.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_A ] ) {
			glm::quat rot = glm::angleAxis( scalar, ChorizoConfig.basisY ); // same as above, but basisY is the axis
			ChorizoConfig.basisX = ( rot * vec4( ChorizoConfig.basisX, 0.0f ) ).xyz();
			ChorizoConfig.basisZ = ( rot * vec4( ChorizoConfig.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_D ] ) {
			glm::quat rot = glm::angleAxis( -scalar, ChorizoConfig.basisY );
			ChorizoConfig.basisX = ( rot * vec4( ChorizoConfig.basisX, 0.0f ) ).xyz();
			ChorizoConfig.basisZ = ( rot * vec4( ChorizoConfig.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_Q ] ) {
			glm::quat rot = glm::angleAxis( scalar, ChorizoConfig.basisZ ); // and again for basisZ
			ChorizoConfig.basisX = ( rot * vec4( ChorizoConfig.basisX, 0.0f ) ).xyz();
			ChorizoConfig.basisY = ( rot * vec4( ChorizoConfig.basisY, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_E ] ) {
			glm::quat rot = glm::angleAxis( -scalar, ChorizoConfig.basisZ );
			ChorizoConfig.basisX = ( rot * vec4( ChorizoConfig.basisX, 0.0f ) ).xyz();
			ChorizoConfig.basisY = ( rot * vec4( ChorizoConfig.basisY, 0.0f ) ).xyz();
		}

		// f to reset basis, shift + f to reset basis and home to origin
		if ( state[ SDL_SCANCODE_F ] ) {
			if ( shift ) ChorizoConfig.eyePosition = vec3( 0.0f, 0.0f, 0.0f );
			ChorizoConfig.basisX = vec3( 1.0f, 0.0f, 0.0f );
			ChorizoConfig.basisY = vec3( 0.0f, 1.0f, 0.0f );
			ChorizoConfig.basisZ = vec3( 0.0f, 0.0f, 1.0f );
		}
		if ( state[ SDL_SCANCODE_UP ] )			ChorizoConfig.eyePosition += scalar * ChorizoConfig.basisZ;
		if ( state[ SDL_SCANCODE_DOWN ] )		ChorizoConfig.eyePosition -= scalar * ChorizoConfig.basisZ;
		if ( state[ SDL_SCANCODE_RIGHT ] )		ChorizoConfig.eyePosition -= scalar * ChorizoConfig.basisX;
		if ( state[ SDL_SCANCODE_LEFT ] )		ChorizoConfig.eyePosition += scalar * ChorizoConfig.basisX;
		if ( state[ SDL_SCANCODE_PAGEDOWN ] )	ChorizoConfig.eyePosition += scalar * ChorizoConfig.basisY;
		if ( state[ SDL_SCANCODE_PAGEUP ] )		ChorizoConfig.eyePosition -= scalar * ChorizoConfig.basisY;

	}

	void ImGuiDrawPalette ( int &palette, string sublabel, float min = 0.0f, float max = 1.0f ) {
		static std::vector< const char * > paletteLabels;
		if ( paletteLabels.size() == 0 ) {
			for ( auto& entry : palette::paletteListLocal ) {
				// copy to a cstr for use by imgui
				char * d = new char[ entry.label.length() + 1 ];
				std::copy( entry.label.begin(), entry.label.end(), d );
				d[ entry.label.length() ] = '\0';
				paletteLabels.push_back( d );
			}
		}

		ImGui::Combo( ( string( "Palette##" ) + sublabel ).c_str(), &palette, paletteLabels.data(), paletteLabels.size() );
		const size_t paletteSize = palette::paletteListLocal[ palette ].colors.size();
		ImGui::Text( "  Contains %.3lu colors:", palette::paletteListLocal[ palette::PaletteIndex ].colors.size() );
		// handle max < min
		float minVal = min;
		float maxVal = max;
		float realSelectedMin = std::min( minVal, maxVal );
		float realSelectedMax = std::max( minVal, maxVal );
		size_t minShownIdx = std::floor( realSelectedMin * ( paletteSize - 1 ) );
		size_t maxShownIdx = std::ceil( realSelectedMax * ( paletteSize - 1 ) );

		bool finished = false;
		for ( int y = 0; y < 8; y++ ) {
			if ( !finished ) {
				ImGui::Text( " " );
			}

			for ( int x = 0; x < 32; x++ ) {

				// terminate when you run out of colors
				const uint32_t index = x + 32 * y;
				if ( index >= paletteSize ) {
					finished = true;
					// goto terminate;
				}

				// show color, or black if past the end of the list
				ivec4 color = ivec4( 0 );
				if ( !finished ) {
					color = ivec4( palette::paletteListLocal[ palette ].colors[ index ], 255 );
					// determine if it is in the active range
					if ( index < minShownIdx || index > maxShownIdx ) {
						color.a = 64; // dim inactive entries
					}
				} 
				if ( color.a != 0 ) {
					ImGui::SameLine();
					ImGui::TextColored( ImVec4( color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f ), "@" );
				}
			}
		}
		// terminate:
	}

	void ImguiPass () {
		ZoneScoped;

		// if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
		// if ( tonemap.showTonemapWindow ) TonemapControlsWindow();
		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		TonemapControlsWindow();

		ImGui::Begin( "Chorizo", NULL, ImGuiWindowFlags_NoScrollWithMouse );

		ImGui::SeparatorText( "Current Totals" );
		ImGui::Indent();
		ImGui::Text( "Bounding Box Impostors: %d", ChorizoConfig.geometryManager.count );
		ImGui::Text( "Point Sprite Impostors: %d", ChorizoConfig.geometryManager.countPointSprite );
		ImGui::Unindent();

		ImGui::SeparatorText( "Controls" );
		ImGui::SliderFloat( "FoV", &ChorizoConfig.FoV, 3.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::Checkbox( "Enable DoF Jitter", &ChorizoConfig.enableThinLens );
		ImGui::SliderFloat( "Blend Amount", &ChorizoConfig.blendAmount, 0.0001f, 0.5f, "%.7f", ImGuiSliderFlags_Logarithmic );
		ImGui::Checkbox( "Progressive Blend", &ChorizoConfig.progressiveBlend );
		if ( ChorizoConfig.progressiveBlend == true ) {
			ChorizoConfig.progressiveFrameCount++;
			ImGui::SameLine();
			if ( ImGui::Button( "Reset" ) ) {
				ChorizoConfig.progressiveFrameCount = 0;
			}
		}
		ImGui::SliderFloat( "Focus Adjust", &ChorizoConfig.focusDistance, 0.01f, 50.0f );
		ImGui::SliderFloat( "Aperture Adjust", &ChorizoConfig.apertureAdjust, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderFloat( "Volumetric Strength", &ChorizoConfig.volumetricStrength, 0.0f, 0.1f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::ColorEdit3( "Volumetric Color", ( float * ) &ChorizoConfig.volumetricColor, ImGuiColorEditFlags_PickerHueWheel );

		ImGui::Text( " " );

		ImGui::SeparatorText( "Generator" );

		if ( ImGui::CollapsingHeader( "Trees" ) ) {

			const int64_t count = myConfig.numCopies * intPow( myConfig.numBranches, myConfig.maxLevels );
			ImGui::Text( "Estimated Number of Primitives: %ld", count );

			ImGui::SliderInt( "Num Copies", &myConfig.numCopies, 1, 10 );
			ImGui::SliderInt( "Max Levels Deep", &myConfig.maxLevels, 1, 15 );
			ImGui::SliderInt( "Num Branches", &myConfig.numBranches, 1, 6 );
			ImGui::SliderFloat( "Terminate Chance", &myConfig.terminateChance, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Rotate Jitter Min", &myConfig.rotateJitterMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Rotate Jitter Max", &myConfig.rotateJitterMax, 0.0f, 1.0f );
			ImGui::SliderFloat( "Branch Tilt", &myConfig.branchTilt, 0.0f, 1.0f );
			ImGui::SliderFloat( "Branch Jitter Min", &myConfig.branchJitterMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Branch Jitter Max", &myConfig.branchJitterMax, 0.0f, 1.0f );
			ImGui::SliderFloat( "Branch Length", &myConfig.branchLength, 0.0f, 1.0f );
			ImGui::SliderFloat( "Length Shrink", &myConfig.lengthShrink, 0.5f, 1.0f );
			ImGui::SliderFloat( "Branch Radius", &myConfig.branchRadius, 0.0f, 0.1f );
			ImGui::SliderFloat( "Radius Shrink", &myConfig.radiusShrink, 0.5f, 1.0f );
			ImGui::SliderFloat( "Shrink Jitter Min", &myConfig.shrinkJitterMin, 0.0f, 2.0f );
			ImGui::SliderFloat( "Shrink Jitter Max", &myConfig.shrinkJitterMax, 0.0f, 2.0f );
			ImGui::SliderFloat( "Palette Jitter", &myConfig.paletteJitter, 0.0f, 0.1f );
			ImGui::SliderFloat( "Base Point X", &myConfig.basePoint.x, -1.0f, 1.0f );
			ImGui::SliderFloat( "Base Point Y", &myConfig.basePoint.y, -1.0f, 1.0f );
			ImGui::SliderFloat( "Base Point Z", &myConfig.basePoint.z, -1.0f, 1.0f );
			if ( ImGui::Button( "Randomize Parameters" ) ) {

				do {
					rngi branches = rngi( 1, 6 );
					myConfig.numBranches = branches();

					rng rotateJitter = rng( 0.0f, 0.75f );
					myConfig.rotateJitterMin = rotateJitter();
					myConfig.rotateJitterMax = rotateJitter();
					myConfig.branchJitterMin = rotateJitter();
					myConfig.branchJitterMax = rotateJitter();

					rng tilt = rng( 0.03f, 0.75f );
					myConfig.branchTilt = tilt();

					rng length = rng( 0.2f, 1.3f );
					myConfig.branchLength = length();

					rng radius = rng( 0.01f, 0.2f );
					myConfig.branchRadius = radius();

					rng shrink = rng( 0.5f, 1.0f );
					myConfig.lengthShrink = shrink();
					myConfig.radiusShrink = shrink();

					rng offset = rng( 0.75f, 1.25f );
					myConfig.shrinkJitterMin = offset();
					myConfig.shrinkJitterMax = offset();
				} while( ( myConfig.numCopies * intPow( myConfig.numBranches, myConfig.maxLevels ) ) > 10000000 );
			}
		}

		if ( ImGui::CollapsingHeader( "Colors" ) ) {
			static uint32_t colorCap = 256;
			ImGui::SliderInt( "Random Select Color Count Cap", ( int* ) &colorCap, 2, 256 );
			ImGui::Text( " " );
			ImGui::Text( " " );

			ImGui::SeparatorText( "Trees" );
			ImGui::Text( " " );
			ImGui::Indent();
			ImGuiDrawPalette( ChorizoConfig.treePaletteID, "trees", ChorizoConfig.treePaletteMin, ChorizoConfig.treePaletteMax );
			if ( ImGui::Button( "Random##trees" ) ) {
				rngi pick = rngi( 0, palette::paletteListLocal.size() - 1 );
				do {
					ChorizoConfig.treePaletteID = pick();
				} while ( palette::paletteListLocal[ ChorizoConfig.treePaletteID ].colors.size() > colorCap );
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Reverse##trees" ) ) {
				std::swap( ChorizoConfig.treePaletteMin, ChorizoConfig.treePaletteMax );
			}
			ImGui::SliderFloat( "Palette Min##trees", &ChorizoConfig.treePaletteMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Palette Max##trees", &ChorizoConfig.treePaletteMax, 0.0f, 1.0f );
			ImGui::Unindent();



			ImGui::SeparatorText( "Grass" );
			ImGui::Text( " " );
			ImGui::Indent();
			ImGuiDrawPalette( ChorizoConfig.grassPaletteID, "grass", ChorizoConfig.grassPaletteMin, ChorizoConfig.grassPaletteMax );
			if ( ImGui::Button( "Random##grass" ) ) {
				rngi pick = rngi( 0, palette::paletteListLocal.size() - 1 );
				do {
					ChorizoConfig.grassPaletteID = pick();
				} while ( palette::paletteListLocal[ ChorizoConfig.grassPaletteID ].colors.size() > colorCap );
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Reverse##grass" ) ) {
				std::swap( ChorizoConfig.grassPaletteMin, ChorizoConfig.grassPaletteMax );
			}
			ImGui::SliderFloat( "Palette Min##grass", &ChorizoConfig.grassPaletteMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Palette Max##grass", &ChorizoConfig.grassPaletteMax, 0.0f, 1.0f );
			ImGui::Unindent();



			ImGui::SeparatorText( "Ground" );
			ImGui::Text( " " );
			ImGui::Indent();
			ImGuiDrawPalette( ChorizoConfig.groundPaletteID, "ground", ChorizoConfig.groundPaletteMin, ChorizoConfig.groundPaletteMax );
			if ( ImGui::Button( "Random##ground" ) ) {
				rngi pick = rngi( 0, palette::paletteListLocal.size() - 1 );
				do {
					ChorizoConfig.groundPaletteID = pick();
				} while ( palette::paletteListLocal[ ChorizoConfig.groundPaletteID ].colors.size() > colorCap );
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Reverse##ground" ) ) {
				std::swap( ChorizoConfig.groundPaletteMin, ChorizoConfig.groundPaletteMax );
			}
			ImGui::SliderFloat( "Palette Min##ground", &ChorizoConfig.groundPaletteMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Palette Max##ground", &ChorizoConfig.groundPaletteMax, 0.0f, 1.0f );
			ImGui::Unindent();



			ImGui::SeparatorText( "Lights" );
			ImGui::Text( " " );
			ImGui::Indent();
			ImGuiDrawPalette( ChorizoConfig.lightPaletteID, "lights", ChorizoConfig.lightPaletteMin, ChorizoConfig.lightPaletteMax );
			if ( ImGui::Button( "Random##lights" ) ) {
				rngi pick = rngi( 0, palette::paletteListLocal.size() - 1 );
				do {
					ChorizoConfig.lightPaletteID = pick();
				} while ( palette::paletteListLocal[ ChorizoConfig.lightPaletteID ].colors.size() > colorCap );
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Reverse##lights" ) ) {
				std::swap( ChorizoConfig.lightPaletteMin, ChorizoConfig.lightPaletteMax );
			}
			ImGui::SliderFloat( "Palette Min##lights", &ChorizoConfig.lightPaletteMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Palette Max##lights", &ChorizoConfig.lightPaletteMax, 0.0f, 1.0f );
			ImGui::Unindent();

		}

		ImGui::End();

		// controls for the physics sim
		ImGui::Begin( "Model Config", NULL, 0 );
		if ( ImGui::BeginTabBar( "Config Sections", ImGuiTabBarFlags_None ) ) {
			ImGui::SameLine();
			if ( ImGui::BeginTabItem( "Simulation" ) ) {
				ImGui::SliderFloat( "Time Scale", &simulationModel.simParameters.timeScale, 0.0f, 0.01f );
				ImGui::SliderFloat( "Gravity", &simulationModel.simParameters.gravity, -10.0f, 10.0f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Noise Amplitude", &simulationModel.simParameters.noiseAmplitudeScale, 0.0f, 0.45f );
				ImGui::SliderFloat( "Noise Speed", &simulationModel.simParameters.noiseSpeed, 0.0f, 10.0f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Chassis Node Mass", &simulationModel.simParameters.chassisNodeMass, 0.1f, 10.0f );
				ImGui::SliderFloat( "Chassis K", &simulationModel.simParameters.chassisKConstant, 0.0f, 15000.0f );
				ImGui::SliderFloat( "Chassis Damping", &simulationModel.simParameters.chassisDamping, 0.0f, 100.0f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Suspension K", &simulationModel.simParameters.suspensionKConstant, 0.0f, 15000.0f );
				ImGui::SliderFloat( "Suspension Damping", &simulationModel.simParameters.suspensionDamping, 0.0f, 100.0f );
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Render" ) ) {
				ImGui::Text("Geometry Toggles");
				ImGui::Separator();
				// ImGui::Checkbox( "Draw Chassis Edges", &simulationModel.displayParameters.showChassisEdges );
				// ImGui::Checkbox( "Draw Chassis Nodes", &simulationModel.displayParameters.showChassisNodes );
				// ImGui::Checkbox( "Draw Suspension Edges", &simulationModel.displayParameters.showSuspensionEdges );
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void DrawAPIGeometry () {
		// ZoneScoped; scopedTimer Start( "API Geometry" );

		{	ZoneScoped; scopedTimer Start( "Bounding Box Compute" );
			// prepare the bounding boxes
			GLuint shader = shaders[ "Bounds" ];
			glUseProgram( shader );
			const uint32_t workgroupsRoundedUp = ( ChorizoConfig.numPrimitives + 63 ) / 64;
			glDispatchCompute( workgroupsRoundedUp, 1, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{	ZoneScoped; scopedTimer Start( "Bounding Box Impostors" );
			// then draw, using the prepared data
			RasterGeoDataSetup();
			glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ ( ChorizoConfig.frameCount++ % 2 ) ] );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glViewport( 0, 0, config.width, config.height );
			glDrawArrays( GL_TRIANGLES, 0, 36 * ChorizoConfig.numPrimitives );
		}

		// draw the point sprites, as well
		{	ZoneScoped; scopedTimer Start( "Point Sprite Impostors" );
			RasterGeoDataSetupPointSprite();
			glDrawArrays( GL_POINTS, 0, ChorizoConfig.numPointSprites );
		}

		// revert to default framebuffer
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	void RasterGeoDataSetup () {
		const GLuint shader = shaders[ "Bounding Box" ];
		glUseProgram( shader );
		glBindVertexArray( ChorizoConfig.vao );

		glUniform1f( glGetUniformLocation( shader, "numPrimitives" ), ChorizoConfig.numPrimitives );
		glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransform ) );
		glUniform3fv( glGetUniformLocation( shader, "eyePosition" ), 1, glm::value_ptr( ChorizoConfig.eyePosition ) );
	}

	void RasterGeoDataSetupPointSprite () {
		const GLuint shader = shaders[ "Point Sprite" ];
		glUseProgram( shader );

		glUniform2f( glGetUniformLocation( shader, "viewportSize" ), config.width, config.height );
		glUniform1f( glGetUniformLocation( shader, "numPrimitives" ), ChorizoConfig.numPointSprites );
		glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransform ) );
		glUniformMatrix4fv( glGetUniformLocation( shader, "invViewTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.viewTransformInverse ) );
		glUniformMatrix4fv( glGetUniformLocation( shader, "projTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.projTransform ) );
		glUniform3fv( glGetUniformLocation( shader, "eyePosition" ), 1, glm::value_ptr( ChorizoConfig.eyePosition ) );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // doing the deferred processing from rasterizer results to rendered result
			scopedTimer Start( "Drawing" );
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			string index = ChorizoConfig.frameCount % 2 ? "0" : "1";
			string indexBack = ChorizoConfig.frameCount % 2 ? "1" : "0";
			textureManager.BindImageForShader( "Blue Noise", "blueNoiseTexture", shader, 0 );
			textureManager.BindImageForShader( "Accumulator", "accumulatorTexture", shader, 1 );
			textureManager.BindTexForShader( "Framebuffer Depth " + index, "depthTex", shader, 2 );
			textureManager.BindTexForShader( "Framebuffer Depth " + indexBack, "depthTexBack", shader, 3 );
			textureManager.BindTexForShader( "Framebuffer Normal " + index, "normals", shader, 4 );
			textureManager.BindTexForShader( "Framebuffer Normal " + indexBack, "normalsBack", shader, 5 );
			textureManager.BindTexForShader( "Framebuffer Primitive ID " + index, "primitiveID", shader, 6 );
			textureManager.BindTexForShader( "Framebuffer Primitive ID " + indexBack, "primitiveIDBack", shader, 7 );

			glUniformMatrix4fv( glGetUniformLocation( shader, "combinedTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransform ) );
			glUniformMatrix4fv( glGetUniformLocation( shader, "combinedTransformInverse" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransformInverse ) );

			glUniform1i( glGetUniformLocation( shader, "inSeed" ), ChorizoConfig.wangSeeder() );
			glUniform1f( glGetUniformLocation( shader, "nearZ" ), ChorizoConfig.nearZ );
			glUniform1f( glGetUniformLocation( shader, "farZ" ), ChorizoConfig.farZ );

			if ( ChorizoConfig.progressiveBlend == true ) {
				glUniform1f( glGetUniformLocation( shader, "blendAmount" ), 1.0f / ( ChorizoConfig.progressiveFrameCount + 1.0f ) );
			} else {
				glUniform1f( glGetUniformLocation( shader, "blendAmount" ), ChorizoConfig.blendAmount );
			}
			
			glUniform1f( glGetUniformLocation( shader, "volumetricStrength" ), ChorizoConfig.volumetricStrength );
			glUniform3fv( glGetUniformLocation( shader, "volumetricColor" ), 1, glm::value_ptr( ChorizoConfig.volumetricColor ) );
			glUniform1i( glGetUniformLocation( shader, "numLights" ), ChorizoConfig.numLights );
			glUniform3fv( glGetUniformLocation( shader, "eyePosition" ), 1, glm::value_ptr( ChorizoConfig.eyePosition ) );
			glUniform3fv( glGetUniformLocation( shader, "ambientColor" ), 1, glm::value_ptr( ChorizoConfig.ambientColor ) );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			bindSets[ "Postprocessing" ].apply();
			glUseProgram( shaders[ "Tonemap" ] );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// const GLuint shader = shaders[ "Animate" ];
		// glUseProgram( shader );
		// glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 3000.0f );
		// const uint workgroupsRoundedUp = ( ChorizoConfig.numPrimitives + 63 ) / 64;
		// glDispatchCompute( 64, workgroupsRoundedUp / 64, 1 );
		// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		simulationModel.Update( 3 );
		ChorizoConfig.geometryManager.ClearLists();
		AddCurrentSoftbodyPoints();
		PrepSSBOs();
	}

	void OnRender () {
		ZoneScoped;
		PrepSceneParameters();
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		DrawAPIGeometry();			// draw the cubes on top, into the ping-ponging framebuffer
		ComputePasses();			// multistage update of displayTexture
		BlitToScreen();				// fullscreen triangle copying to the screen
		{
			scopedTimer Start( "ImGUI Pass" );
			ImguiFrameStart();		// start the imgui frame
			ImguiPass();			// do all the gui stuff
			ImguiFrameEnd();		// finish imgui frame and put it in the framebuffer
		}
		window.Swap();				// show what has just been drawn to the back buffer
	}

	bool MainLoop () { // this is what's called from the loop in main
		ZoneScoped;

		// event handling
		HandleTridentEvents();
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};
