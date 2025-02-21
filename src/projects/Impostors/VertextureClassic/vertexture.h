#include <vector>
#include <string>

#include <includes.h>

// program config
struct vertextureConfig {

	// scaling the size of the GPU resources
	int Guys;
	int Trees;
	int Rocks;
	int GroundCover;
	int Lights;

	// timekeeping since last reset
	float timeVal;

	// visualization parameters
	float scale;
	float heightScale;
	bool showLightDebugLocations;
	vec3 groundColor;

	// output settings
	bool showTiming;
	bool showTrident;

	std::vector< vec3 > obstacles; // x,y location, then radius

	// default orientation
	vec3 basisX;
	vec3 basisY;
	vec3 basisZ;

	// for computing screen AR, from config.json
	uint32_t width;
	uint32_t height;
	float screenAR;

};

struct rnGenerators {
	// these should come from the config, construct everything in the APIGeometryContainer::LoadConfig() function

	rngi shaderWangSeed = rngi( 0, 100000 );

	// needed:
	// 	entity height min, max
	// 	entity phase gen - rngN probably do spread about 0
	// 	dudes max height for the bouncing
	// 	tree height step
	// 		tree number of points in canopy
	// 	tree jitter diameter
	// 		tree diameter for sim should be linked ( * 10.0f, looks like? )
	// 	trunk constrict parameter ( try values >1.0 )
	// 	trunk point size min, max
	// 	canopy point size min, max
	// 	ground cover point size min, max
};

// graphics api resources
struct openGLResources {

	std::unordered_map< string, GLuint > VAOs;
	std::unordered_map< string, GLuint > VBOs;
	std::unordered_map< string, GLuint > SSBOs;
	std::unordered_map< string, GLuint > FBOs;
	std::unordered_map< string, GLuint > textures;
	std::unordered_map< string, GLuint > shaders;

	// static / dynamic point counts, updated and reported during init
	int numPointsGround = 0;
	int numPointsSkirts = 0;
	int numPointsSpheres = 0;
	int numPointsMovingSpheres = 0;
	int numPointsWater = 0;

	// separate tridents for each shadowmap
	// textures for each shadowmap
	// FBOs for each shadowmap

};

// scoring, kills, etc, append a string
std::vector< std::string > eventReports;

// remapping float range from1..to1 -> from2..to2
inline float Remap ( float value, float from1, float to1, float from2, float to2 ) {
	return ( value - from1 ) / ( to1 - from1 ) * ( to2 - from2 ) + from2;
}

// quad subdivision ( recursive )
inline void subdivide (
	std::vector< vec3 > &pointsVector,
	const std::vector< vec3 > inputPoints,
	const float minDisplacement = 0.01f ) {

	if ( glm::distance( inputPoints[ 0 ], inputPoints[ 2 ] ) < minDisplacement ) {
		/* corner-to-corner distance is small, time to write API geometry
			A ( 0 )	@=======@ B ( 1 )
					|      /|
					|     / |
					|    /  |
					|   /   |
					|  /    |
					| /     |
					|/      |
			C ( 2 )	@=======@ D ( 3 ) --> X */
		// triangle 1 ABC
		pointsVector.push_back( inputPoints[ 0 ] );
		pointsVector.push_back( inputPoints[ 1 ] );
		pointsVector.push_back( inputPoints[ 2 ] );
		// triangle 2 BCD
		pointsVector.push_back( inputPoints[ 1 ] );
		pointsVector.push_back( inputPoints[ 2 ] );
		pointsVector.push_back( inputPoints[ 3 ] );
	} else {
		const vec3 center = ( inputPoints[ 0 ] +  inputPoints[ 1 ] + inputPoints[ 2 ] + inputPoints[ 3 ] ) / 4.0f;
		// midpoints between corners
		const vec3 midpoint13 = ( inputPoints[ 1 ] + inputPoints[ 3 ] ) / 2.0f;
		const vec3 midpoint01 = ( inputPoints[ 0 ] + inputPoints[ 1 ] ) / 2.0f;
		const vec3 midpoint23 = ( inputPoints[ 2 ] + inputPoints[ 3 ] ) / 2.0f;
		const vec3 midpoint02 = ( inputPoints[ 0 ] + inputPoints[ 2 ] ) / 2.0f;
		// recursive calls, next level of subdivision
		subdivide( pointsVector, { midpoint01, inputPoints[ 1 ], center, midpoint13 }, minDisplacement );
		subdivide( pointsVector, { inputPoints[ 0 ], midpoint01, midpoint02, center }, minDisplacement );
		subdivide( pointsVector, { center, midpoint13, midpoint23, inputPoints[ 3 ] }, minDisplacement );
		subdivide( pointsVector, { midpoint02, center, inputPoints[ 2 ], midpoint23 }, minDisplacement );
	}
}

