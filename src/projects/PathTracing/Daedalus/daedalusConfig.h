#include "tileDispenser.h"

struct rngen_t {
	rngi wangSeeder = rngi( 0, 10000000 );
	rngi blueNoiseOffset = rngi( 0, 512 );
};

struct viewConfig_t {
	bool edgeLines = true;
	vec3 edgeLinesColor = vec3( 0.0618f );
	bool centerLines = true;
	vec3 centerLinesColor = vec3( 0.1618f, 0.0618f, 0.0f );
	bool ROTLines = true;
	vec3 ROTLinesColor = vec3( 0.0f, 0.0618f, 0.0618f );
	bool goldenLines = true;
	vec3 goldenLinesColor = vec3( 0.1618f, 0.0f, 0.0f );
	bool vignette = true;

	// main display, grid pan + zoom
	float outputZoom;
	vec2 outputOffset;

	vec2 clickPosition;
};

struct sceneConfig_t {
	// raymarch intersection config
	bool raymarchEnable;
	float raymarchMaxDistance;
	float raymarchUnderstep;
	int raymarchMaxSteps;
	float marbleRadius;

	// Hybrid DDA traversal
	bool ddaSpheresEnable;
	vec3 ddaSpheresBoundSize;
	int ddaSpheresResolution;
	// mode config? may require a lot of parameterization...

	// additional config? tbd
	bool maskedPlaneEnable;

	// config for the spheres
	bool explicitListEnable;
	int numExplicitPrimitives;
	GLuint explicitPrimitiveSSBO;
	std::vector< vec4 > explicitPrimitiveData;

	// sky config
	uvec2 skyDims;
	bool skyNeedsUpdate;
	int skyMode;
	vec3 skyConstantColor1;
	vec3 skyConstantColor2;
	int sunThresh;
	float skyTime;
	bool skyInvert;
	float skyBrightnessScalar;
	float skyClamp;
};

struct colorGradingConfig_t {
	// brightness histograms
	bool updateHistogram = false;
	GLuint colorHistograms;

	// waveform displays
	bool updateParade = false;
	bool updateWaveform = false;
	GLuint waveformMins;
	GLuint waveformMaxs;
	GLuint waveformGlobalMax;

	bool updateVectorscope = false;
	GLuint vectorscopeMax;
};

struct renderConfig_t {
	int subpixelJitterMethod;
	float uvScalar;
	float exposure;
	float FoV;
	float epsilon;
	float maxDistance;

	vec3 viewerPosition;
	vec3 basisX;
	vec3 basisY;
	vec3 basisZ;

	bool thinLensEnable;
	float thinLensFocusDistance;
	float thinLensJitterRadiusInner;
	float thinLensJitterRadiusOuter;
	int bokehMode;
	int cameraType;
	float voraldoCameraScalar;
	bool cameraOriginJitter;
	int maxBounces;

	bool render = true;

	sceneConfig_t scene;
	colorGradingConfig_t grading;
};

struct postConfig_t {
	bool enableLensDistort = false;
	bool lensDistortNormalize = true;
	int lensDistortNumSamples = 100;
	vec3 lensDistortParametersStart = vec3( 0.0f, 0.02f, 0.0f );
	vec3 lensDistortParametersEnd = vec3( 0.00618f, 0.0f, 0.0f );
	bool lensDistortChromab = true;
};

struct daedalusConfig_t {

