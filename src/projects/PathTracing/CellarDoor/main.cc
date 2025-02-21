#include "../../../engine/engine.h"

// ================================================================================================================
// ==== Config Structs ============================================================================================
// ================================================================================================================
struct spherePackConfig_t {
	// color picking
	float paletteRefMin = 0.0f;
	float paletteRefMax = 1.0f;
	float paletteRefJitter = 0.01f;
	float alphaGenMin = 0.5f;
	float alphaGenMax = 1.0f;

	// sizing
	float radiiInitialValue = 63.0f;
	float radiiStepShrink = 1.0f / 1.618f;
	float iterationMultiplier = 2.3f;
	int rngSeed = 0;

	// bounds manip
	vec3 boundsStepShrink = vec3( 0.99f, 0.99f, 0.92f );

	// termination
	uint32_t maxAllowedTotalIterations = 1000000;
	uint32_t sphereTrim = 100;
};

struct perlinPackConfig_t {
	// color picking
	float paletteRefMin = 0.0f;
	float paletteRefMax = 1.0f;
	float paletteRefJitter = 0.01f;

	// sizing
	float radiusMin = 1.618f;
	float radiusMax = 10.0f;
	float radiusJitter = 0.2f;
	float rampPower = 5.0f;
	float zSquashMin = 0.25f;
	float zSquashMax = 1.2f;

	// may eventually want to plumb this into all generators, to make everything deterministic ( sphere placement, etc )
	int rngSeed = 0;

	// other
	float padding = 20.0f;
	vec3 noiseScalar = vec3( 120.0f );
	vec3 noiseOffset = vec3( 0.0f );

	uint32_t maxAllowedTotalIterations = 1000000;
};

struct torusPackConfig_t {
	// color picking
	float paletteRefMin = 0.0f;
	float paletteRefMax = 1.0f;
	float paletteRefJitter = 0.01f;

	// sphere sizing
	float sphereRadiusMin = 1.0f;
	float sphereRadiusMax = 10.0f;
	float sphereRadiusJitter = 0.2f;
	float rampPower = 5.0f;

	// torus sizing
	float minorRadius = 69.0f;
	float majorRadius = 168.0f;

	// stuff about torus major and minor radii variation
		// vary radii with noise, sampled at the angle - map noise to this range
	// float minorRadiusMin = 100.0;
	// float minorRadiusMax = 100.0;
	// float majorRadiusMin = 200.0;
	// float majorRadiusMax = 200.0;

	// other
	vec3 noiseScalar = vec3( 120.0f );
	vec3 noiseOffset = vec3( 0.0f );
	int rngSeed = 0;
	uint32_t maxAllowedTotalIterations = 1000000;
};

struct cellarDoorConfig_t {
	bool userRequestedScreenshot = false;
	ivec3 dimensions;

	// leaving this global for now, because I still want to do the 16-bit addressing thing, at some point
	uint32_t maxSpheres = 65535;

	float scale = 1.0f;
	float thinLensIntensity = 0.1f;
	float thinLensDistance = 2.0f;
	float blendAmount = 0.9f;
	vec2 viewOffset = vec2( 0.0f );
	rngi wangSeeder = rngi( 0, 69420 );
	rng noiseOffset = rng( -1000.0f, 1000.0f );

	// worker thread/generation stuff
	int jobType = 2;
	bool workerThreadShouldRun = false;
	bool bufferReady = false;
	std::vector< vec4 > sphereBuffer;

	// config structs
	spherePackConfig_t incrementalConfig;
	perlinPackConfig_t perlinConfig;
	torusPackConfig_t torusConfig;

	// scene setup
	bool refractiveBubble = false;
	float IoR = 4.0f;

	// volumetric effect
	float fogScalar = 0.0001618f;
	vec3 fogColor = vec3( 1.0f );

	// for tiled update
	std::vector< ivec3 > updateTiles;
	size_t numTiles;

	// for the bayer pattern analog thing, I prefer to do it this way
	int lightingRemaining = 0;
	std::vector< ivec3 > offsets;

	// OpenGL Data
	GLuint sphereSSBO;
};

// ================================================================================================================
// ==== CellarDoor ================================================================================================
// ================================================================================================================
// Very similar in apparance to recent versions of Voraldo, in a lot of ways - they share no code, beyond the AABB
// intersection code for the voxel bounds. This does a forward DDA traversal to the first voxel with a nonzero alpha
// value. The color value is taken from that voxel, and a volume term is added to it, based on the length of the
// traversal. There is no blending, and no transparency - the illusion comes from the translucency in baked into
// the volume. The shadows and lighting are computed as a separate pass, destructivelsy baked into the albedo,
// informed by Beer's law and traversals towards a given light direction.
// ================================================================================================================
class cellarDoor final : public engineBase {
public:
	cellarDoor () { Init(); OnInit(); PostInit(); }
	~cellarDoor () { Quit(); }

	cellarDoorConfig_t cellarDoorConfig;

	// for visually monitoring worker thread, GPU work, etc
	progressBar generateBar;
	progressBar evaluateBar;
	progressBar lightingBar;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// prep reporter shit
			generateBar.reportWidth = evaluateBar.reportWidth = lightingBar.reportWidth = 16;
			generateBar.label = string( "Generate: " );
			evaluateBar.label = string( "Evaluate: " );
			lightingBar.label = string( "Lighting: " );

			// compile shaders
			CompileShaders();

			// get some initial palette selected
			palette::PickRandomPalette();

	// ================================================================================================================
	// ==== Load Config ===============================================================================================
	// ================================================================================================================

