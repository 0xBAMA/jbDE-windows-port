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
#define SPHERICAL 0
#define OCTAHEDRAL 1
#define HEMIOCTAHEDRAL 2

struct atlasRendererConfig_t {
	const int numViewsX = 9;
	const int numViewsY = 9;

	const int resolution = 256;

	const int impostorType = SPHERICAL;
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
	GLuint framebuffer;

	// for rendering the primitives in the geometry manager
	GLuint bboxComputeShader; // precomputing the bounding boxes
	GLuint bboxRasterShader; // rasterizing then raytracing

	// buffers used to generate the atlas views
	GLuint primitiveGeometryBuffer; // A: 16 floats describing each primitive (directly from geometryManager)
	GLuint bboxTransformsPrecomputed; // B: 4x4 matrix computed by bboxComputeShader from data in primitiveGeometryBuffer

	// todo: organization pass on all this
	glm::mat4 viewTransform = glm::mat4( 1.0f );

	// init
	atlasRenderer_t () {}

	void CompileShaders () {
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

	void CreateFramebuffers () {
		// == Framebuffer Textures ============
		textureOptions_t opts;
		// ==== Depth =========================
		opts.dataType = GL_DEPTH_COMPONENT32;
		opts.textureType = GL_TEXTURE_2D;
		opts.width = atlasRenderConfig.numViewsX * atlasRenderConfig.resolution;
		opts.height = atlasRenderConfig.numViewsY * atlasRenderConfig.resolution;
		textureManager->Add( "Framebuffer Depth", opts );
		// ==== Primitive ID ==================
		opts.dataType = GL_RG32UI;
		textureManager->Add( "Framebuffer ID", opts );
		// ==== Normal Vector =================
		// opts.dataType = GL_R32UI;
		opts.dataType = GL_RGB32F;
		textureManager->Add( "Framebuffer Normal", opts );
		// ====================================

		// == Framebuffer Objects =============
		glGenFramebuffers( 1, &framebuffer );
		const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 }; // 2x 32-bit primitive ID/instance ID, 2x half float encoded normals

		glBindFramebuffer( GL_FRAMEBUFFER, framebuffer );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager->Get( "Framebuffer Depth" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager->Get( "Framebuffer ID" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager->Get( "Framebuffer Normal" ), 0 );
		glDrawBuffers( 2, bufs );
		if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
			cout << "framebuffer creation successful" << endl;
		}

	}

