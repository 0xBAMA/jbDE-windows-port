#pragma once

#include "../../../engine/engine.h"
#include "light.h"

struct AetherConfig {
	// handle for the texture manager
	textureManager_t *textureManager;
	unordered_map< string, GLuint > *shaders;

	// for the tally buffers
	ivec3 dimensions{ 1280, 720, 128 };

	// managing the list of specific lights...
	bool lightListDirty = true;
	int numLights = 2;
	static constexpr int maxLights = 1024;
	lightSpec lights[ maxLights ];
	vec4 visualizerColors[ maxLights ];
	GLuint lightBuffer;

};

inline void CompileShaders ( AetherConfig &config ) {
	( *config.shaders )[ "Draw" ] = computeShader( "../src/projects/PathTracing/Aether/shaders/draw.cs.glsl" ).shaderHandle;
}

inline void CreateTextures ( AetherConfig &config ) {
	// configuring the tally textures
	textureOptions_t opts;
	opts.dataType		= GL_R32I;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_3D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	opts.width			= config.dimensions.x;
	opts.height			= config.dimensions.y;
	opts.depth			= config.dimensions.z;

	// considering maybe switching to RGB... existing tonemapping, etc would be more effective
	config.textureManager->Add( "XTally", opts );
	config.textureManager->Add( "YTally", opts );
	config.textureManager->Add( "ZTally", opts );
	config.textureManager->Add( "Count", opts );

	// displacement etc?
}

inline void ResetTextures ( AetherConfig &config ) {
	config.textureManager->ZeroTexture3D( "XTally" );
	config.textureManager->ZeroTexture3D( "YTally" );
	config.textureManager->ZeroTexture3D( "ZTally" );
	config.textureManager->ZeroTexture3D( "Count" );
	config.textureManager->ZeroTexture2D( "Accumulator" );
}