			// get the configuration from config.json
			json j; ifstream i ( "../src/engine/config.json" ); i >> j; i.close();
			cellarDoorConfig.dimensions.x	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "x" ];
			cellarDoorConfig.dimensions.y	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "y" ];
			cellarDoorConfig.dimensions.z	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "z" ];

	// ================================================================================================================
	// ==== Create Textures ===========================================================================================
	// ================================================================================================================
			textureOptions_t opts;
			opts.textureType	= GL_TEXTURE_3D;
			opts.width			= cellarDoorConfig.dimensions.x;
			opts.height			= cellarDoorConfig.dimensions.y;
			opts.depth			= cellarDoorConfig.dimensions.z;

			// baked primitive IDs
			opts.dataType		= GL_RG32UI;
			textureManager.Add( "ID Buffer", opts );

			// forward pt buffers
			opts.dataType		= GL_R32UI;
			textureManager.Add( "Red Atomic", opts );
			textureManager.Add( "Blue Atomic", opts );
			textureManager.Add( "Green Atomic", opts );
			textureManager.Add( "Count Atomic", opts );

			// output color
			opts.dataType		= GL_RGBA16F;
			textureManager.Add( "Color Buffer", opts );

	// ================================================================================================================
	// ===== Sphere Packing ===========================================================================================
	// ================================================================================================================

			glGenBuffers( 1, &cellarDoorConfig.sphereSSBO );

			{
				// yucky, yucky, whatever
				ComputeTileList(); cellarDoorConfig.updateTiles.clear();
				ComputeUpdateOffsets(); cellarDoorConfig.offsets.clear();
			}

			// kick off worker thread, ready to go
			cellarDoorConfig.workerThreadShouldRun = true;

		}

	// ================================================================================================================
	// ==== Physarum Init =============================================================================================
	// ================================================================================================================
		// agents, buffers
		// sage jenson - "guided" physarum - segments generated by L-system write values into the pheremone buffer? something like this

		// more:
			// vines, growing over the spheres
			// fishies?
			// forward PT ( caustics from overhead water surface, other lighting )
				// some kind of feedback, plant growth informed by environmental lighting

	}

	void CompileShaders () {
		shaders[ "Dummy Draw" ]		= computeShader( "../src/projects/PathTracing/CellarDoor/shaders/dummyDraw.cs.glsl" ).shaderHandle;
		shaders[ "Precompute" ]		= computeShader( "../src/projects/PathTracing/CellarDoor/shaders/precompute.cs.glsl" ).shaderHandle;
		shaders[ "Lighting" ]		= computeShader( "../src/projects/PathTracing/CellarDoor/shaders/lighting.cs.glsl" ).shaderHandle;
		shaders[ "Ray" ]			= computeShader( "../src/projects/PathTracing/CellarDoor/shaders/ray.cs.glsl" ).shaderHandle;
		shaders[ "Buffer Copy" ]	= computeShader( "../src/projects/PathTracing/CellarDoor/shaders/bufferCopy.cs.glsl" ).shaderHandle;
	}

	void ComputeTileList () {
		cellarDoorConfig.updateTiles.clear();
		for ( int y = 0; y < cellarDoorConfig.dimensions.y; y += 64 ){
			for ( int x = 0; x < cellarDoorConfig.dimensions.x; x += 64 ){
				for ( int z = 0; z < cellarDoorConfig.dimensions.z; z += 64 ){
					cellarDoorConfig.updateTiles.push_back( ivec3( x, y, z ) );
				}
			}
		}
		evaluateBar.total = cellarDoorConfig.numTiles = cellarDoorConfig.updateTiles.size();
		// auto rng = std::default_random_engine {};
		// std::shuffle( std::begin( cellarDoorConfig.updateTiles ), std::end( cellarDoorConfig.updateTiles ), rng );
	}

	void ComputeSpherePacking () {

		const uint32_t maxSpheres = cellarDoorConfig.maxSpheres + cellarDoorConfig.incrementalConfig.sphereTrim; // 16-bit addressing gives us 65k max
		std::deque< vec4 > sphereLocationsPlusColors;

		// stochastic sphere packing, inside the volume
		vec3 min = vec3( -cellarDoorConfig.dimensions.x / 2.0f, -cellarDoorConfig.dimensions.y / 2.0f, -cellarDoorConfig.dimensions.z / 2.0f );
		vec3 max = vec3(  cellarDoorConfig.dimensions.x / 2.0f,  cellarDoorConfig.dimensions.y / 2.0f,  cellarDoorConfig.dimensions.z / 2.0f );
		uint32_t maxIterations = 500;

		// I think this is the easiest way to handle things that don't terminate on their own
		uint32_t attemptsRemaining = cellarDoorConfig.incrementalConfig.maxAllowedTotalIterations;

		// float currentRadius = 0.866f * cellarDoorConfig.dimensions.z / 2.0f;
		float currentRadius = cellarDoorConfig.incrementalConfig.radiiInitialValue;
		rng paletteRefVal = rng( cellarDoorConfig.incrementalConfig.paletteRefMin, cellarDoorConfig.incrementalConfig.paletteRefMax );
		rng alphaGen = rng( cellarDoorConfig.incrementalConfig.alphaGenMin, cellarDoorConfig.incrementalConfig.alphaGenMax );
		rngN paletteRefJitter = rngN( 0.0f, cellarDoorConfig.incrementalConfig.paletteRefJitter );
		float currentPaletteVal = paletteRefVal();

		while ( ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres && !pQuit ) { // watch for program quit
			rng x = rng( min.x + currentRadius, max.x - currentRadius );
			rng y = rng( min.y + currentRadius, max.y - currentRadius );
			rng z = rng( min.z + currentRadius, max.z - currentRadius );
			uint32_t iterations = maxIterations;

			// redundant check, but I think it's the easiest way not to run over
			while ( iterations-- && ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres && attemptsRemaining-- && !pQuit ) {
				// generate point inside the parent cube
				vec3 checkP = vec3( x(), y(), z() );
				// check for intersection against all other spheres
				bool foundIntersection = false;
				for ( uint32_t idx = 0; idx < sphereLocationsPlusColors.size() / 2; idx++ ) {
					vec4 otherSphere = sphereLocationsPlusColors[ 2 * idx ];
					if ( glm::distance( checkP, otherSphere.xyz() ) < ( currentRadius + otherSphere.a ) ) {
						foundIntersection = true;
						break;
					}
				}
				// if there are no intersections, add it to the list with the current material
				if ( !foundIntersection ) {
					sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
					sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( std::clamp( currentPaletteVal + paletteRefJitter(), 0.0f, 1.0f ) ), alphaGen() ) );
				}

			// need to determine which is the greater percentage:
				// number of spheres / max spheres
				float sphereFrac = float( sphereLocationsPlusColors.size() / 2.0f ) / float( cellarDoorConfig.maxSpheres );
				// number of attempts taken as a fraction of max attempts
				float attemptFrac = float( cellarDoorConfig.incrementalConfig.maxAllowedTotalIterations - attemptsRemaining ) / float( cellarDoorConfig.incrementalConfig.maxAllowedTotalIterations );

				// the greater of the two should set the level on the progress bar
				generateBar.done = std::max( sphereFrac, attemptFrac );
				generateBar.total = 1.0f;

				if ( !attemptsRemaining ) break;
			}
			if ( !attemptsRemaining ) break;

			// if you've gone max iterations, time to halve the radius and double the max iteration count, get new material
			currentPaletteVal = paletteRefVal();
			currentRadius *= cellarDoorConfig.incrementalConfig.radiiStepShrink;
			maxIterations *= cellarDoorConfig.incrementalConfig.iterationMultiplier;

			// this replaces explicit shrinking on each axis
			min.x *= cellarDoorConfig.incrementalConfig.boundsStepShrink.x; max.x *= cellarDoorConfig.incrementalConfig.boundsStepShrink.x;
			min.y *= cellarDoorConfig.incrementalConfig.boundsStepShrink.y; max.y *= cellarDoorConfig.incrementalConfig.boundsStepShrink.y;
			min.z *= cellarDoorConfig.incrementalConfig.boundsStepShrink.z; max.z *= cellarDoorConfig.incrementalConfig.boundsStepShrink.z;

		}

		// ================================================================================================================

		// because it's a deque, I can pop the front N off ( see cellarDoorConfig.sphereTrim, above )
		// also, if I've got some kind of off by one issue, however I want to handle the zero reserve value, that'll be easy

		for ( uint32_t i = 0; i < cellarDoorConfig.incrementalConfig.sphereTrim; i++ ) {
			sphereLocationsPlusColors.pop_front();
		}

		cellarDoorConfig.sphereBuffer.resize( 0 );
		cellarDoorConfig.sphereBuffer.reserve( sphereLocationsPlusColors.size() );
		for ( auto& val : sphereLocationsPlusColors ) {
			cellarDoorConfig.sphereBuffer.push_back( val );
		}

		// if we aborted from running out of iterations, avoid seg fault in glBufferData
		cellarDoorConfig.sphereBuffer.resize( cellarDoorConfig.maxSpheres * 2 );

			// make sure that the generate progress bar reports 100% at this stage

		// ================================================================================================================
	}

	void ComputePerlinPacking () {
		// const uint32_t maxSpheres = cellarDoorConfig.maxSpheres + cellarDoorConfig.sphereTrim; // 16-bit addressing gives us 65k max
		const uint32_t maxSpheres = cellarDoorConfig.maxSpheres; // 16-bit addressing gives us 65k max
		std::vector< vec4 > sphereLocationsPlusColors;

		// stochastic sphere packing, inside the volume
		vec3 min = vec3( -cellarDoorConfig.dimensions.x / 2.0f, -cellarDoorConfig.dimensions.y / 2.0f, -cellarDoorConfig.dimensions.z / 2.0f );
		vec3 max = vec3(  cellarDoorConfig.dimensions.x / 2.0f,  cellarDoorConfig.dimensions.y / 2.0f,  cellarDoorConfig.dimensions.z / 2.0f );
		uint32_t maxIterations = cellarDoorConfig.perlinConfig.maxAllowedTotalIterations;
		uint32_t iterations = maxIterations;

		// data generation
		rng radiusGen = rng( 0.25f, 1.25f );
		rngN radiusJitter = rngN( 0.0f, cellarDoorConfig.perlinConfig.radiusJitter );
		rngN paletteJitter = rngN( 0.0f, cellarDoorConfig.perlinConfig.paletteRefJitter );
		float padding = cellarDoorConfig.perlinConfig.padding;
		PerlinNoise p;

		// generate point inside the parent cube
		rng x = rng( min.x + padding, max.x - padding );
		rng y = rng( min.y + padding, max.y - padding );
		rng z = rng( min.z + padding, max.z - padding );

		while ( ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres && iterations-- && !pQuit ) {
			vec3 checkP = vec3( x(), y(), z() );
			float noiseValue = p.noise(
				checkP.x / cellarDoorConfig.perlinConfig.noiseScalar.x + cellarDoorConfig.perlinConfig.noiseOffset.x,
				checkP.y / cellarDoorConfig.perlinConfig.noiseScalar.y + cellarDoorConfig.perlinConfig.noiseOffset.y,
				checkP.z / cellarDoorConfig.perlinConfig.noiseScalar.z + cellarDoorConfig.perlinConfig.noiseOffset.z );

			checkP.z *= RemapRange( noiseValue, 0.0f, 1.0f, cellarDoorConfig.perlinConfig.zSquashMin, cellarDoorConfig.perlinConfig.zSquashMax );

			// something to skip spheres outside of a certain range
			// if ( noiseValue > 0.75f ) {
				// continue;
			// }

			// determine radius, from the noise field
			float currentRadius = ( radiusGen() * RemapRange( std::pow( noiseValue, cellarDoorConfig.perlinConfig.rampPower ), 0.0f, 1.0f, cellarDoorConfig.perlinConfig.radiusMin, cellarDoorConfig.perlinConfig.radiusMax ) + radiusJitter() ) * cellarDoorConfig.dimensions.z / 24.0f;

			// check for intersection against all other spheres
			bool foundIntersection = false;
			for ( uint32_t idx = 0; idx < sphereLocationsPlusColors.size() / 2; idx++ ) {
				vec4 otherSphere = sphereLocationsPlusColors[ 2 * idx ];
				if ( glm::distance( checkP, otherSphere.xyz() ) < ( currentRadius + otherSphere.a ) ) {
					foundIntersection = true;
					break;
				}
			}

			// if there are no intersections, add it to the list with the current material
			if ( !foundIntersection ) {
				sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
				// sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( std::clamp( noiseValue, 0.0f, 1.0f ) ), alphaGen() ) );
				// sphereLocationsPlusColors.push_back( vec4( vec3( std::clamp( abs( ( noiseValue - 0.6f ) * 2.5f ), 0.0f, 1.0f ) ), alphaGen() ) );
				sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( RemapRange( std::clamp( noiseValue, 0.0f, 1.0f ), 0.0f, 1.0f, cellarDoorConfig.perlinConfig.paletteRefMin, cellarDoorConfig.perlinConfig.paletteRefMax ) + paletteJitter() ), noiseValue ) );

			// need to determine which is the greater percentage:
				// number of spheres / max spheres
				float sphereFrac = float( sphereLocationsPlusColors.size() / 2.0f ) / float( cellarDoorConfig.maxSpheres );
				// number of attempts taken as a fraction of max attempts
				float attemptFrac = float( cellarDoorConfig.perlinConfig.maxAllowedTotalIterations - iterations ) / float( cellarDoorConfig.perlinConfig.maxAllowedTotalIterations );

				// the greater of the two should set the level on the progress bar
				generateBar.done = std::max( sphereFrac, attemptFrac );
				generateBar.total = 1.0f;
			}
		}

		// ================================================================================================================

		// send the SSBO
		cellarDoorConfig.sphereBuffer.resize( 0 );
		cellarDoorConfig.sphereBuffer.reserve( sphereLocationsPlusColors.size() );
		for ( auto& val : sphereLocationsPlusColors ) {
			cellarDoorConfig.sphereBuffer.push_back( val );
		}

		// if we aborted from running out of iterations, avoid seg fault in glBufferData
		cellarDoorConfig.sphereBuffer.resize( cellarDoorConfig.maxSpheres * 2 );

		// ================================================================================================================
	}

	void ComputeTorusPacking () {
		// point in a torus, would be a cool extension to the packing logic
			// theta, phi, for placement on the torus surface - additional term for distance along that minor radius
		const uint32_t maxSpheres = cellarDoorConfig.maxSpheres; // 16-bit addressing gives us 65k max
		std::vector< vec4 > sphereLocationsPlusColors;

		// ================================================================================================================
		// the idea is basically the same as the others, but based on points generated in a torus, rather than uniformly
		// in an AABB in space. I think this should  be fairly straightforward, I think tight packing is probably the easiest
		// case, but I am imagining that you could vary the radii about the major and minor axes with noise reads in space.

		// initial impl can just be this kind of almost-uniform point in a torus thing
		rng theta 	= rng( 0.0f, 2.0f * pi );
		rng phi 	= rng( 0.0f, 2.0f * pi );
		rng r 		= rng( 0.0f, 1.0f );

		uint32_t maxIterations = cellarDoorConfig.torusConfig.maxAllowedTotalIterations;
		uint32_t iterations = maxIterations;

		// data generation
		rng radiusGen = rng( 0.25f, 1.25f );
		rngN radiusJitter = rngN( 0.0f, cellarDoorConfig.torusConfig.sphereRadiusJitter );
		rngN paletteJitter = rngN( 0.0f, cellarDoorConfig.torusConfig.paletteRefJitter );
		PerlinNoise p;

		while ( ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres && iterations-- && !pQuit ) {

			// generating a point inside the torus
			vec3 checkP;
			// sqrt is normalizing factor, so as not to concentrate at the center of the ring
			checkP = vec3( sqrt( r() ) * cellarDoorConfig.torusConfig.minorRadius, 0.0f, 0.0f );
			checkP = ( glm::rotate( phi(), vec3( 0.0f, 0.0f, 1.0f ) ) * vec4( checkP, 1.0f ) ).xyz();
			checkP.x += cellarDoorConfig.torusConfig.majorRadius;
			checkP = ( glm::rotate( theta(), vec3( 0.0f, 1.0f, 0.0f ) ) * vec4( checkP, 1.0f ) ).xzy();

			float noiseValue = p.noise(
				checkP.x / cellarDoorConfig.torusConfig.noiseScalar.x + cellarDoorConfig.torusConfig.noiseOffset.x,
				checkP.y / cellarDoorConfig.torusConfig.noiseScalar.y + cellarDoorConfig.torusConfig.noiseOffset.y,
				checkP.z / cellarDoorConfig.torusConfig.noiseScalar.z + cellarDoorConfig.torusConfig.noiseOffset.z );

			// determine radius, from the noise field
			float currentRadius = ( radiusGen() * RemapRange( std::pow( noiseValue, cellarDoorConfig.torusConfig.rampPower ), 0.0f, 1.0f, cellarDoorConfig.torusConfig.sphereRadiusMin, cellarDoorConfig.torusConfig.sphereRadiusMax ) + radiusJitter() ) * cellarDoorConfig.dimensions.z / 24.0f;

			// check for intersection against all other spheres
			bool foundIntersection = false;
			for ( uint32_t idx = 0; idx < sphereLocationsPlusColors.size() / 2; idx++ ) {
				vec4 otherSphere = sphereLocationsPlusColors[ 2 * idx ];
				if ( glm::distance( checkP, otherSphere.xyz() ) < ( currentRadius + otherSphere.a ) ) {
					foundIntersection = true;
					break;
				}
			}

			// if there are no intersections, add it to the list with the current material
			if ( !foundIntersection ) {
				sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
				// sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( std::clamp( noiseValue, 0.0f, 1.0f ) ), alphaGen() ) );
				// sphereLocationsPlusColors.push_back( vec4( vec3( std::clamp( abs( ( noiseValue - 0.6f ) * 2.5f ), 0.0f, 1.0f ) ), alphaGen() ) );
				sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( RemapRange( std::clamp( noiseValue, 0.0f, 1.0f ), 0.0f, 1.0f, cellarDoorConfig.torusConfig.paletteRefMin, cellarDoorConfig.torusConfig.paletteRefMax ) + paletteJitter() ), noiseValue ) );

			// need to determine which is the greater percentage:
				// number of spheres / max spheres
				float sphereFrac = float( sphereLocationsPlusColors.size() / 2.0f ) / float( cellarDoorConfig.maxSpheres );
				// number of attempts taken as a fraction of max attempts
				float attemptFrac = float( cellarDoorConfig.torusConfig.maxAllowedTotalIterations - iterations ) / float( cellarDoorConfig.torusConfig.maxAllowedTotalIterations );

				// the greater of the two should set the level on the progress bar
				generateBar.done = std::max( sphereFrac, attemptFrac );
				generateBar.total = 1.0f;
			}
		}

		// ================================================================================================================

		// send the SSBO
		cellarDoorConfig.sphereBuffer.resize( 0 );
		cellarDoorConfig.sphereBuffer.reserve( sphereLocationsPlusColors.size() );
		for ( auto& val : sphereLocationsPlusColors ) {
			cellarDoorConfig.sphereBuffer.push_back( val );
		}

		// if we aborted from running out of iterations, avoid seg fault in glBufferData
		cellarDoorConfig.sphereBuffer.resize( cellarDoorConfig.maxSpheres * 2 );

		// ================================================================================================================
	}

	void ComputeUpdateOffsets () {
		cellarDoorConfig.offsets.clear();
		cellarDoorConfig.lightingRemaining = lightingBar.total = 8 * 8 * 8;

		// // generate the list of offsets via shuffle
		// for ( int x = 0; x < 8; x++ )
		// for ( int y = 0; y < 8; y++ )
		// for ( int z = 0; z < 8; z++ ) {
		// 	cellarDoorConfig.offsets.push_back( ivec3( x, y, z ) );
		// }
		// auto rng = std::default_random_engine {};
		// std::shuffle( std::begin( cellarDoorConfig.offsets ), std::end( cellarDoorConfig.offsets ), rng );

		// ======================================================================

		// probably move this to the bayer header - also rewrite that, for the 2d case, to generalize it to any power of 2 size

		// simplify shit below
		#define index3(v,n) (uint32_t(v.x)+uint32_t(v.y)*n+uint32_t(v.z)*n*n)

		// derived from 2d bayer, the recursive construction
		// 0: 1->2, 1:  2->4, 2: 4->8
		std::vector< uint32_t > values = { 0 };
		for ( int i = 0; i < 3; i++ ) {
			// the size is x8 each step
			const size_t prevSize = values.size();
			std::vector< uint32_t > newValues;
			newValues.resize( prevSize * 8 );

			// source edge size is 2^i, 1, 2, 4 for the three iterations
			const int64_t edgeSizePrev = intPow( 2, i );
			const int64_t edgeSizeNext = intPow( 2, i + 1 );

			// the cantidate pattern
			const ivec3 iPattern [] = {
				ivec3( 0, 0, 1 ),
				ivec3( 1, 1, 0 ),
				ivec3( 0, 1, 1 ),
				ivec3( 1, 0, 0 ),
				ivec3( 0, 1, 0 ),
				ivec3( 1, 0, 1 ),
				ivec3( 0, 0, 0 ),
				ivec3( 1, 1, 1 ),
			};

			// for each value in the current set of offsets in iPattern
			for ( int x = 0; x < edgeSizePrev; x++ )
			for ( int y = 0; y < edgeSizePrev; y++ )
			for ( int z = 0; z < edgeSizePrev; z++ ) {
				ivec3 basePt = ivec3( x, y, z );
				uint32_t v = values[ index3( basePt, edgeSizePrev ) ];

				for ( int i = 0; i < 8; i++ ) {
					ivec3 pt = basePt + iPattern[ i ] * int( edgeSizePrev );
					newValues[ index3( pt, edgeSizeNext ) ] = v * 8 + i;
				}
			}

			// prep for next iteration
			values.clear();
			values = newValues;
		}

		// for ( int z = 0; z < 8; z++ ) {
		// 	for ( int y = 0; y < 8; y++ ) {
		// 		for ( int x = 0; x < 8; x++ ) {
		// 			cout << fixedWidthNumberString( values[ index3( ivec3( x, y, z ), 8 ) ], 3, ' ' ) << " ";
		// 		}
		// 		cout << endl;
		// 	}
		// 	cout << endl;
		// }

		// iterate through all of them, and get the result as ivec3s in cellarDoorConfig.offsets
		cellarDoorConfig.offsets.resize( 8 * 8 * 8 );
		for ( int x = 0; x < 8; x++ )
		for ( int y = 0; y < 8; y++ )
		for ( int z = 0; z < 8; z++ ) {
			ivec3 loc = ivec3( x, y, z );
			uint32_t idx = index3( loc, 8 );
			cellarDoorConfig.offsets[ values[ idx ] ] = loc;
		}

		// cout << "Offsets List:" << endl;
		// for ( auto& v : cellarDoorConfig.offsets ) {
		// 	cout << "  " << glm::to_string( v ) << endl;
		// }


		#undef index
		#undef index3
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		const bool * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		const SDL_Keymod k		= SDL_GetModState();
		const bool shift		= ( k & SDL_KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		// screenshot
		cellarDoorConfig.userRequestedScreenshot = false; // one shot, repeated trigger is not desired
		if ( state[ SDL_SCANCODE_T ] && shift ) { cellarDoorConfig.userRequestedScreenshot = true; }
		if ( state[ SDL_SCANCODE_R ] && shift ) { ResetForwardPtBuffers(); }

		// zoom in and out
		if ( state[ SDL_SCANCODE_LEFTBRACKET ] )  { cellarDoorConfig.scale /= shift ? 0.9f : 0.99f; }
		if ( state[ SDL_SCANCODE_RIGHTBRACKET ] ) { cellarDoorConfig.scale *= shift ? 0.9f : 0.99f; }

		// vim-style x,y offsetting
		if ( state[ SDL_SCANCODE_H ] ) { cellarDoorConfig.viewOffset.x += shift ? 5.0f : 1.0f; }
		if ( state[ SDL_SCANCODE_J ] ) { cellarDoorConfig.viewOffset.y -= shift ? 5.0f : 1.0f; }
		if ( state[ SDL_SCANCODE_K ] ) { cellarDoorConfig.viewOffset.y += shift ? 5.0f : 1.0f; }
		if ( state[ SDL_SCANCODE_L ] ) { cellarDoorConfig.viewOffset.x -= shift ? 5.0f : 1.0f; }

		// hot recompile
		if ( state[ SDL_SCANCODE_Y ] ) {
			CompileShaders();
		}
	}

	void ResetForwardPtBuffers () {
		Image_4U zeroes( cellarDoorConfig.dimensions.x, cellarDoorConfig.dimensions.y * cellarDoorConfig.dimensions.z );
		GLuint handle = textureManager.Get( "Red Atomic" );
		glBindTexture( GL_TEXTURE_3D, handle );
		glTexImage3D( GL_TEXTURE_3D, 0, GL_R32UI, cellarDoorConfig.dimensions.x, cellarDoorConfig.dimensions.y, cellarDoorConfig.dimensions.z, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, ( void * ) zeroes.GetImageDataBasePtr() );
		handle = textureManager.Get( "Blue Atomic" );
		glBindTexture( GL_TEXTURE_3D, handle );
		glTexImage3D( GL_TEXTURE_3D, 0, GL_R32UI, cellarDoorConfig.dimensions.x, cellarDoorConfig.dimensions.y, cellarDoorConfig.dimensions.z, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, ( void * ) zeroes.GetImageDataBasePtr() );
		handle = textureManager.Get( "Green Atomic" );
		glBindTexture( GL_TEXTURE_3D, handle );
		glTexImage3D( GL_TEXTURE_3D, 0, GL_R32UI, cellarDoorConfig.dimensions.x, cellarDoorConfig.dimensions.y, cellarDoorConfig.dimensions.z, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, ( void * ) zeroes.GetImageDataBasePtr() );
		handle = textureManager.Get( "Count Atomic" );
		glBindTexture( GL_TEXTURE_3D, handle );
		glTexImage3D( GL_TEXTURE_3D, 0, GL_R32UI, cellarDoorConfig.dimensions.x, cellarDoorConfig.dimensions.y, cellarDoorConfig.dimensions.z, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, ( void * ) zeroes.GetImageDataBasePtr() );
	}

	void ControlsWindow () {
		ImGui::Begin( "Controls", NULL, 0 );
		if ( ImGui::BeginTabBar( "Config Sections", ImGuiTabBarFlags_None ) ) {
			if ( ImGui::BeginTabItem( "Generation" ) ) {
				// prototype palette browser
				ImGui::SeparatorText( "Palette Browser" );
				std::vector< const char * > paletteLabels;
				if ( paletteLabels.size() == 0 ) {
					for ( auto& entry : palette::paletteListLocal ) {
						// copy to a cstr for use by imgui
						char * d = new char[ entry.label.length() + 1 ];
						std::copy( entry.label.begin(), entry.label.end(), d );
						d[ entry.label.length() ] = '\0';
						paletteLabels.push_back( d );
					}
				}

				ImGui::Combo( "Palette", &palette::PaletteIndex, paletteLabels.data(), paletteLabels.size() );
				ImGui::SameLine();
				if ( ImGui::Button( "Random" ) ) {
					palette::PickRandomPalette( false );
				}
				const size_t paletteSize = palette::paletteListLocal[ palette::PaletteIndex ].colors.size();
				ImGui::Text( "  Contains %.3lu colors:", palette::paletteListLocal[ palette::PaletteIndex ].colors.size() );
				// handle max < min
				const  bool isIncremental = ( cellarDoorConfig.jobType == 0 );
				float minVal = isIncremental ? cellarDoorConfig.incrementalConfig.paletteRefMin : cellarDoorConfig.perlinConfig.paletteRefMin;
				float maxVal = isIncremental ? cellarDoorConfig.incrementalConfig.paletteRefMax : cellarDoorConfig.perlinConfig.paletteRefMax;
				float realSelectedMin = std::min( minVal, maxVal );
				float realSelectedMax = std::max( minVal, maxVal );
				size_t minShownIdx = std::floor( realSelectedMin * ( paletteSize - 1 ) );
				size_t maxShownIdx = std::ceil( realSelectedMax * ( paletteSize - 1 ) );

				bool finished = false;
				for ( int y = 0; y < 8; y++ ) {
					ImGui::Text( " " );
					for ( int x = 0; x < 32; x++ ) {

						// terminate when you run out of colors
						const uint32_t index = x + 32 * y;
						if ( index >= paletteSize ) {
							finished = true;
						}

						// show color, or black if past the end of the list
						ivec4 color = ivec4( 0 );
						if ( !finished ) {
							color = ivec4( palette::paletteListLocal[ palette::PaletteIndex ].colors[ index ], 255 );
							// determine if it is in the active range
							if ( index < minShownIdx || index > maxShownIdx ) {
								color.a = 64; // dim inactive entries
							}
						} 
						ImGui::SameLine();
						ImGui::TextColored( ImVec4( color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f ), "@" );

					}
				}

				ImGui::SeparatorText( "Sphere Packing Config" );
				// build config struct, pass to the functions
					// perlin or incremental version

				ImGui::Text( " Method: " );
				ImGui::SameLine();
				ImGui::RadioButton( " Incremental ", &cellarDoorConfig.jobType, 0 );
				ImGui::SameLine();
				ImGui::RadioButton( " Perlin ", &cellarDoorConfig.jobType, 1 );
				ImGui::SameLine();
				ImGui::RadioButton( " Torus ", &cellarDoorConfig.jobType, 2 );

				// only show the controls for one at a time, to read easier
				if ( cellarDoorConfig.jobType == 0 ) {	// incremental version

					ImGui::SliderFloat( "Palette Min", &cellarDoorConfig.incrementalConfig.paletteRefMin, 0.0f, 1.0f );
					ImGui::SliderFloat( "Palette Max", &cellarDoorConfig.incrementalConfig.paletteRefMax, 0.0f, 1.0f );
					ImGui::SliderFloat( "Palette Jitter", &cellarDoorConfig.incrementalConfig.paletteRefJitter, 0.0f, 0.2f );
					ImGui::Text( " " );
					ImGui::SliderFloat( "Alpha Min", &cellarDoorConfig.incrementalConfig.alphaGenMin, 0.0f, 1.0f );
					ImGui::SliderFloat( "Alpha Max", &cellarDoorConfig.incrementalConfig.alphaGenMax, 0.0f, 1.0f );
					ImGui::Text( " " );
					ImGui::SliderFloat( "Initial Radius", &cellarDoorConfig.incrementalConfig.radiiInitialValue, 1.0f, 300.0f );
					ImGui::SliderFloat( "Radius Step Shrink", &cellarDoorConfig.incrementalConfig.radiiStepShrink, 0.0f, 1.0f );
					ImGui::SliderFloat( "Iteration Count Multiplier", &cellarDoorConfig.incrementalConfig.iterationMultiplier, 0.0f, 10.0f );
					ImGui::SliderInt( "Max Total Iterations", ( int * ) &cellarDoorConfig.incrementalConfig.maxAllowedTotalIterations, 0, 100000000 );
					ImGui::Text( " " );
					ImGui::SliderFloat( "X Bounds Step Shrink", &cellarDoorConfig.incrementalConfig.boundsStepShrink.x, 0.0f, 1.0f );
					ImGui::SliderFloat( "Y Bounds Step Shrink", &cellarDoorConfig.incrementalConfig.boundsStepShrink.y, 0.0f, 1.0f );
					ImGui::SliderFloat( "Z Bounds Step Shrink", &cellarDoorConfig.incrementalConfig.boundsStepShrink.z, 0.0f, 1.0f );
					ImGui::Text( " " );
					ImGui::SliderInt( "Trim", ( int* ) &cellarDoorConfig.incrementalConfig.sphereTrim, 0, 100000 );

				} else if ( cellarDoorConfig.jobType == 1 ) { // perlin mode

					ImGui::SliderFloat( "Palette Min", &cellarDoorConfig.perlinConfig.paletteRefMin, 0.0f, 1.0f );
					ImGui::SliderFloat( "Palette Max", &cellarDoorConfig.perlinConfig.paletteRefMax, 0.0f, 1.0f );
					ImGui::SliderFloat( "Palette Jitter", &cellarDoorConfig.perlinConfig.paletteRefJitter, 0.0f, 0.2f );
					ImGui::Text( " " );
					ImGui::SliderFloat( "Radius Min", &cellarDoorConfig.perlinConfig.radiusMin, 0.0f, 100.0f );
					ImGui::SliderFloat( "Radius Max", &cellarDoorConfig.perlinConfig.radiusMax, 0.0f, 100.0f );
					ImGui::SliderFloat( "Radius Jitter", &cellarDoorConfig.perlinConfig.radiusJitter, 0.0f, 0.2f );
					ImGui::SliderFloat( "Ramp Power", &cellarDoorConfig.perlinConfig.rampPower, 0.0f, 0.2f );
					ImGui::Text( " " );
					ImGui::SliderFloat( "zSquash Min", &cellarDoorConfig.perlinConfig.zSquashMin, 0.0f, 2.0f );
					ImGui::SliderFloat( "zSquash Max", &cellarDoorConfig.perlinConfig.zSquashMax, 0.0f, 2.0f );
					ImGui::SliderFloat( "Padding", &cellarDoorConfig.perlinConfig.padding, 0.0f, 200.0f );
					ImGui::Text( " " );
					ImGui::SliderFloat( "X Noise Scale", &cellarDoorConfig.perlinConfig.noiseScalar.x, -1000.0f, 1000.0f );
					ImGui::SliderFloat( "Y Noise Scale", &cellarDoorConfig.perlinConfig.noiseScalar.y, -1000.0f, 1000.0f );
					ImGui::SliderFloat( "Z Noise Scale", &cellarDoorConfig.perlinConfig.noiseScalar.z, -1000.0f, 1000.0f );
					ImGui::Text( " " );
					ImGui::SliderInt( "Max Total Iterations", ( int * ) &cellarDoorConfig.perlinConfig.maxAllowedTotalIterations, 0, 100000000 );

				} else if ( cellarDoorConfig.jobType == 2 ) {

					// torus mode
					ImGui::SliderFloat( "Palette Min", &cellarDoorConfig.torusConfig.paletteRefMin, 0.0f, 1.0f );
					ImGui::SliderFloat( "Palette Max", &cellarDoorConfig.torusConfig.paletteRefMax, 0.0f, 1.0f );
					ImGui::SliderFloat( "Palette Jitter", &cellarDoorConfig.torusConfig.paletteRefJitter, 0.0f, 0.2f );
					ImGui::Text( " " );
					ImGui::SliderFloat( "Minor Radius", &cellarDoorConfig.torusConfig.minorRadius, 0.0f, 500.0f );
					ImGui::SliderFloat( "Major Radius", &cellarDoorConfig.torusConfig.majorRadius, 0.0f, 500.0f );
					ImGui::Text( " " );
					ImGui::SliderFloat( "Radius Min", &cellarDoorConfig.torusConfig.sphereRadiusMin, 0.0f, 100.0f );
					ImGui::SliderFloat( "Radius Max", &cellarDoorConfig.torusConfig.sphereRadiusMax, 0.0f, 100.0f );
					ImGui::SliderFloat( "Radius Jitter", &cellarDoorConfig.torusConfig.sphereRadiusJitter, 0.0f, 0.2f );
					ImGui::SliderFloat( "Ramp Power", &cellarDoorConfig.torusConfig.rampPower, 0.0f, 0.2f );
					ImGui::Text( " " );
					ImGui::SliderFloat( "X Noise Scale", &cellarDoorConfig.torusConfig.noiseScalar.x, -1000.0f, 1000.0f );
					ImGui::SliderFloat( "Y Noise Scale", &cellarDoorConfig.torusConfig.noiseScalar.y, -1000.0f, 1000.0f );
					ImGui::SliderFloat( "Z Noise Scale", &cellarDoorConfig.torusConfig.noiseScalar.z, -1000.0f, 1000.0f );
					ImGui::Text( " " );
					ImGui::SliderInt( "Max Total Iterations", ( int * ) &cellarDoorConfig.torusConfig.maxAllowedTotalIterations, 0, 100000000 );

				}
				
				// move outside the dynamic area
				if ( ImGui::Button( " Do It " ) ) {
						// eventually manage seeds better than this
					cellarDoorConfig.incrementalConfig.rngSeed = cellarDoorConfig.wangSeeder();
					cellarDoorConfig.perlinConfig.rngSeed = cellarDoorConfig.wangSeeder();

					// so the noise is not uniform run-to-run
					cellarDoorConfig.perlinConfig.noiseOffset = vec3(
						cellarDoorConfig.noiseOffset(),
						cellarDoorConfig.noiseOffset(),
						cellarDoorConfig.noiseOffset() );

					// worker thread sees this and begins work
					cellarDoorConfig.workerThreadShouldRun = true;
				}
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Rendering" ) ) {
				// controlling things like fog density, fog color
				// lighting controls, button to recompute lighting
					// or flag that gets set when a value changes with ImGui::IsItemEdited() etc

				ImGui::SeparatorText( " Thin Lens " );
				ImGui::SliderFloat( "Intensity", &cellarDoorConfig.thinLensIntensity, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
				ImGui::SliderFloat( "Distance", &cellarDoorConfig.thinLensDistance, 0.0f, 5.0f, "%.3f" );
				ImGui::SeparatorText( " Scene " );
				ImGui::Checkbox( "Bubble", &cellarDoorConfig.refractiveBubble );
				ImGui::SliderFloat( "Bubble IoR", &cellarDoorConfig.IoR, -6.0f, 6.0f );
				ImGui::SliderFloat( "Fog Scalar", &cellarDoorConfig.fogScalar, -0.001f, 0.001f, "%.6f", ImGuiSliderFlags_Logarithmic );
				ImGui::ColorEdit3( "Fog Color", ( float * ) &cellarDoorConfig.fogColor, ImGuiColorEditFlags_PickerHueWheel );
				ImGui::SeparatorText( " Tonemapping " );
				const char* tonemapModesList[] = {
					"None (Linear)",
					"ACES (Narkowicz 2015)",
					"Unreal Engine 3",
					"Unreal Engine 4",
					"Uncharted 2",
					"Gran Turismo",
					"Modified Gran Turismo",
					"Rienhard",
					"Modified Rienhard",
					"jt",
					"robobo1221s",
					"robo",
					"reinhardRobo",
					"jodieRobo",
					"jodieRobo2",
					"jodieReinhard",
					"jodieReinhard2"
				};
				ImGui::Combo("Tonemapping Mode", &tonemap.tonemapMode, tonemapModesList, IM_ARRAYSIZE( tonemapModesList ) );
				ImGui::SliderFloat( "Gamma", &tonemap.gamma, 0.0f, 3.0f );
				ImGui::SliderFloat( "PostExposure", &tonemap.postExposure, 0.0f, 5.0f );
				ImGui::SliderFloat( "Saturation", &tonemap.saturation, 0.0f, 4.0f );
				// ImGui::Checkbox( "Saturation Uses Improved Weight Vector", &tonemap.saturationImprovedWeights );
				ImGui::SliderFloat( "Color Temperature", &tonemap.colorTemp, 1000.0f, 40000.0f );
				if ( ImGui::Button( "Reset to Defaults" ) ) {
					TonemapDefaults();
				}
				ImGui::SeparatorText( " Other Rendering " );
				ImGui::SliderFloat( "Blend Amount", &cellarDoorConfig.blendAmount, 0.9f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
				
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	void ImguiPass () {
		ZoneScoped;
		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		ControlsWindow();

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );

			textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Dummy Draw" ], 2 );
			textureManager.BindImageForShader( "Color Buffer", "colorBuffer", shaders[ "Dummy Draw" ], 3 );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "time" ), SDL_GetTicks() / 1600.0f );

			static rng blueNoiseOffset = rng( 0, 512 );
			glUniform2i( glGetUniformLocation( shaders[ "Dummy Draw" ], "noiseOffset" ), blueNoiseOffset(), blueNoiseOffset() );

			const glm::mat3 inverseBasisMat = inverse( glm::mat3( -trident.basisX, -trident.basisY, -trident.basisZ ) );
			glUniformMatrix3fv( glGetUniformLocation( shaders[ "Dummy Draw" ], "invBasis" ), 1, false, glm::value_ptr( inverseBasisMat ) );
			glUniform3f( glGetUniformLocation( shaders[ "Dummy Draw" ], "blockSize" ), cellarDoorConfig.dimensions.x / 1024.0f,  cellarDoorConfig.dimensions.y / 1024.0f,  cellarDoorConfig.dimensions.z / 1024.0f );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "scale" ), cellarDoorConfig.scale );
			glUniform2f( glGetUniformLocation( shaders[ "Dummy Draw" ], "viewOffset" ), cellarDoorConfig.viewOffset.x, cellarDoorConfig.viewOffset.y );
			glUniform1i( glGetUniformLocation( shaders[ "Dummy Draw" ], "wangSeed" ), cellarDoorConfig.wangSeeder() );
			glUniform2f( glGetUniformLocation( shaders[ "Dummy Draw" ], "resolution" ), config.width, config.height );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "blendAmount" ), cellarDoorConfig.blendAmount );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "thinLensDistance" ), cellarDoorConfig.thinLensDistance );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "thinLensIntensity" ), cellarDoorConfig.thinLensIntensity );
			glUniform1i( glGetUniformLocation( shaders[ "Dummy Draw" ], "refractiveBubble" ), cellarDoorConfig.refractiveBubble );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "bubbleIoR" ), cellarDoorConfig.IoR );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "fogScalar" ), cellarDoorConfig.fogScalar );
			glUniform3f( glGetUniformLocation( shaders[ "Dummy Draw" ], "fogColor" ), cellarDoorConfig.fogColor.r, cellarDoorConfig.fogColor.g, cellarDoorConfig.fogColor.b );

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

		// handle the case where the user requested a screenshot - todo: why is it taking twice?
		if ( cellarDoorConfig.userRequestedScreenshot ) {
			cellarDoorConfig.userRequestedScreenshot = false;
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			ColorScreenShotWithFilename( string( "Output-" ) + timeDateString() + string( ".png" ) );
		}

		// shader to apply dithering
			// ...

		// other postprocessing
			// ...

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );

			// get status from engine local progress bars, write them above the timestamp
				// updated from worker thread, so we don't lock up like before

			textRenderer.DrawProgressBarString( 3, generateBar );
			textRenderer.DrawProgressBarString( 2, evaluateBar );
			textRenderer.DrawProgressBarString( 1, lightingBar );

			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );

			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// { // show trident with current orientation
		// 	scopedTimer Start( "Trident" );
		// 	trident.Update( textureManager.Get( "Display Texture" ) );
		// 	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		// }
	}

	// update thread
	std::thread workerThread { [=] () {
		while ( !pQuit ) { // while the program is running
			if ( !cellarDoorConfig.workerThreadShouldRun ) { // waiting
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
			} else {
				// reset the bars
				generateBar.done = evaluateBar.done = lightingBar.done = 0.0f;
				if ( cellarDoorConfig.jobType == 0 ) // incremental version
					ComputeSpherePacking();
				else if ( cellarDoorConfig.jobType == 1 ) // perlin verison
					ComputePerlinPacking();
				else if ( cellarDoorConfig.jobType == 2 ) // torus version
					ComputeTorusPacking();
				// else
					// blah blah, other generators
				// and the data is ready to send
				cellarDoorConfig.bufferReady = true;
				cellarDoorConfig.workerThreadShouldRun = false;
			}
		}
	}};

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );


		{
			glUseProgram( shaders[ "Ray" ] );

			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, cellarDoorConfig.sphereSSBO );
			textureManager.BindImageForShader( "Blue Noise", "blueNoiseTexture", shaders[ "Ray" ], 0 );
			textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Ray" ], 2 );
			textureManager.BindImageForShader( "Color Buffer", "colorBuffer", shaders[ "Ray" ], 3 );

			textureManager.BindImageForShader( "Red Atomic", "redAtomic", shaders[ "Ray" ], 4 );
			textureManager.BindImageForShader( "Green Atomic", "greenAtomic", shaders[ "Ray" ], 5 );
			textureManager.BindImageForShader( "Blue Atomic", "blueAtomic", shaders[ "Ray" ], 6 );
			textureManager.BindImageForShader( "Count Atomic", "countAtomic", shaders[ "Ray" ], 7 );

			static rng blueNoiseOffset = rng( 0, 512 );
			glUniform2i( glGetUniformLocation( shaders[ "Ray" ], "noiseOffset" ), blueNoiseOffset(), blueNoiseOffset() );
			glUniform1i( glGetUniformLocation( shaders[ "Ray" ], "wangSeed" ), cellarDoorConfig.wangSeeder() );

			// glDispatchCompute( 16, 16, 1 );
			glDispatchCompute( 4, 4, 1 );
		}

		{
			glUseProgram( shaders[ "Buffer Copy" ] );
			textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Buffer Copy" ], 2 );
			textureManager.BindImageForShader( "Color Buffer", "colorBuffer", shaders[ "Buffer Copy" ], 3 );

			textureManager.BindImageForShader( "Red Atomic", "redAtomic", shaders[ "Buffer Copy" ], 4 );
			textureManager.BindImageForShader( "Green Atomic", "greenAtomic", shaders[ "Buffer Copy" ], 5 );
			textureManager.BindImageForShader( "Blue Atomic", "blueAtomic", shaders[ "Buffer Copy" ], 6 );
			textureManager.BindImageForShader( "Count Atomic", "countAtomic", shaders[ "Buffer Copy" ], 7 );

			glDispatchCompute(
				( cellarDoorConfig.dimensions.x + 7 ) / 8,
				( cellarDoorConfig.dimensions.y + 7 ) / 8,
				( cellarDoorConfig.dimensions.z + 7 ) / 8 );
		}


		// generate bar is updated internal to the generate functions - this is hacky but whatever
		if ( cellarDoorConfig.workerThreadShouldRun || cellarDoorConfig.bufferReady ) {
			evaluateBar.done = lightingBar.done= 0.0f;
		} else {
			evaluateBar.done = cellarDoorConfig.numTiles - cellarDoorConfig.updateTiles.size();
			lightingBar.done = 8 * 8 * 8 - cellarDoorConfig.lightingRemaining;
		}

		if ( cellarDoorConfig.bufferReady ) {

			// update state
			cellarDoorConfig.bufferReady = false;
			cellarDoorConfig.workerThreadShouldRun = false;

			// send the data to the SSBO
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, cellarDoorConfig.sphereSSBO );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * cellarDoorConfig.maxSpheres, ( GLvoid * ) &cellarDoorConfig.sphereBuffer.data()[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, cellarDoorConfig.sphereSSBO );

			ComputeTileList(); // ready to go to the next stage
			ComputeUpdateOffsets();

		} else if ( cellarDoorConfig.updateTiles.size() != 0 ) {
			glUseProgram( shaders[ "Precompute" ] );

			// buffer setup
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, cellarDoorConfig.sphereSSBO );
			// textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Precompute" ], 2 );
			textureManager.BindImageForShader( "Color Buffer", "dataCacheBuffer", shaders[ "Precompute" ], 3 );

			// other uniforms
			ivec3 offset = cellarDoorConfig.updateTiles[ cellarDoorConfig.updateTiles.size() - 1 ];
			cellarDoorConfig.updateTiles.pop_back();
			glUniform3i( glGetUniformLocation( shaders[ "Precompute" ], "offset" ), offset.x, offset.y, offset.z );
			glUniform1i( glGetUniformLocation( shaders[ "Precompute" ], "wangSeed" ), cellarDoorConfig.incrementalConfig.rngSeed );
			glDispatchCompute( 8, 8, 8 );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		}

		// } else if ( cellarDoorConfig.lightingRemaining >= 0 ) {
		// 	glUseProgram( shaders[ "Lighting" ] );

		// 	// buffer setup
		// 	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, cellarDoorConfig.sphereSSBO );
		// 	// textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Lighting" ], 2 );
		// 	textureManager.BindImageForShader( "Color Buffer", "dataCacheBuffer", shaders[ "Lighting" ], 3 );

		// 	// other uniforms
		// 	ivec3 offset = cellarDoorConfig.offsets[ cellarDoorConfig.lightingRemaining-- ];
		// 	glUniform3i( glGetUniformLocation( shaders[ "Lighting" ], "offset" ), offset.x, offset.y, offset.z );
		// 	glDispatchCompute(
		// 		( ( cellarDoorConfig.dimensions.x + 7 ) / 8 + 7 ) / 8,
		// 		( ( cellarDoorConfig.dimensions.y + 7 ) / 8 + 7 ) / 8,
		// 		( ( cellarDoorConfig.dimensions.z + 7 ) / 8 + 7 ) / 8 );

		// 	// do a second one
		// 	if ( cellarDoorConfig.lightingRemaining >= 0 && !( cellarDoorConfig.workerThreadShouldRun || cellarDoorConfig.bufferReady ) ) {
		// 		// other uniforms
		// 		offset = cellarDoorConfig.offsets[ cellarDoorConfig.lightingRemaining-- ];
		// 		glUniform3i( glGetUniformLocation( shaders[ "Lighting" ], "offset" ), offset.x, offset.y, offset.z );
		// 		glDispatchCompute(
		// 			( ( cellarDoorConfig.dimensions.x + 7 ) / 8 + 7 ) / 8,
		// 			( ( cellarDoorConfig.dimensions.y + 7 ) / 8 + 7 ) / 8,
		// 			( ( cellarDoorConfig.dimensions.z + 7 ) / 8 + 7 ) / 8 );
		// 	}
		// 	// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		// }
	}

	void ColorScreenShotWithFilename ( const string filename ) {
		std::vector< float > imageBytesToSave;
		imageBytesToSave.resize( config.width * config.height * 4, 0 );
		glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Display Texture" ) );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
		Image_4F screenshot( config.width, config.height, &imageBytesToSave.data()[ 0 ] );
		screenshot.FlipVertical();

		// we have it as floats, now do gamma to match the visual in the framebuffer
		if ( config.SRGBFramebuffer )
			screenshot.GammaCorrect( 2.2f );
		
		screenshot.Save( filename );
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
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

int main ( int argc, char *argv[] ) {
	cellarDoor engineInstance;
	while( !engineInstance.MainLoop() );

	// program has terminated, time to die
	engineInstance.workerThread.join();

	return 0;
}