/* ==========================================================================================================================
Consolidating the resources for the API geometry
========================================================================================================================== */

struct APIGeometryContainer {

	APIGeometryContainer () {}
	~APIGeometryContainer () {}

	// for keeping track of textures
	textureManager_t * textureManager_local;

	// init
	void LoadConfig ();	// read the relevant section of config.json
	void Initialize ();	// create the shaders, buffers and stuff, random init for points etc
	void InitReport ();	// tell how many points are being used, etc
	void Reset () {
		LoadConfig();
		Initialize();
		InitReport();
	}

	// shutdown
	void Terminate ();	// delete resources

	// main loop functions
	void Update ();	// run the movement compute shaders
	void Shadow ();	// update the shadowmap(s)
	void Render ();	// update the Gbuffer
	void DeferredPass (); // do the deferred lighting operations

	// ImGui manipulation of program state
	void ControlWindow ();

	// application data + state
	vertextureConfig config;
	openGLResources resources;
	rnGenerators rngs;

};

void APIGeometryContainer::LoadConfig () {
	// load the config from disk
	json j; ifstream i ( "../src/engine/config.json" ); i >> j; i.close();

	// this informs the behavior of Initialize()
	config.Guys			= j[ "app" ][ "VertextureClassic" ][ "Guys" ];
	config.Trees		= j[ "app" ][ "VertextureClassic" ][ "Trees" ];
	config.Rocks		= j[ "app" ][ "VertextureClassic" ][ "Rocks" ];
	config.Lights		= j[ "app" ][ "VertextureClassic" ][ "Lights" ];
	config.GroundCover	= j[ "app" ][ "VertextureClassic" ][ "GroundCover" ];
	config.showLightDebugLocations = j[ "app" ][ "VertextureClassic" ][ "ShowLightDebugLocations" ];

		// todo: write out the rest of the parameterization ( point sizes, counts, other distributions... )

	// other program state
	config.showTrident	= j[ "app" ][ "VertextureClassic" ][ "showTrident" ];
	config.showTiming	= j[ "app" ][ "VertextureClassic" ][ "showTiming" ];
	config.scale		= j[ "app" ][ "VertextureClassic" ][ "InitialScale" ];
	config.heightScale	= j[ "app" ][ "VertextureClassic" ][ "InitialHeightScale" ];
	config.basisX		= vec3( j[ "app" ][ "VertextureClassic" ][ "basisX" ][ "x" ],
								j[ "app" ][ "VertextureClassic" ][ "basisX" ][ "y" ],
								j[ "app" ][ "VertextureClassic" ][ "basisX" ][ "z" ] );
	config.basisY		= vec3( j[ "app" ][ "VertextureClassic" ][ "basisY" ][ "x" ],
								j[ "app" ][ "VertextureClassic" ][ "basisY" ][ "y" ],
								j[ "app" ][ "VertextureClassic" ][ "basisY" ][ "z" ] );
	config.basisZ		= vec3( j[ "app" ][ "VertextureClassic" ][ "basisZ" ][ "x" ],
								j[ "app" ][ "VertextureClassic" ][ "basisZ" ][ "y" ],
								j[ "app" ][ "VertextureClassic" ][ "basisZ" ][ "z" ] );

	// update AR with current value of screen dims
	config.width = j[ "system" ][ "screenWidth" ];
	config.height = j[ "system" ][ "screenHeight" ];
	config.screenAR = ( float ) config.width / ( float ) config.height;

	// reset amount of time since last reset
	config.timeVal = 0.0f;
}

