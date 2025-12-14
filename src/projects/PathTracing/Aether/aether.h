#include "../../../engine/engine.h"

struct AetherConfig {
	// handle for the texture manager
	textureManager_t *textureManager;
	unordered_map< string, GLuint > *shaders;

	// for the tally buffers
	ivec3 dimensions{ 1280, 720, 128 };

	// the list of different light types
	const std::vector< string > LUTFilenames = { "AmberLED", "2700kLED", "6500kLED", "Candle", "Flourescent1", "Flourescent2", "Flourescent3", "Halogen", "HPMercury",
		"HPSodium1", "HPSodium2", "LPSodium", "Incandescent", "MetalHalide1", "MetalHalide2", "SkyBlueLED", "SulphurPlasma", "Sunlight", "Xenon" };

	// the list of specific lights...
	bool lightListDirty = true;

};

inline void CompileShaders ( AetherConfig &config ) {
	( *config.shaders )[ "Draw" ] = computeShader( "../src/projects/PathTracing/Aether/shaders/draw.cs.glsl" ).shaderHandle;
}

inline void CreateTextures ( AetherConfig &config ) {

}

inline void SetupImportanceSampling_lightTypes ( AetherConfig &config ) {
	// setup the texture with rows for the specific light types, for the importance sampled emission spectra
	const string LUTPath = "../src/data/spectraLUT/Preprocessed/";
	Image_1F inverseCDF( 1024, config.LUTFilenames.size() );

	for ( int i = 0; i < config.LUTFilenames.size(); i++ ) {
		Image_4F pdfLUT( LUTPath + config.LUTFilenames[ i ] + ".png" );

		// First step is populating the cumulative distribution function... "how much of the curve have we passed" (accumulated integral)
		std::vector< float > cdf;
		float cumSum = 0.0f;
		for ( int x = 0; x < pdfLUT.Width(); x++ ) {
			float sum = 0.0f;
			for ( int y = 0; y < pdfLUT.Height(); y++ ) {
				// invert because lut uses dark for positive indication... maybe fix that
				sum += 1.0f - pdfLUT.GetAtXY( x, y ).GetLuma();
			}
			// increment cumulative sum and CDF
			cumSum += sum;
			cdf.push_back( cumSum );
		}

		// normalize the CDF values by the final value during CDF sweep
		std::vector< vec2 > CDFpoints;
		for ( int x = 0; x < pdfLUT.Width(); x++ ) {
			// compute the inverse CDF with the aid of a series of 2d points along the curve
			// adjust baseline for our desired range -> 380nm to 830nm, we have 450nm of data
			CDFpoints.emplace_back( x + 380, cdf[ x ] / cumSum );
		}

		for ( int x = 0; x < 1024; x++ ) {
			// each pixel along this strip needs a value of the inverse CDF
			// this is the intersection with the line defined by the set of segments in the array CDFpoints
			float normalizedPosition = ( x + 0.5f ) / 1024.0f;
			for ( int p = 0; p < CDFpoints.size(); p++ )
				if ( p == ( CDFpoints.size() - 1 ) ) {
					inverseCDF.SetAtXY( x, i, color_1F( { CDFpoints[ p ].x } ) );
				} else if ( CDFpoints[ p ].y >= normalizedPosition ) {
					inverseCDF.SetAtXY( x, i, color_1F( { RangeRemap( normalizedPosition,
							CDFpoints[ p - 1 ].y, CDFpoints[ p ].y, CDFpoints[ p - 1 ].x, CDFpoints[ p ].x ) } ) );
					break;
				}
		}
	}

	// we now have the solution for the LUT
	textureOptions_t opts;
	opts.width = 1024;
	opts.height = config.LUTFilenames.size();
	opts.dataType = GL_R32F;
	opts.minFilter = GL_LINEAR;
	opts.magFilter = GL_LINEAR;
	opts.textureType = GL_TEXTURE_2D;
	opts.pixelDataType = GL_FLOAT;
	opts.initialData = inverseCDF.GetImageDataBasePtr();
	config.textureManager->Add( "iCDF", opts );

	Image_1F::rangeRemapInputs_t remap;
	remap.rangeStartLow = 380.0f;
	remap.rangeStartHigh = 830.0f;
	remap.rangeEndLow = 0.0f;
	remap.rangeEndHigh = 1.0f;
	remap.
	inverseCDF.RangeRemap( {  } );
	inverseCDF.Save( "testCDF.png" );
}

inline void LightConfigWindow ( AetherConfig &config ) {
	ImGui::Begin( "Light Setup" );

	// this is starting from the lighting config in Newton

	ImGui::End();
}

inline void SetupImportanceSampling_lights ( AetherConfig &config ) {
	static bool firstTime = true;
	if ( firstTime ) {
		// create the texture
		firstTime = false;
	}

	// using the current configuration of the lights...

}