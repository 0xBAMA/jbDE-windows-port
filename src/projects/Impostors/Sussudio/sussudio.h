#include "../../../engine/includes.h"

#define CAPSULE		0
#define CAPPEDCONE	1
#define ROUNDEDBOX	2
#define ELLIPSOID	3

// ==== Geometry Manager ======================================================================================================
// a very basic set of primitives are supported, Chorizo style rasterizing compute-fit bounding box -> raytrace in fragment shader
struct geometryManager_t {

	// ========================================================================================================================
	//	capsule is the surface "radius" distance away from the line segment between "pointA" and "pointB"
	//		parameters are:
	//			pointA, pointB as vec3's
	//			radius as float
	//		total of 7 floats ( 56 bytes ) + 4 floats ( 16 bytes ) for RGBA color
	void AddCapsule ( const vec3 pointA, const vec3 pointB, const float radius, const vec4 color ) {
		// add data for a capsule
		float parameters[ 16 ] = {
			CAPSULE, pointA.x, pointA.y, pointA.z,
			pointB.x, pointB.y, pointB.z, radius,
			0.0f, 0.0f, 0.0f, 0.0f,
			color.r, color.g, color.b, color.a
		};
		AddPrimitive( parameters );
	}

	// ========================================================================================================================
	//	"capped cone" is a superset of sphere + capsule, can easily use it for a lot of segment-type uses... bounding boxes for capsule is somewhat wasteful
	//		parameters are:
	//			point A, point B as vec3's
	//			radius A, radius B, as floats
	//		total of 8 floats ( 64 bytes ) + 4 floats ( 16 bytes ) for RGBA color
	void AddCappedCone ( const vec3 pointA, const vec3 pointB, const float radiusA, const float radiusB, const vec4 color ) {
		// add data for a capped cone
		float parameters[ 16 ] = {
			CAPPEDCONE, pointA.x, pointA.y, pointA.z,
			pointB.x, pointB.y, pointB.z, radiusA,
			radiusB, 0.0f, 0.0f, 0.0f,
			color.r, color.g, color.b, color.a
		};
		AddPrimitive( parameters );
	}

	// ========================================================================================================================
	//	"rounded box" is a box with a configurable bevel around the edges, from sharp to possibly basically spherical
	//		parameters are:
	//			center point as vec3
	//			scale factors as vec3
	//			euler angle rotation... Chorizo has this packed as a single float but I think that's unneccesary
	//			rounding factor for the edges as a float
	//		total of 10 floats ( 80 bytes ) + 4 floats ( 16 bytes ) for RGBA color
	void AddRoundedBox ( const vec3 center, const vec3 scale, const vec3 rotation, const float rounding, const vec4 color ) {
		// add data for a rounded box
		float parameters[ 16 ] = {
			ROUNDEDBOX, center.x, center.y, center.z,
			scale.x, scale.y, scale.z, rotation.x,
			rotation.y, rotation.z, rounding, 0.0f,
			color.r, color.g, color.b, color.a
		};
		AddPrimitive( parameters );
	}

	// ========================================================================================================================
	//	"ellipsoid" is a 3d ellipsoid shape - can be spherical but also easy to bias into a rounded needle type of primitive
	//		parameters are:
	//			center point as vec3
	//			radii as vec3
	//			rotation as vec3 ( euler angles )
	//		total of 9 floats ( 72 bytes ) + 4 floats ( 16 bytes ) for RGBA color
	void AddEllipsoid ( const vec3 center, const vec3 radii, const vec3 rotation, const vec4 color ) {
		// add data for an ellipsoid
		float parameters[ 16 ] = {
			ELLIPSOID, center.x, center.y, center.z,
			radii.x, radii.y, radii.z, rotation.x,
			rotation.y, rotation.z, 0.0f, 0.0f,
			color.r, color.g, color.b, color.a
		};
		AddPrimitive( parameters );
	}

	// ========================================================================================================================
	// can keep Chorizo's design of 16 floats ( 128 bytes ) per primitive because nothing needs more than that
	//	Not going to be supporting alpha here, I don't think... maybe dither-type... tbd
	void AddPrimitive ( const float parameters[ 16 ] ) {
		count++;
		parametersList.reserve( 16 * count );
		for ( int i = 0; i < 16; i++ ) {
			parametersList.push_back( parameters[ i ] );
		}
	}

	void Clear () {
		parametersList.resize( 0 );
		count = 0;
	}