void APIGeometryContainer::Initialize () {

	// if this is the first time that this has run
		// probably something we can do if we check for this behavior, something - tbd

	// clear existing list of obstacles
	config.obstacles.resize( 0 );

	// create all the graphics api resources
	const string basePath( "../src/projects/Impostors/VertextureClassic/shaders/" );
	resources.shaders[ "Background" ]			= computeShader( basePath + "background.cs.glsl" ).shaderHandle;
	resources.shaders[ "Deferred" ]				= computeShader( basePath + "deferred.cs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere Movement" ]		= computeShader( basePath + "movingSphere.cs.glsl" ).shaderHandle;
	resources.shaders[ "Light Movement" ]		= computeShader( basePath + "movingLight.cs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere Map Update" ]	= computeShader( basePath + "movingSphereMaps.cs.glsl" ).shaderHandle;
	resources.shaders[ "Ground" ]				= regularShader( basePath + "ground.vs.glsl", basePath + "ground.fs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere" ]				= regularShader( basePath + "sphere.vs.glsl", basePath + "sphere.fs.glsl" ).shaderHandle;
	resources.shaders[ "Moving Sphere" ]		= regularShader( basePath + "movingSphere.vs.glsl", basePath + "movingSphere.fs.glsl" ).shaderHandle;
	resources.shaders[ "Water" ]				= regularShader( basePath + "water.vs.glsl", basePath + "water.fs.glsl" ).shaderHandle;
	resources.shaders[ "Skirts" ]				= regularShader( basePath + "skirts.vs.glsl", basePath + "skirts.fs.glsl" ).shaderHandle;

	static bool firstTime = true;
	if ( firstTime ) {

		textureOptions_t opts;

		// ====================================
		// == Simulation Maps =================
		// ==== Steepness =====================
		opts.dataType = GL_RGBA32F;
		opts.textureType = GL_TEXTURE_2D;
		opts.width = 512;
		opts.height = 512;
		textureManager_local->Add( "Steepness Map", opts );
		// ==== Distance / Direction ==========
		textureManager_local->Add( "Distance / Direction Map", opts );
		// ====================================


		// ====================================
		// == Framebuffer Textures ============
		// ==== Depth =========================
		opts.dataType = GL_DEPTH_COMPONENT32;
		opts.width = config.width;
		opts.height = config.height;
		textureManager_local->Add( "Framebuffer Depth", opts );
		// ==== Color =========================
		opts.dataType = GL_RGBA16F;
		textureManager_local->Add( "Framebuffer Color", opts );
		// ==== Normal ========================
		textureManager_local->Add( "Framebuffer Normal", opts );
		// ==== Position ======================
		textureManager_local->Add( "Framebuffer Position", opts );
		// ====================================


		// ====================================
		// == Display / Data Textures =========
		// ==== Sphere Heightmap ==============
		// maybe port this to the analytic model, later - I might also want to keep it for legacy's sake
		Image_4U sphereImageData( "../src/projects/Impostors/VertextureClassic/textures/sphere.png" );
		opts.dataType = GL_RGBA8;
		opts.minFilter = GL_LINEAR_MIPMAP_LINEAR;
		opts.magFilter = GL_LINEAR;
		opts.wrap = GL_REPEAT;
		opts.width = sphereImageData.Width();
		opts.height = sphereImageData.Height();
		opts.initialData = ( void * ) sphereImageData.GetImageDataBasePtr();
		textureManager_local->Add( "Sphere Heightmap", opts );

		// ==== Rock Heightmap ================
		// consider swapping out for a generated heightmap? something with ~10s of erosion applied? tbd
		Image_4U heightmapImage( "../src/projects/Impostors/VertextureClassic/textures/rock_height.png" );
		opts.width = heightmapImage.Width();
		opts.height = heightmapImage.Height();
		opts.initialData = ( void * ) heightmapImage.GetImageDataBasePtr();
		textureManager_local->Add( "Ground Heightmap", opts );

		// ==== Water Heightmap ===============
		Image_4U height( "../src/projects/Impostors/VertextureClassic/textures/water_height.png" );
		opts.width = height.Width();
		opts.height = height.Height();
		opts.initialData = ( void * ) height.GetImageDataBasePtr();
		textureManager_local->Add( "Water Heightmap", opts );

		// ==== Water Normal ==================
		Image_4U normal( "../src/projects/Impostors/VertextureClassic/textures/water_norm.png" );
		opts.width = normal.Width();
		opts.height = normal.Height();
		opts.initialData = ( void * ) normal.GetImageDataBasePtr();
		textureManager_local->Add( "Water Normal Map", opts );

		// ==== Water Color ===================
		Image_4U color( "../src/projects/Impostors/VertextureClassic/textures/water_color.png" );
		opts.width = color.Width();
		opts.height = color.Height();
		opts.initialData = ( void * ) color.GetImageDataBasePtr();
		textureManager_local->Add( "Water Color Map", opts );

		// ====================================

		// setup the buffers for the rendering process - need depth, worldspace position, normals, color
		GLuint primaryFramebuffer;
		glGenFramebuffers( 1, &primaryFramebuffer );
		glBindFramebuffer( GL_FRAMEBUFFER, primaryFramebuffer );

		// last argument is mip level, interesting
		glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager_local->Get( "Framebuffer Depth" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager_local->Get( "Framebuffer Color" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager_local->Get( "Framebuffer Normal" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, textureManager_local->Get( "Framebuffer Position" ), 0 );

		const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		glDrawBuffers( 3, bufs );

		resources.FBOs[ "Primary" ] = primaryFramebuffer;
		if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
			cout << "framebuffer creation successful" << endl;
		}

	}

	// generate all the geometry

	// pick a first palette ( lights )
	palette::PickRandomPalette();

	// randomly positioned + colored lights
		// ssbo, shader for movement
	std::vector< GLfloat > lightData;
	{
		GLuint ssbo;
		rng location( -1.0f, 1.0f );
		rng zDistrib( 0.0f, 0.0f );
		rng colorPick( 0.6f, 0.8f );
		rng brightness( 0.002f, 0.005f );

		for ( int x = 0; x < config.Lights; x++ ) {
		// need to figure out what the buffer needs to hold
			// position ( vec3 + some extra value... we'll find a use for it )
			// color ( vec3 + some extra value, again we'll find some kind of use for it )

			// distribute initial light points
			lightData.push_back( location() );
			lightData.push_back( location() );
			lightData.push_back( zDistrib() );
			lightData.push_back( 0.0f );

			vec3 col = palette::paletteRef( colorPick(), palette::type::paletteIndexed_interpolated ) * brightness();
			lightData.push_back( col.r );
			lightData.push_back( col.g );
			lightData.push_back( col.b );
			lightData.push_back( 1.0f );

			// halfway through, switch colors
			if ( x == ( config.Lights / 2 ) ) {
				palette::PickRandomPalette();
			}
		}

		// create the SSBO and bind in slot 4
		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * config.Lights, ( GLvoid * ) &lightData[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, ssbo );

		resources.SSBOs[ "Lights" ] = ssbo;
	}

	// pick a second palette ( ground, skirts, spheres )
	palette::PickRandomPalette();

	// ground
		// shader, vao / vbo
		// heightmap texture
	{
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		{
			basePoints.resize( 4 );
			basePoints[ 0 ] = vec3( -1.0f, -1.0f, 0.0f );
			basePoints[ 1 ] = vec3( -1.0f,  1.0f, 0.0f );
			basePoints[ 2 ] = vec3(  1.0f, -1.0f, 0.0f );
			basePoints[ 3 ] = vec3(  1.0f,  1.0f, 0.0f );
			subdivide( world, basePoints );
		}

		GLuint vao, vbo;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		resources.numPointsGround = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * resources.numPointsGround;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Ground" ], "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );

		resources.VAOs[ "Ground" ] = vao;
		resources.VBOs[ "Ground" ] = vbo;

		rng gen( 0.0f, 1.0f ); // this can probably be tuned better
		config.groundColor = palette::paletteRef( gen() + 0.5f, palette::type::paletteIndexed_interpolated );
	}

	// skirts
		// shader, vao / vbo
	{
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		{
			basePoints.resize( 4 );
			basePoints[ 0 ] = vec3( -1.0, -1.0,  0.2f );
			basePoints[ 1 ] = vec3( -1.0, -1.0, -0.5f );
			basePoints[ 2 ] = vec3(  1.0, -1.0,  0.2f );
			basePoints[ 3 ] = vec3(  1.0, -1.0, -0.5f );

			// triangle 1 ABC
			world.push_back( basePoints[ 0 ] );
			world.push_back( basePoints[ 1 ] );
			world.push_back( basePoints[ 2 ] );
			// triangle 2 BCD
			world.push_back( basePoints[ 2 ] );
			world.push_back( basePoints[ 1 ] );
			world.push_back( basePoints[ 3 ] );

			basePoints[ 0 ] = vec3( -1.0, -1.0,  0.2f );
			basePoints[ 1 ] = vec3( -1.0, -1.0, -0.5f );
			basePoints[ 2 ] = vec3( -1.0,  1.0,  0.2f );
			basePoints[ 3 ] = vec3( -1.0,  1.0, -0.5f );

			world.push_back( basePoints[ 0 ] );
			world.push_back( basePoints[ 2 ] );
			world.push_back( basePoints[ 1 ] );
			world.push_back( basePoints[ 1 ] );
			world.push_back( basePoints[ 2 ] );
			world.push_back( basePoints[ 3 ] );

			basePoints[ 0 ] = vec3(  1.0, -1.0,  0.2f );
			basePoints[ 1 ] = vec3(  1.0, -1.0, -0.5f );
			basePoints[ 2 ] = vec3(  1.0,  1.0,  0.2f );
			basePoints[ 3 ] = vec3(  1.0,  1.0, -0.5f );

			world.push_back( basePoints[ 0 ] );
			world.push_back( basePoints[ 1 ] );
			world.push_back( basePoints[ 2 ] );
			world.push_back( basePoints[ 2 ] );
			world.push_back( basePoints[ 1 ] );
			world.push_back( basePoints[ 3 ] );

			basePoints[ 0 ] = vec3(  1.0,  1.0,  0.2f );
			basePoints[ 1 ] = vec3(  1.0,  1.0, -0.5f );
			basePoints[ 2 ] = vec3( -1.0,  1.0,  0.2f );
			basePoints[ 3 ] = vec3( -1.0,  1.0, -0.5f );

			world.push_back( basePoints[ 0 ] );
			world.push_back( basePoints[ 1 ] );
			world.push_back( basePoints[ 2 ] );
			world.push_back( basePoints[ 2 ] );
			world.push_back( basePoints[ 1 ] );
			world.push_back( basePoints[ 3 ] );
		}

		GLuint vao, vbo;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		resources.numPointsSkirts = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * resources.numPointsSkirts;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		resources.VAOs[ "Skirts" ] = vao;
		resources.VBOs[ "Skirts" ] = vbo;

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Skirts" ], "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );
	}

	// spheres
		// shader + vertex buffer of the static points
			// optionally include the debug spheres for the light positions
		// shader + ssbo buffer of the dynamic points
	{
		std::vector< vec4 > points;
		std::vector< vec4 > ssboPoints;
		std::vector< vec4 > colors;

		{
			// some set of static points, loaded into the vbo - these won't change, they get generated once, they know where to read from
				// the texture for the height, and then they displace vertically in the shader

			rng heightGen( 0.0f, 0.14f );
			rng phaseGen( 1.85f, 3.7f );
			rng randomWorldXYGen( -1.0f, 1.0f );
			rngN trunkJitter( 0.0f, 0.006f );
			rng trunkSizes( 1.5f, 4.76f );
			rng treeHeightGen( 0.185f, 0.74f );
			rng basePtPlace( -0.75f, 0.75f );
			rng leafSizes( 1.27f, 12.6f );
			rngN foliagePlace( 0.0f, 0.1618f );
			rng roughnessGen( 0.1f, 40.0f );
			rngN rockGen( 0.0f, 0.037f );
			rngN rockHGen( 0.06f, 0.037f );
			rngN rockSize( 3.86f, 6.0f );

			for ( int i = 0; i < config.GroundCover; i++ ) { // randomly placed, uniformly distributed ground cover
				points.push_back( vec4( randomWorldXYGen(), randomWorldXYGen(), heightGen(), phaseGen() ) );
				colors.push_back( vec4( palette::paletteRef( heightGen() * 3.0f, palette::type::paletteIndexed_interpolated ), 1.0f ) );
			}

			for ( int i = 0; i < config.Trees; i++ ) { // trees
				const vec2 basePtOrig = vec2( basePtPlace(), basePtPlace() );
				vec2 basePt = basePtOrig;
				float constrict = 1.618f;
				float scalar = treeHeightGen();

				// add a new obstacle at the tree location
				config.obstacles.push_back( vec3( basePt.x, basePt.y, 0.06f ) );

				// create the points for the trunk
				for ( float t = 0; t < scalar; t += 0.002f ) {
					basePt.x += trunkJitter() * 0.5f;
					basePt.y += trunkJitter() * 0.5f;
					constrict *= 0.999f;
					points.push_back( vec4( constrict * trunkJitter() + basePt.x, constrict * trunkJitter() + basePt.y, t, constrict * trunkSizes() ) );
					colors.push_back( vec4( palette::paletteRef( heightGen(), palette::type::paletteIndexed_interpolated ), roughnessGen() ) );
				}

				// create the points for the canopy
				// for ( int j = 0; j < 1000; j++ ) {
				for ( int j = 0; j < 5000; j++ ) {
					rngN foliageHeightGen( scalar, 0.05f );
					points.push_back( vec4( basePt.x + foliagePlace(), basePt.y + foliagePlace(), foliageHeightGen(), leafSizes() ) );
					colors.push_back( vec4( palette::paletteRef( heightGen() + 0.3f, palette::type::paletteIndexed_interpolated ), roughnessGen() ) );
				}
			}

			for ( int i = 0; i < config.Rocks; i++ ) {
				vec2 basePt = vec2( basePtPlace(), basePtPlace() );
				config.obstacles.push_back( vec3( basePt.x, basePt.y, 0.13f ) );
				for ( int l = 0; l < 1000; l++ ) {
					points.push_back( vec4( basePt.x + rockGen(), basePt.y + rockGen(), rockHGen(), rockSize() ) );
					colors.push_back( vec4( palette::paletteRef( heightGen() + 0.2f, palette::type::paletteIndexed_interpolated ), roughnessGen() ) );
				}
			}

			// debug spheres for the lights
			if ( config.showLightDebugLocations == true ) {
				for ( unsigned int i = 0; i < lightData.size() / 8; i++ ) {
					const size_t basePt = 8 * i;

					vec4 position = vec4( lightData[ basePt ], lightData[ basePt + 1 ], lightData[ basePt + 2 ], 10.0f );
					// vec4 color = vec4( 1.0f, 1.0f, 1.0f, 11.0f );
					vec4 color = vec4( 1.0f, 0.0f, 0.0f, 1.0f );

					points.push_back( position );
					colors.push_back( color );
				}
			}

			int dynamicPointCount = 0;
			rng size( 0.5f, 4.5f );
			rng phase( 0.0f, pi * 6.0f );
			for ( int x = 0; x < config.Guys; x++ ) {
				for ( int y = 0; y < config.Guys; y++ ) {
					ssboPoints.push_back( vec4( 2.0f * ( ( x / float( config.Guys ) ) - 0.5f ), 2.0f * ( ( y / float( config.Guys ) ) - 0.5f ), 0.6f * heightGen(), size() ) );
					ssboPoints.push_back( vec4( palette::paletteRef( heightGen() + 0.5f, palette::type::paletteIndexed_interpolated ), phase() ) );
					dynamicPointCount++;
				}
			}
			resources.numPointsMovingSpheres = dynamicPointCount;
		}

		GLuint vao, vbo;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		resources.numPointsSpheres = points.size();
		size_t numBytesPoints = sizeof( vec4 ) * resources.numPointsSpheres;
		size_t numBytesColors = sizeof( vec4 ) * resources.numPointsSpheres;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints + numBytesColors, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &points[ 0 ] );
		glBufferSubData( GL_ARRAY_BUFFER, numBytesPoints, numBytesColors, &colors[ 0 ] );

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Sphere" ], "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );
		GLuint vColor = glGetAttribLocation( resources.shaders[ "Sphere" ], "vColor" );
		glEnableVertexAttribArray( vColor );
		glVertexAttribPointer( vColor, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( numBytesPoints ) ) );

		GLuint ssbo;
		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * resources.numPointsMovingSpheres, ( GLvoid * ) &ssboPoints[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, ssbo );

		resources.SSBOs[ "Moving Sphere" ] = ssbo;
		resources.VAOs[ "Sphere" ] = vao;
		resources.VBOs[ "Sphere" ] = vbo;
	}

	// water
		// shader, vao / vbo
		// 3x textures
	{
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		basePoints.resize( 4 );
		basePoints[ 0 ] = vec3( -1.0f, -1.0f, 0.01f );
		basePoints[ 1 ] = vec3( -1.0f,  1.0f, 0.01f );
		basePoints[ 2 ] = vec3(  1.0f, -1.0f, 0.01f );
		basePoints[ 3 ] = vec3(  1.0f,  1.0f, 0.01f );
		subdivide( world, basePoints );

		GLuint vao, vbo;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		resources.numPointsWater = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * resources.numPointsWater;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Water" ], "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );

		resources.VAOs[ "Water" ] = vao;
		resources.VBOs[ "Water" ] = vbo;
	}

	// todo: deferred pass resources

}