inline void SetupImportanceSampling_lightTypes ( AetherConfig &config ) {
	// setup the texture with rows for the specific light types, for the importance sampled emission spectra
	const string LUTPath = "../src/data/spectraLUT/Preprocessed/";
	Image_1F inverseCDF( 1024, numLUTs );

	for ( int i = 0; i < numLUTs; i++ ) {
		Image_4F pdfLUT( LUTPath + LUTFilenames[ i ] + ".png" );

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
	opts.height = numLUTs;
	opts.dataType = GL_R32F;
	opts.minFilter = GL_LINEAR;
	opts.magFilter = GL_LINEAR;
	opts.textureType = GL_TEXTURE_2D;
	opts.pixelDataType = GL_FLOAT;
	opts.initialData = inverseCDF.GetImageDataBasePtr();
	config.textureManager->Add( "iCDF", opts );

	/*
	// for inspection, remap to visible range
	Image_1F::rangeRemapInputs_t remap[ 1 ];
	remap[ 0 ].rangeStartLow = 380.0f;
	remap[ 0 ].rangeStartHigh = 830.0f;
	remap[ 0 ].rangeEndLow = 0.0f;
	remap[ 0 ].rangeEndHigh = 1.0f;
	remap[ 0 ].rangeType = Image_1F::HARDCLIP;
	inverseCDF.RangeRemap( remap );
	inverseCDF.Save( "testCDF.png" );
	*/
}

inline void LightConfigWindow ( AetherConfig &config ) {
	ImGui::Begin( "Light Setup" );

	// this is starting from the lighting config in Newton
	static int flaggedForRemoval = -1; // this will run next frame when I want to remove an entry from the list, to avoid imgui confusion
	if ( flaggedForRemoval != -1 && flaggedForRemoval < config.numLights ) {
		// remove it from the list by bumping the remainder of the list up
		for ( int i = flaggedForRemoval; i < config.numLights; i++ ) {
			config.lights[ i ] = config.lights[ i + 1 ];
		}

		// reset trigger and decrement light count
		flaggedForRemoval = -1;
		config.numLights--;
		if ( config.numLights < config.maxLights )
			config.lights[ config.numLights ] = lightSpec(); // zero out the entry

		config.lightListDirty = true;
	}

	// visualizer of the light buffer importance structure...
	ImGui::Text( "" );
	const int w = ImGui::GetContentRegionAvail().x;
	ImGui::Image( ( ImTextureID ) ( void * ) intptr_t( config.textureManager->Get( "Light Importance Visualizer" ) ), ImVec2( w, w * ( 16.0f * 5.0f + 1.0f ) / ( 64.0f * 5.0f + 1.0f ) ) );
	ImGui::Text( "" );

	// iterate through the list of lights, and allow manipulation of state on each one
	for ( int l = 0; l < config.numLights; l++ ) {
		const string lString = string( "##" ) + to_string( l );

		ImGui::Text( "" );
		// ImGui::Text( ( string( "Light " ) + to_string( l ) ).c_str() );
		ImGui::Indent();

		// color used for visualization
		ImGui::ColorEdit4( ( string( "MyColor" ) + lString ).c_str(), ( float* ) &config.visualizerColors[ l ], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel );
		config.lightListDirty |= ImGui::IsItemEdited();

		ImGui::SameLine();
		ImGui::InputTextWithHint( ( string( "Light Name" ) + lString ).c_str(), "Enter a name for this light, if you would like. Not used for anything other than organization.", config.lights[ l ].label, 256 );
		ImGui::SliderFloat( ( string( "Power" ) + lString ).c_str(), &config.lights[ l ].power, 0.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		// this will latch, so we basically get a big chained or that triggers if any of the conditionals like this one got triggered
		config.lightListDirty |= ImGui::IsItemEdited();
		ImGui::Combo( ( string( "Light Type" ) + lString ).c_str(), &config.lights[ l ].pickedLUT, LUTFilenames, numLUTs ); // may eventually do some kind of scaled gaussians for user-configurable RGB triplets...
		config.lightListDirty |= ImGui::IsItemEdited();
		ImGui::Combo( ( string( "Emitter Type" ) + lString ).c_str(), &config.lights[ l ].emitterType, emitterTypes, numEmitters );
		config.lightListDirty |= ImGui::IsItemEdited();
		ImGui::Text( "Emitter Settings:" );
		switch ( config.lights[ l ].emitterType ) {
		case 0: // point emitter

			// need to set the 3D point location
			ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -10.0f, 10.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		case 1: // cauchy beam emitter

			// need to set the 3D emitter location
			ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -10.0f, 10.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			// need to set the 3D direction - tbd how this is going to go, euler angles?
			ImGui::SliderFloat3( ( string( "Direction" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 1 ][ 0 ], -10.0f, 10.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			// need to set the scale factor for the angular spread
			ImGui::SliderFloat( ( string( "Angular Spread" ) + lString ).c_str(), &config.lights[ l ].emitterParams[ 0 ][ 3 ], 0.0001f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		case 2: // laser disk

			// need to set the 3D emitter location
			ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -10.0f, 10.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			// need to set the 3D direction (defining disk plane)
			ImGui::SliderFloat3( ( string( "Direction" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 1 ][ 0 ], -10.0f, 10.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			// need to set the radius of the disk being used
			ImGui::SliderFloat( ( string( "Radius" ) + lString ).c_str(), &config.lights[ l ].emitterParams[ 0 ][ 3 ], 0.0001f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		case 3: // uniform line emitter

			// need to set the 3D location of points A and B
			ImGui::SliderFloat3( ( string( "Position A" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -10.0f, 10.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			ImGui::SliderFloat3( ( string( "Position B" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 1 ][ 0 ], -10.0f, 10.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		default:
			ImGui::Text( "Invalid Emitter Type" );
			break;
		}
		if ( ImGui::Button( ( string( "Remove" ) + lString ).c_str() ) ) {
			config.lightListDirty = true;
			flaggedForRemoval = l;
		}
		ImGui::Text( "" );
		ImGui::Unindent();
		ImGui::Separator();
	}

	// option to add a new light
	ImGui::Text( "" );
	if ( ImGui::Button( " + Add Light " ) ) {
		// add a new light with default settings
		config.lightListDirty = true;
		config.numLights++;
	}

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