	void AddGeometry () {
		// remove any existing primitives
		geometryManager.Clear();

		// give the geometryManager a set of primitives
		//	I'd like to do some simple L system stuff

		// distribute a random set of N primitives... need to test some ranges
		bool randomDistribution = true;
		if ( randomDistribution ) {

			palette::PickRandomPalette( true );

			// rngN position = rngN( 0.0f, 0.25f );
			rng position = rng( -0.99f, 0.99f );
			rng sizeD = rng( 0.05f, 0.1f );
			rngN color = rngN( 0.5f, 0.2f );

			for ( int i = 0; i < 100; i++ ) {
				// add some capped cone primitives
				geometryManager.AddCappedCone(
					vec3( position(), position(), position() ),
					vec3( position(), position(), position() ),
					sizeD() / 5.0f, sizeD() / 5.0f, vec4( palette::paletteRef( color() ), 1.0f ) );

				// add some rounded box primitives
				geometryManager.AddRoundedBox(
					vec3( position(), position(), position() ),
					0.2f * vec3( sizeD(), sizeD(), sizeD() ),
					1000.0f * vec3( sizeD(), sizeD(), sizeD() ),
					sizeD() / 10.0f, vec4( palette::paletteRef( color() ), 1.0f ) );

				// add some ellipsoid primitives
				geometryManager.AddEllipsoid(
					vec3( position(), position(), position() ),
					vec3( sizeD(), sizeD(), sizeD() ),
					100.0f * vec3( sizeD(), sizeD(), sizeD() ),
					vec4( palette::paletteRef( color() ), 1.0f ) );
			}

			vec3 p0, p1;
			for ( int copy = 0; copy < 3; copy++ )
			for ( int axis = 0; axis < 3; axis++ ) {
				for ( float low : { -0.98f, 0.98f } ) {
					for ( float high : { -0.98f, 0.98f } ) {
						switch ( axis ) {
						case 0:
							p0 = vec3( 0.98f, low, high );
							p1 = vec3( -0.98f, low, high );
							break;

						case 1:
							p0 = vec3( low, 0.98f, high );
							p1 = vec3( low, -0.98f, high );
							break;

						case 2:
							p0 = vec3( low, high, 0.98f );
							p1 = vec3( low, high, -0.98f );
							break;
						}
						geometryManager.AddCapsule( p0, p1, 0.01f, vec4( palette::paletteRef( color() ), 1.0f ) );
					}
				}
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

	vector< vector< ivec2 > > viewportOffsets;
	vector< vector< mat4 > > atlasTransforms;
	// here, we can setup to do spherical, octahedral, hemioctahedral atlases
	void PrepAtlasTransforms () {

		// setup memory
		atlasTransforms.resize( atlasRenderConfig.numViewsX );
		viewportOffsets.resize( atlasRenderConfig.numViewsX );
		for ( int i = 0; i < atlasRenderConfig.numViewsX; i++ ) {
			atlasTransforms[ i ].resize( atlasRenderConfig.numViewsY );
			viewportOffsets[ i ].resize( atlasRenderConfig.numViewsY );
		}

		// populate the transforms
		mat4 baseTransform = glm::scale( vec3( 1.0f / sqrt( 3.0f ) ) ); // need to fit -1 to 1 inside of NDC in all cases (cube diagonal is worst case)
		const int nx = atlasRenderConfig.numViewsX;
		const int ny = atlasRenderConfig.numViewsY;
		for ( int x = 0; x < nx; x++ ) {
			for ( int y = 0; y < ny; y++ ) {

				// where on the atlas to render to
				viewportOffsets[ x ][ y ] = ivec2( x * atlasRenderConfig.resolution, y * atlasRenderConfig.resolution );

				// the view transform to use
				switch ( atlasRenderConfig.impostorType ) {
				case SPHERICAL: {
					// figure out what theta and phi are...
					const float theta = tau * ( float( x ) + 0.5f ) / nx;
					const float phi = pi * ( ( ( float( y ) + 0.5f ) / ny ) - 0.5f );

					// deciding on Z up, basic spherical transform
					atlasTransforms[ x ][ y ] = glm::mat4(
						glm::angleAxis( theta, vec3( 0.0f, 0.0f, 1.0f ) ) *
						glm::angleAxis( phi, vec3( 1.0f, 0.0f, 0.0f ) ) ) * baseTransform;

					break;
				}

				case OCTAHEDRAL: {
					// todo - https://www.shadertoy.com/view/NsfBWf mapping code is in the top of the image buffer
					break;
				}

				case HEMIOCTAHEDRAL: {
					// todo - https://aras-p.info/texts/CompactNormalStorage.html has some resources on the mapping
					break;
				}
				}
			}
		}
	}

	void RenderAtlas () {

		// using the current set of geometry in the geometryManager
		// create two buffers, sized based on the number of primitives:
		//	A: primitive geometry (16 floats per)
		//	B: bbox transforms (16 floats per)

		const int numPrimitives = geometryManager.count;

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

		// bind the current framebuffer
		glBindFramebuffer( GL_FRAMEBUFFER, framebuffer );

		// update the atlas views on the framebuffer, one at a time
		shader = bboxRasterShader;
		glUseProgram( shader );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		// blue noise jitter
		textureManager->BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );
		static rngi noiseOffset = rngi( 0, 512 );
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

		// iterate through the atlas entries
		const int nx = atlasRenderConfig.numViewsX;
		const int ny = atlasRenderConfig.numViewsY;
		for ( int x = 0; x < nx; x++ ) {
			for ( int y = 0; y < ny; y++ ) {

				// viewport base locations
				const int vpx = viewportOffsets[ x ][ y ].x;
				const int vpy = viewportOffsets[ x ][ y ].y;

				// transform, etc
				glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, false, glm::value_ptr( atlasTransforms[ x ][ y ] ) );
				glUniform1i( glGetUniformLocation( shader, "numPrimitives" ), numPrimitives );
				glUniform2i( glGetUniformLocation( shader, "viewportBase" ), vpx, vpy );
				glUniform2i( glGetUniformLocation( shader, "viewportSize" ), atlasRenderConfig.resolution, atlasRenderConfig.resolution );

				// set viewport and draw
				glViewport( vpx, vpy, atlasRenderConfig.resolution, atlasRenderConfig.resolution );
				glDrawArrays( GL_TRIANGLES, 0, 36 * numPrimitives );
			}
		}

		// revert to default framebuffer
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );

		// eventually want to add some stuff here to mix the result of several passes
	}
};