void APIGeometryContainer::InitReport () {
	// tell the stats for the current run of the program
	cout << "\nVertexture2 Init Complete:\n";
	cout << "Point Totals:\n";
	cout << "\tGround:\t\t\t" << resources.numPointsGround << newline;
	cout << "\tSkirts:\t\t\t" << resources.numPointsSkirts << newline;
	cout << "\tSpheres:\t\t" << resources.numPointsSpheres << newline;
	cout << "\tMoving Spheres:\t\t" << resources.numPointsMovingSpheres << newline;
	cout << "\tWater:\t\t\t" << resources.numPointsWater << newline << newline;
}

void APIGeometryContainer::Terminate () {

	// TODO: destroy the graphics api resources

	glDeleteFramebuffers( 1, &resources.FBOs[ "Primary" ] );

}

void APIGeometryContainer::Shadow () {
	// =================================================================================================

	// shadowmapping resources -> will not be here
	// orientTrident tridentDepth;
	// GLuint shadowmapFramebuffer = 0;
	// GLuint depthTexture;

	// shadowmapping resources:
	//	http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/#rendering-the-shadow-map
	//	https://ogldev.org/www/tutorial23/tutorial23.html

	// create the shadowmap resources

	/*
	GLuint shadowmapFramebuffer = 0;
	glGenFramebuffers( 1, &shadowmapFramebuffer );
	glBindFramebuffer( GL_FRAMEBUFFER, shadowmapFramebuffer );

	// Depth texture - slower than a depth buffer, but you can sample it later in your shader
	glGenTextures( 1, &depthTexture );
	glBindTexture( GL_TEXTURE_2D, depthTexture );
	glTexImage2D( GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT16, 1024, 1024, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0 );
	glDrawBuffer( GL_NONE ); // No color buffer is drawn to.

	if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		cout << "framebuffer creation failed" << endl; abort();
	}

	// revert to default framebuffer
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

		// I think eventually it's going to make more sense to just rewrite things to manage both
		// color and depth targets, so that we have more explicit state management. I'm not sure
		// what was causing the state leakage issue I was seeing but I have other things I want to
		// mess with instead.

		// And maybe that's something I'll want to carry forwards in the future - there is a number
		// of cool aspects there, you know, you can access that color data and do whatever postprocessing
		// that you want - also opens up deferred shading, which may be significant for this case
		// where you are writing depth for these primitives - keeping normals and depth to reconstruct
		// the world position, we can do whatever lighting on that flat target, and avoid having to
		// potentially shade the occluded fragments.. I'm not clear on if the current impl will have to


		// // prepare to render the shadowmap depth
		// glBindFramebuffer( GL_FRAMEBUFFER, shadowmapFramebuffer );
		// glClear( GL_DEPTH_BUFFER_BIT );

		// revert to default framebuffer
		// glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		// glClear( GL_DEPTH_BUFFER_BIT );


	*/

	// =================================================================================================
}