	// containing sets of 16 floats describing each primitive
	std::vector< float > parametersList;
	int count;
};

// ============================================================================================================================
struct atlasRendererConfig_t {
	int numViewsX = 9;
	int numViewsY = 9;

	int resolution = 256;
};

// ============================================================================================================================
struct atlasRenderer_t {

	// mostly for sizing the framebuffers right now
	atlasRendererConfig_t atlasRenderConfig;

	// raster target (depth + color attachments)
	GLuint targetFramebuffer; // todo - useful for batch generating atlas info as needed

	// local pointer to the texture manager
	textureManager_t * textureManager;

	// containing list of potentially many primitives that will be used for generating the atlas views
	geometryManager_t geometryManager;

	// OpenGL stuff
	GLuint vao;
	GLuint vbo;

	// for rendering the primitives in the geometry manager
	GLuint bboxComputeShader; // precomputing the bounding boxes
	GLuint bboxRasterShader; // rasterizing then raytracing

	// buffers used to generate the atlas views
	GLuint primitiveGeometryBuffer; // A: 16 floats describing each primitive (directly from geometryManager)
	GLuint bboxTransformsPrecomputed; // B: 4x4 matrix computed by bboxComputeShader from data in primitiveGeometryBuffer

	// todo: organization pass on all this
	glm::mat4 viewTransform = glm::mat4( 1.0f );

	// how are we supporting ortho?
	vec3 eyePosition = vec3( 0.0f, 0.0f, -1.0f );

	atlasRenderer_t () {}

	void AddGeometry () {
		// remove any existing primitives
		geometryManager.Clear();

		// give the geometryManager a set of primitives
		//	I'd like to do some simple L system stuff

		// distribute a random set of N primitives... need to test some ranges
		bool randomDistribution = true;
		if ( randomDistribution ) {

			palette::PickRandomPalette( true );

			rngN position = rngN( 0.0f, 0.25f );
			rng sizeD = rng( 0.01f, 0.1f );
			rngN color = rngN( 0.5f, 0.2f );

			for ( int i = 0; i < 10; i++ ) {
				// add some capped cone primitives
				geometryManager.AddCappedCone(
					vec3( position(), position(), position() ),
					vec3( position(), position(), position() ),
					sizeD(), sizeD(), vec4( palette::paletteRef( color() ), 1.0f ) );

				// add some rounded box primitives
				geometryManager.AddRoundedBox(
					vec3( position(), position(), position() ),
					vec3( sizeD(), sizeD(), sizeD() ),
					vec3( sizeD(), sizeD(), sizeD() ),
					sizeD() / 10.0f, vec4( palette::paletteRef( color() ), 1.0f ) );

				// add some ellipsoid primitives
				geometryManager.AddEllipsoid(
					vec3( position(), position(), position() ),
					vec3( sizeD(), sizeD(), sizeD() ),
					vec3( sizeD(), sizeD(), sizeD() ),
					vec4( palette::paletteRef( color() ), 1.0f ) );
			}
		}

		// then as a last preparation step, iterate through and resize/recenter everything to fit in the -1..1 unit cube
	}

	void PrepSceneParameters() {
	/*
		// const float time = SDL_GetTicks() / 10000.0f;
		static rng jitterAmount = rng( 0.0f, 1.0f );
		const vec2 pixelJitter = vec2( jitterAmount() - 0.5f, jitterAmount() - 0.5f ) * 0.001f;
		const vec3 localEyePos = ChorizoConfig.eyePosition;
		const float aspectRatio = 1.0f; // for the time being, square aspect ratio is fine

		ChorizoConfig.projTransform = glm::perspective( glm::radians( ChorizoConfig.FoV ), aspectRatio, ChorizoConfig.nearZ, ChorizoConfig.farZ );
		ChorizoConfig.projTransformInverse = glm::inverse( ChorizoConfig.projTransform );

		ChorizoConfig.viewTransform = glm::lookAt( localEyePos, ChorizoConfig.eyePosition + ChorizoConfig.basisZ * ChorizoConfig.focusDistance, ChorizoConfig.basisY );
		ChorizoConfig.viewTransformInverse = glm::inverse( ChorizoConfig.viewTransform );

		ChorizoConfig.combinedTransform = ChorizoConfig.projTransform * ChorizoConfig.viewTransform;
		ChorizoConfig.combinedTransformInverse = glm::inverse( ChorizoConfig.combinedTransform );
	*/

	}