	daedalusConfig_t() {

		// ===============================

		// initialize with identity matrix
		modelMatrix = glm::mat4( 1.0f );
		panDolly = vec3( 0.0f );

		// this will need to come in from elsewhere, eventually - for now, hardcoded screen res
		outputWidth = 2560;
		outputHeight = 1440;

		// configuring the accumulator
		targetWidth = 1280;
		targetHeight = 720;
		tileSize = 256;

		// initialize the tile dispenser
		tiles = tileDispenser( tileSize, targetWidth, targetHeight );

		// setup performance monitor
		performanceHistorySamples = 250;
		timeHistory.resize( performanceHistorySamples );

		// ===============================

		// intialize the main view
		showConfigWindow = true;
		view.outputZoom = 1.0f;
		view.outputOffset = ( vec2( targetWidth, targetHeight ) - vec2( outputWidth, outputHeight ) ) / 2.0f;
		filterBlendAmount = 0.5f;
		filterEveryFrame = false;
		filterSelector = 0;

		view.clickPosition = vec2( 0.0f );

		// ===============================

		render.subpixelJitterMethod = 3;
		render.uvScalar = 1.0f;
		render.exposure = 1.0f;
		render.FoV = 0.618f;
		render.epsilon = 0.0001f;
		render.maxDistance = 100.0f;

		// render.viewerPosition = vec3( 0.0f );
		// render.basisX = vec3( 1.0f, 0.0f, 0.0f );
		// render.basisY = vec3( 0.0f, 1.0f, 0.0f );
		// render.basisZ = vec3( 0.0f, 0.0f, 1.0f );

		render.viewerPosition = vec3( 0.984f, 0.332f, -0.583f );
		render.basisX = vec3( 0.11f, -0.876f, -0.469f );
		render.basisY = vec3( 0.021f, -0.469f, 0.833f );
		render.basisZ = vec3( -0.994f, -0.107f, -0.033f );

		// thin lens config
		render.thinLensEnable = false;
		render.thinLensFocusDistance = 10.0f;
		render.thinLensJitterRadiusInner = 0.0005f;
		render.thinLensJitterRadiusOuter = 0.0008f;
		render.bokehMode = 18;
		render.cameraType = 0;
		render.voraldoCameraScalar = 1.0f;
		render.cameraOriginJitter = true;
		render.maxBounces = 10;

		// SDF raymarch geo
		render.scene.raymarchEnable = true;
		render.scene.raymarchMaxDistance = render.maxDistance; // do I want to keep both? not sure
		render.scene.raymarchMaxSteps = 100;
		render.scene.raymarchUnderstep = 0.9f;
		render.scene.marbleRadius = 8.0f;

		render.scene.ddaSpheresEnable = false;
		render.scene.ddaSpheresBoundSize = vec3( 1.6f );
		render.scene.ddaSpheresResolution = 513;

		// masked planes
		render.scene.maskedPlaneEnable = false;

		// explicit primitive list
		render.scene.explicitListEnable = false;
		render.scene.numExplicitPrimitives = 1;

		// sky config
		render.scene.skyDims = uvec2( 4096, 2048 );
		render.scene.skyNeedsUpdate = false;
		render.scene.skyMode = 3;
		render.scene.skyConstantColor1 = vec3( 1.0f );
		render.scene.skyConstantColor2 = vec3( 0.0f );
		render.scene.sunThresh = 164;
		render.scene.skyInvert = false;
		render.scene.skyBrightnessScalar = 2.146f;
		render.scene.skyTime = 5.0f;
		render.scene.skyClamp = 5.0f;

		// ===============================

		// load a config file and shit? ( src/projects/PathTracing/Daedalus/config.json individual config )
			// tbd, that could be a nice(r) way to handle this
	}

	// wip gizmo stuff
	vec3 panDolly;
	mat4 modelMatrix;

	// size of output
	uint32_t outputWidth;
	uint32_t outputHeight;

	// sizing for accumulators, etc
	uint32_t targetWidth;
	uint32_t targetHeight;
	uint32_t tileSize;
	tileDispenser tiles;

	// performance settings / monitoring
	uint32_t performanceHistorySamples;
	std::deque< float > timeHistory;	// ms per frame

	// filter operation config
	float filterBlendAmount;
	bool filterEveryFrame;
	int filterSelector;

	bool showConfigWindow;	// tabbed interface for configuring scene/rendering parameters
	viewConfig_t view;		// parameters for ouput presentation
	renderConfig_t render;	// parameters for the tiled renderer
	postConfig_t post;

	// class holding the random number generators
	rngen_t rng;
};

/*

void stateSerialize ( daedalusConfig_t * config, string filename ) {
	YAML::Node outputConfig;

	// supply the values for the output from the current values on the config struct passed in
		// this needs to include the full (include-processed, to bake that in) source of the shader, so that we have fully transferrable state along with the

	// outputConfig
}

void stateDeserialize ( daedalusConfig_t * config, string filename ) {
	YAML::Node inputConfig = YAML::LoadFile( filename );

	// take the values from the input file, put them onto the config struct passed in
}

*/