void APIGeometryContainer::DeferredPass () {
	glUseProgram( resources.shaders[ "Deferred" ] );

	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ ); // matrix for the view transform
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Deferred" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );

	textureManager_local->BindTexForShader( "Framebuffer Depth", "depthTexture", resources.shaders[ "Deferred" ], 0 );
	textureManager_local->BindTexForShader( "Framebuffer Color", "colorTexture", resources.shaders[ "Deferred" ], 1 );
	textureManager_local->BindTexForShader( "Framebuffer Normal", "normalTexture", resources.shaders[ "Deferred" ], 2 );
	textureManager_local->BindTexForShader( "Framebuffer Position", "positionTexture", resources.shaders[ "Deferred" ], 3 );

	glUniform1i( glGetUniformLocation( resources.shaders[ "Deferred" ], "lightCount" ), config.Lights );
	glUniform2f( glGetUniformLocation( resources.shaders[ "Deferred" ], "resolution" ), config.width, config.height );
	glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void APIGeometryContainer::Render () {

	// then the regular view of the geometry
	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ ); // matrix for the view transform

	glBindFramebuffer( GL_FRAMEBUFFER, resources.FBOs[ "Primary" ] );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// ground
	glBindVertexArray( resources.VAOs[ "Ground" ] );
	glUseProgram( resources.shaders[ "Ground" ] );
	glEnable( GL_DEPTH_TEST );
	glUniform3f( glGetUniformLocation( resources.shaders[ "Ground" ], "groundColor" ), config.groundColor.x, config.groundColor.y, config.groundColor.z );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Ground" ], "time" ), config.timeVal / 10000.0f );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Ground" ], "heightScale" ), config.heightScale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Ground" ], "lightCount" ), config.Lights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Ground" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Ground" ], "scale" ), config.scale );
	textureManager_local->BindTexForShader( "Ground Heightmap", "heightmap", resources.shaders[ "Ground" ], 0 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Ground" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_TRIANGLES, 0, resources.numPointsGround );

	// spheres
	glBindVertexArray( resources.VAOs[ "Sphere" ] );
	glUseProgram( resources.shaders[ "Sphere" ] );
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_PROGRAM_POINT_SIZE );
	glPointParameteri( GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT );

	// static points
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "heightScale" ), config.heightScale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "lightCount" ), config.Lights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "frameHeight" ), config.height );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "scale" ), config.scale );
	textureManager_local->BindTexForShader( "Ground Heightmap", "heightmap", resources.shaders[ "Sphere" ], 0 );
	textureManager_local->BindTexForShader( "Sphere Heightmap", "sphere", resources.shaders[ "Sphere" ], 1 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Sphere" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_POINTS, 0, resources.numPointsSpheres );

	// dynamic points
	glUseProgram( resources.shaders[ "Moving Sphere" ] );
	glBindImageTexture( 1, textureManager_local->Get( "Steepness Map" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glBindImageTexture( 2, textureManager_local->Get( "Distance / Direction Map" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "heightScale" ), config.heightScale );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "lightCount" ), config.Lights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "frameHeight" ), config.height );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "scale" ), config.scale );
	textureManager_local->BindTexForShader( "Ground Heightmap", "heightmap", resources.shaders[ "Moving Sphere" ], 0 );
	textureManager_local->BindTexForShader( "Sphere Heightmap", "sphere", resources.shaders[ "Moving Sphere" ], 1 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_POINTS, 0, resources.numPointsMovingSpheres );

	// water
	glBindVertexArray( resources.VAOs[ "Water" ] );
	glUseProgram( resources.shaders[ "Water" ] );
	glEnable( GL_DEPTH_TEST );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Water" ], "time" ), config.timeVal / 10000.0f );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Water" ], "lightCount" ), numLights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Water" ], "heightScale" ), config.heightScale );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Water" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Water" ], "scale" ), config.scale );
	textureManager_local->BindTexForShader( "Water Color Map", "colorMap", resources.shaders[ "Water" ], 0 );
	textureManager_local->BindTexForShader( "Water Normal Map", "normalMap", resources.shaders[ "Water" ], 1 );
	textureManager_local->BindTexForShader( "Water Heightmap", "heightMap", resources.shaders[ "Water" ], 2 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Water" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_TRIANGLES, 0, resources.numPointsWater );

	// skirts
	glBindVertexArray( resources.VAOs[ "Skirts" ] );
	glUseProgram( resources.shaders[ "Skirts" ] );
	glEnable( GL_DEPTH_TEST );
	glUniform3f( glGetUniformLocation( resources.shaders[ "Skirts" ], "groundColor" ), config.groundColor.x, config.groundColor.y, config.groundColor.z );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Skirts" ], "lightCount" ), numLights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Skirts" ], "heightScale" ), config.heightScale );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Skirts" ], "time" ), config.timeVal / 10000.0f );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Skirts" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Skirts" ], "scale" ), config.scale );
	textureManager_local->BindTexForShader( "Ground Heightmap", "heightMap", resources.shaders[ "Skirts" ], 0 );
	textureManager_local->BindTexForShader( "Water Heightmap", "waterHeight", resources.shaders[ "Skirts" ], 1 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Skirts" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_TRIANGLES, 0, resources.numPointsSkirts );

	// revert to default framebuffer
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void APIGeometryContainer::Update () {

	config.timeVal += 0.1f;

	// run the stuff to update the moving point locations
	glUseProgram( resources.shaders[ "Sphere Map Update" ] );
	glBindImageTexture( 1, textureManager_local->Get( "Steepness Map" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glBindImageTexture( 2, textureManager_local->Get( "Distance / Direction Map" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	textureManager_local->BindTexForShader( "Ground Heightmap", "heightmap", resources.shaders[ "Sphere Map Update" ], 0 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "numObstacles" ), config.obstacles.size() );
	glUniform3fv( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "obstacles" ), config.obstacles.size(), glm::value_ptr( config.obstacles[ 0 ] ) );
	glDispatchCompute( 512 / 16, 512 / 16, 1 );

	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	glUseProgram( resources.shaders[ "Sphere Movement" ] );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, resources.SSBOs[ "Moving Sphere" ] );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, resources.SSBOs[ "Moving Sphere" ] );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "dimension" ), config.Guys );
	glDispatchCompute( config.Guys / 16, config.Guys / 16, 1 ); // dispatch the compute shader to update ssbo

	// run the stuff to update the light positions - this needs more work
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	glUseProgram( resources.shaders[ "Light Movement" ] );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Light Movement" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Light Movement" ], "inSeed" ), rngs.shaderWangSeed() );
	glDispatchCompute( config.Lights / 16, 1, 1 ); // dispatch the compute shader to update ssbo

}

void APIGeometryContainer::ControlWindow () {
	ImGui::Begin( "Controls Window", NULL, 0 );

	ImGui::Checkbox( "Show Timing", &config.showTiming );
	ImGui::Checkbox( "Show Trident", &config.showTrident );

	// etc

	ImGui::End();
}