	void RenderAtlas () {
		// create the framebuffers at the specified resolution (if they don't exist... problems doing this in the constructor)
		//	this is not crucial for intial testing, but will be moreso after I confirm that I have something on the screen

		// using the current set of geometry in the geometryManager
		// create two buffers, sized based on the number of primitives:
		//	A: primitive geometry (16 floats per)
		//	B: bbox transforms (16 floats per)

		const int numPrimitives = geometryManager.count;

		static bool firstTime = true;
		if ( firstTime ) {
			firstTime = false;
			glGenBuffers( 1, &primitiveGeometryBuffer );
			glGenBuffers( 1, &bboxTransformsPrecomputed );

			glGenVertexArrays( 1, &vao );
			glGenBuffers( 1, &vbo );

			// bbox compute shader ( A -> B precomputing transforms )
			bboxComputeShader = computeShader( "../src/projects/Impostors/Sussudio/shaders/bbox/bboxPrecompute.cs.glsl" ).shaderHandle;

			// bbox raster shaders ( B used during vertex shader, A used again during fragment shader )
			bboxRasterShader = regularShader( "../src/projects/Impostors/Sussudio/shaders/bbox/bboxRaster.vs.glsl",
				"../src/projects/Impostors/Sussudio/shaders/bbox/bboxRaster.fs.glsl" ).shaderHandle;
		}

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, primitiveGeometryBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, numPrimitives * 16 * sizeof( float ), ( GLvoid* ) &geometryManager.parametersList[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, primitiveGeometryBuffer );

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, bboxTransformsPrecomputed );
		glBufferData( GL_SHADER_STORAGE_BUFFER, numPrimitives * 16 * sizeof( float ), NULL, GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, bboxTransformsPrecomputed );

		// vao/vbo binding
		glBindVertexArray( vao );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );

		// other OpenGL config
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
		glFrontFace( GL_CCW );
		glDisable( GL_BLEND );

		// first step is to precompute the bounding box transforms into buffer B (this only runs one time for static geometry usage like this)
		// each rendered view will then rasterize the transformed boxes, and do the ray-primitive testing in the fragment shader
		// the raytrace itself will be referring back to buffer A for the primitive parameters
		GLuint shader = bboxComputeShader;
		glUseProgram( shader );
		const int workgroupsRoundedUp = ( numPrimitives + 63 ) / 64;
		glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );

		// update the atlas views on the framebuffer, one at a time
		//			render the appropriately rotated view with some obnoxious blue noise supersampling in the fragment shader
		shader = bboxRasterShader;
		glUseProgram( shader );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		mat4 baseTransform = glm::mat4( glm::angleAxis( 0.001f * SDL_GetTicks(), vec3( 0.0f, 0.0f, 1.0f ) ) );
		textureManager->BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );
		static rngi noiseOffset = rngi( 0, 512 );
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

		for ( int y = 0; y < atlasRenderConfig.resolution * atlasRenderConfig.numViewsY; y += atlasRenderConfig.resolution ) {

			viewTransform = glm::mat4( glm::angleAxis( ( y / atlasRenderConfig.resolution ) * 6.28f / atlasRenderConfig.numViewsY, vec3( 1.0f, 0.0f, 0.0f ) ) ) * baseTransform;

			for ( int x = 0; x < atlasRenderConfig.resolution * atlasRenderConfig.numViewsX; x += atlasRenderConfig.resolution ) {
			
				viewTransform = glm::mat4( glm::angleAxis( 6.28f / atlasRenderConfig.numViewsX, vec3( 0.0f, 1.0f, 0.0f ) ) ) * viewTransform;

				glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, false, glm::value_ptr( viewTransform ) );
				glUniform3f( glGetUniformLocation( shader, "eyePosition" ), eyePosition.x, eyePosition.y, eyePosition.z );
				glUniform1i( glGetUniformLocation( shader, "numPrimitives" ), numPrimitives );
				glUniform2i( glGetUniformLocation( shader, "viewportBase" ), x, y );
				glUniform2i( glGetUniformLocation( shader, "viewportSize" ), atlasRenderConfig.resolution, atlasRenderConfig.resolution );

				// glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ ( ChorizoConfig.frameCount++ % 2 ) ] );
				glViewport( x, y, atlasRenderConfig.resolution, atlasRenderConfig.resolution );
				glDrawArrays( GL_TRIANGLES, 0, 36 * numPrimitives );
			}
		}
	}
};