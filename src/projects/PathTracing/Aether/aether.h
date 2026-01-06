#pragma once

#include "../../../engine/engine.h"
#include "light.h"

// https://suricrasia.online/blog/shader-functions/
vec3 erot ( vec3 p, vec3 ax, float ro ) {
	return mix( dot( ax, p ) * ax, p, cos( ro ) ) + cross( ax,p ) * sin( ro );
}

struct AetherConfig {
	// handle for the texture manager
	textureManager_t *textureManager;
	orientTrident *trident;
	unordered_map< string, GLuint > *shaders;

	// for the tally buffers
	ivec3 dimensions{ 1000, 400, 200 };
	// ivec3 dimensions{ 2000, 1000, 500 };

	// managing the list of specific lights...
	bool lightListDirty = true;
	int numLights = 0;
	static constexpr int maxLights = 1024;
	lightSpec lights[ maxLights ];
	vec4 visualizerColors[ maxLights ];
	GLuint lightBuffer;

	float scale = 1000.0f;

	bool runSimToggle = false;
	bool runDrawToggle = false;

	json gelatinRecords;
	void LoadGelatinRecords () {
		ifstream i( "../src/data/LeeGelList.json" ); i >> gelatinRecords; i.close();
		// config.windowTitle				= j[ "system" ][ "windowTitle" ];
	}

	void ClearAccumulator () {
		textureManager->ZeroTexture2D( "Accumulator" );
	}

	void ClearLightCache () {
		textureManager->ZeroTexture3D( "XTally" );
		textureManager->ZeroTexture3D( "YTally" );
		textureManager->ZeroTexture3D( "ZTally" );
		textureManager->ZeroTexture3D( "Count" );
	}

	void ScreenShot ( string filename ) {
		const string label = "Display Texture";
		const GLuint tex = textureManager->Get( label );
		uvec2 dims = textureManager->GetDimensions( label );
		std::vector< float > imageBytesToSave;
		imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
		glBindTexture( GL_TEXTURE_2D, tex );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave[ 0 ] );
		auto imageSaveThread = std::jthread( [ dims, filename, imageBytesToSave ] () {
			Image_4F screenshot( dims.x, dims.y, &imageBytesToSave[ 0 ] );
			screenshot.RGBtoSRGB();
			screenshot.FlipVertical();
			screenshot.Save( filename, Image_4F::backend::LODEPNG );
		} );
	}

	void AddRandomLight () {
		RandomizeIndexedLight( numLights );
		numLights++;
	}

	void RandomizeIndexedLight ( int index ) {
		rngi lightpick = rngi( 0, numLUTs - 1 );
		rngi emitterpick = rngi( 0, numEmitters - 2 ); // disable image lights till I can get the importance sampling sorted out
		rng placement = rng( -1024.0f, 1024.0f );
		rng smallerpick = rng( 0.0f, 1.0f );
		lights[ index ].emitterParams[ 0 ].x = placement();
		lights[ index ].emitterParams[ 0 ].y = placement();
		lights[ index ].emitterParams[ 0 ].z = placement();
		lights[ index ].emitterParams[ 0 ].w = 0.4f;
		lights[ index ].emitterParams[ 1 ] = -lights[ index ].emitterParams[ 0 ];
		lights[ index ].emitterParams[ 1 ].w = smallerpick();
		lights[ index ].cachedEmitterParams[ 0 ] = lights[ index ].emitterParams[ 0 ];
		lights[ index ].cachedEmitterParams[ 1 ] = lights[ index ].emitterParams[ 1 ];
		lights[ index ].pickedLUT = lightpick();
		lights[ index ].emitterType = emitterpick();
		lights[ index ].rotationAxis = normalize( vec3( placement(), placement(), placement() ) );
		sprintf( lights[ index ].label, "example light label" );
	}

	// animation config
	int32_t frame = 0;
	int32_t samples = 0;
	int32_t simSteps = 0;

	const int32_t numFrames = 1200;
	const int32_t numSamples = 175;
	const int32_t numSimSteps = 250;

	// whipped-together 4-state FSM for animation control
	int animState = 0;

	// this is the state advance and transition function
	void AnimationUpdate () {
		switch ( animState ) {
			case 0: // initial state, waiting
				break;

			case 1: // we are running the sim
				runSimToggle = true;
				runDrawToggle = false;
				AdvanceSim();
				break;

			case 2: // we are rendering
				runSimToggle = false;
				runDrawToggle = true;
				AdvanceSample();
				break;

			case 4: // we are waiting to take a screenshot
				break;

			default: break;
		}
	}

	void AttemptScreenShot () {
		if ( animState == 4 ) {
			ScreenShot( "frames/" + fixedWidthNumberString( frame ) + ".png" );

			// clear the framebuffer's accumulated image
			ClearAccumulator();

			// clear the light cache's accumulated image(s)
			ClearLightCache();

			animState = 1;
		} // all other cases, fall through
	}

	// I don't want to double trigger, and don't want to get into threading here
	bool lockout = false;
	void AnimationTrigger () {
		if ( !lockout ) {
			// need to zero out the simulation state, keeping the current light config
			lockout = true;
			animState = 1;
			ClearLightCache();
		}
	}

	void CacheLights () {
		for ( int i = 0 ; i < numLights; i++ ) {
			// cache all prior values
			lights[ i ].cachedEmitterParams[ 0 ] = lights[ i ].emitterParams[ 0 ];
			lights[ i ].cachedEmitterParams[ 1 ] = lights[ i ].emitterParams[ 1 ];
			lights[ i ].cachedEmitterType =  lights[ i ].emitterType;
			lights[ i ].cachedPickedLUT = lights[ i ].pickedLUT;
			lights[ i ].cachedPower = lights[ i ].power; // tbd
		}
	}

	void AdvanceSim () {
		simSteps++;
		if ( simSteps == numSimSteps ) {
			// time to render
			animState = 2;
		}
	}

	void AdvanceSample () {
		++samples;
		if ( samples >= numSamples ) {
			// finished rendering, time to save
			AdvanceFrame();
		}
	}

	void AdvanceFrame () {
	// need to do a couple things:
		// save out this frame's prepared image

		// reset render sample count, simulation steps + increment frame counter
		samples = 0u;
		simSteps = 0u;
		++frame;

		cout << "Advancing to frame " << frame << endl;

		CacheLights();

		// compute new light position/directions
		for ( int i = 0 ; i < numLights; i++ ) {
			vec3 p = lights[ i ].emitterParams[ 0 ].xyz;
			p = erot( p, lights[ i ].rotationAxis, 0.0000618f );
			lights[ i ].emitterParams[ 0 ].x = p.x;
			lights[ i ].emitterParams[ 0 ].y = p.y;
			lights[ i ].emitterParams[ 0 ].z = p.z;
			lights[ i ].emitterParams[ 1 ].x = -p.x;
			lights[ i ].emitterParams[ 1 ].y = -p.y;
			lights[ i ].emitterParams[ 1 ].z = -p.z;
		}

		lightListDirty = true;

		// apply some kind of small rotation of the view
		trident->RotateX( 0.001618f );
		trident->RotateY( 0.000618f );

		// and we are in waiting state, image is ready to save
		animState = 4;

		// you can call it again
		if ( frame == numFrames ) {
			lockout = false;
		}
	}

};

inline void CompileShaders ( AetherConfig &config ) {
	( *config.shaders )[ "Draw" ] = computeShader( "../src/projects/PathTracing/Aether/shaders/draw.cs.glsl" ).shaderHandle;
	( *config.shaders )[ "Sim" ] = computeShader( "../src/projects/PathTracing/Aether/shaders/sim.cs.glsl" ).shaderHandle;
}

inline void CreateTextures ( AetherConfig &config ) {
	{
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
	}

	{
		// array of illumination textures
		textureOptions_t opts;

	}


	// start filesystem crap
	struct pathLeafString {
		std::string operator()( const std::filesystem::directory_entry &entry ) const {
			return entry.path().string();
		}
	};
	std::vector< string > directoryStrings;
	std::filesystem::path p( "../panels_final/" );
	std::filesystem::directory_iterator start( p );
	std::filesystem::directory_iterator end;
	std::transform( start, end, std::back_inserter( directoryStrings ), pathLeafString() );
	// std::sort( directoryStrings.begin(), directoryStrings.end() ); // sort alphabetically

	/*
	std::vector< Image_4F > textures;
	ivec2 maxDims = ivec2( 0 );
	for ( auto& s :  directoryStrings ) {
		Image_4F image( s, Image_4F::backend::STB_IMG );
		if ( image.Width() <= 640  || image.Height() <= 480 ) {
			image.Crop( 640, 480 ); // fills with zeroes
			textures.push_back( image );
		}
		maxDims = glm::max( maxDims, ivec2( textures[ textures.size() - 1 ].Width(), textures[ textures.size() - 1 ].Height() ) );
	}
	*/

	/*
	// create the texture data on the GPU
	textureOptions_t opts;
	opts.dataType		= GL_RGBA32F;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_2D_ARRAY;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	opts.width			= config.dimensions.x;
	opts.height			= config.dimensions.y;
	opts.pixelDataType	= GL_FLOAT;

	// std::vector< float >
	*/
}

inline void ResetAccumulator ( AetherConfig &config ) {
	config.ClearAccumulator();
}

inline void ResetTextures ( AetherConfig &config ) {
	config.ClearLightCache();
	ResetAccumulator( config );
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
	const float blockSizeMax = float( std::max( std::max( config.dimensions.x, config.dimensions.y ), config.dimensions.z ) );

	ImGui::Checkbox( "Run Sim", &config.runSimToggle );
	ImGui::Checkbox( "Run Draw", &config.runDrawToggle );

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
			ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		case 1: // cauchy beam emitter

			// need to set the 3D emitter location
			ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			// need to set the 3D direction - tbd how this is going to go, euler angles?
			ImGui::SliderFloat3( ( string( "Direction" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 1 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			// need to set the scale factor for the angular spread
			ImGui::SliderFloat( ( string( "Angular Spread" ) + lString ).c_str(), &config.lights[ l ].emitterParams[ 0 ][ 3 ], 0.0001f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		case 2: // laser disk

			// need to set the 3D emitter location
			ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			// need to set the 3D direction (defining disk plane)
			ImGui::SliderFloat3( ( string( "Direction" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 1 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			// need to set the radius of the disk being used
			ImGui::SliderFloat( ( string( "Radius" ) + lString ).c_str(), &config.lights[ l ].emitterParams[ 0 ][ 3 ], 0.0001f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		case 3: // uniform line emitter

			// need to set the 3D location of points A and B
			ImGui::SliderFloat3( ( string( "Position A" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			ImGui::SliderFloat3( ( string( "Position B" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 1 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		case 4: // line beam

			ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			ImGui::SliderFloat3( ( string( "Direction" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 1 ][ 0 ], -blockSizeMax, blockSizeMax, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			ImGui::SliderFloat( ( string( "Width" ) + lString ).c_str(), &config.lights[ l ].emitterParams[ 0 ][ 3 ], 0.0001f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;

		/*
		case 5: // image light

			ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 0 ][ 0 ], -400.0f, 400.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			ImGui::SliderFloat3( ( string( "Direction" ) + lString ).c_str(), ( float * ) &config.lights[ l ].emitterParams[ 1 ][ 0 ], -400.0f, 400.0f, "%.3f" );
			config.lightListDirty |= ImGui::IsItemEdited();
			ImGui::SliderFloat( ( string( "Width" ) + lString ).c_str(), &config.lights[ l ].emitterParams[ 0 ][ 3 ], 0.0001f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			config.lightListDirty |= ImGui::IsItemEdited();

			ImGui::SliderFloat( ( string( "Image" ) + lString ).c_str(), &config.lights[ l ].emitterParams[ 1 ][ 3 ], 0.0f, 100.0f );
			config.lightListDirty |= ImGui::IsItemEdited();

			break;
			*/

		default:
			ImGui::Text( "Invalid Emitter Type" );
			break;
		}
		if ( ImGui::Button( ( string( "Remove" ) + lString ).c_str() ) ) {
			config.lightListDirty = true;
			flaggedForRemoval = l;
		}
		if ( ImGui::Button( ( string( "Randomize " ) + lString ).c_str() ) ) {
			config.RandomizeIndexedLight( l );
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

	if ( config.lightListDirty ) {
		config.CacheLights();
	}
}

inline void GelatinConfigWindow ( AetherConfig &config ) {
	// lambda utilities
	auto normalizePDF = [] ( std::vector< float > &PDFVector ) {
		// first establish the maximum value across the range
		float max = 0.0f;
		for ( auto& v : PDFVector )
			max = std::max( max, v );

		// and the normalize pass, remap all values to 0..1 based on the observed max
		for ( auto& v : PDFVector )
			v = v / max;
	};

	auto loadLUT = [] ( std::vector< float > &PDFVector, const int index ) {
		const string LUTPath = "../src/data/spectraLUT/Preprocessed/";
		Image_4F pdfLUT( LUTPath + LUTFilenames[ index ] + ".png" );

		// almost the same as the other usage, but we need the
		PDFVector.clear();
		for ( int x = 0; x < pdfLUT.Width(); x++ ) {
			float sum = 0.0f;
			for ( int y = 0; y < pdfLUT.Height(); y++ ) {
				// invert because lut uses dark for positive indication... maybe fix that
				sum += 1.0f - pdfLUT.GetAtXY( x, y ).GetLuma();
			}
			PDFVector.push_back( sum );
		}
	};

	struct gel {
		string label;
		string description;
		vec3 color;
		vector< float > filter;
	};

	static vector< gel > gelList;

	auto loadGels = [&] () {
		for ( auto& e : config.gelatinRecords ) {
			gel g;

			// Replace non-breaking space with regular space
			string text = e[ "text" ];

			// Trim leading spaces
			size_t firstPos = text.find_first_not_of( " \n" );

			// Find first and second occurrence of the same number (extracted from first part)
			size_t numEnd = text.find( ' ', firstPos );
			std::string number = text.substr( firstPos, numEnd - firstPos );
			size_t secondPos = text.find( number, numEnd );

			// Output first part
			g.label = text.substr( firstPos, secondPos - firstPos );

			// Output second part
			g.description = text.substr( secondPos );

			// get the hex codes and convert to color
			string c = e[ "color" ];
			std::transform( c.begin(), c.end(), c.begin(), [] ( unsigned char cf ) { return std::tolower( cf ); } );
			g.color = HexToVec3( c );

			cout << g.label << endl;
			cout << g.description << endl;
			cout << to_string( g.color ) << endl;

			// spectral data... need to reject the listing if we do not have this
			vector< float > filter;
			filter.clear();
			if ( e.contains( "datatext" ) ) {
				for ( int lambda = 405;; lambda += 5 ) {
					if ( e[ "datatext" ].contains( to_string( lambda ) ) ) {
						filter.push_back( std::stof( e[ "datatext" ][ to_string( lambda ) ].get< string >() ) / 100.0f );
					} else {
						filter.push_back( 0.0f );
						filter.push_back( 0.0f );
						break;
					}
				}
			}

			// if we made it through that loop without adding any data, we have to skip this entry
			if ( filter.size() != 0 ) {
				bool allZeroes = true;
				for ( int i = 0; i < filter.size(); i++ ) {
					if ( filter[ i ] != 0.0f ) {
						allZeroes = false;
					}
				}
				if ( allZeroes ) {
					break;
				}

				// let's just go ahead and precompute a 1:1 representation
				for ( int w = 380; w < 400; w++ ) {
					g.filter.push_back( 0.0f );
				}

				// lerp out the transmission values
				float vprev = 0.0f;
				float v = filter[ 0 ];
				for ( int wOffset = 1; wOffset < filter.size(); wOffset++ ) {
					// each entry spawns 5 elements
					for ( int i = 0; i < 5; i++ ) {
						g.filter.push_back( glm::mix( vprev, v, ( i + 0.5f ) / 5.0f ) );
					}
					// cycle in the new values
					vprev = v;
					v = filter[ wOffset ];
				}

				// and the tail end, all zeroes
				while ( g.filter.size() < 780 ) {
					g.filter.push_back( 0.0f );
				}

				gelList.push_back( g );
			}
		}

		// sort the gel list by label
		std::sort( gelList.begin(), gelList.end(), [] ( gel g1, gel g2 ) { return g1.label < g2.label; } );

		// ================================================================================================================
		{
			// visualizing the filtered light PDF
			textureOptions_t opts;
			opts.dataType = GL_RGBA8;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.width = 450;
			opts.height = 256;
			opts.textureType = GL_TEXTURE_2D;

			config.textureManager->Add( "Filtered PDF Preview", opts );
		}
	};

	auto applyGel = [&] ( std::vector< float > &PDFVector, const int index ) {
		// over the range of PDF values... we need to multiply by %Y
		for ( int i = 0; i < PDFVector.size(); i++ ) {
			PDFVector[ i ] *= gelList[ index ].filter[ i ];
		}
	};

	// current state value
	static std::vector< float > myPDF;

	struct gelState {
		char name[ 1024 ];
		int pickedLUT = 0;
		std::vector< int > pickedGels{{ 0 }};
	};

	Image_4U preview( 450, 256 );
	auto updateGelStack = [&] ( std::vector< float > &PDFVector, gelState state ) {
		loadLUT( PDFVector, state.pickedLUT );
		normalizePDF( PDFVector );

		for ( int i = 0; i < state.pickedGels.size(); i++ ) {
			applyGel( PDFVector, state.pickedGels[ i ] );
			normalizePDF( PDFVector );
		}

		// this should then update the texture for the preview
		// this first part of the buffer, I want to visualize...

		for ( int i = 0; i < PDFVector.size(); i++ ) {
			for ( int y = 0; y < 256; y++ ) {
				float fractionalValue = ( y + 0.5f ) / 256.0f;

				Image_4U::color c( { 0, 0, 0, 255 } );
				if ( fractionalValue > PDFVector[ i ] ) {
					c = Image_4U::color( { 255, 255, 255, 255 } );
				}

				preview.SetAtXY( i, y, c );
			}
		}
		preview.FlipVertical();

		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, config.textureManager->Get( "Filtered PDF Preview" ) );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, preview.Width(), preview.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, preview.GetImageDataBasePtr() );
	};

	static const char ** gelStrings;
	static gelState myState;
	static bool firstTime = true;
	if ( firstTime ) {
		firstTime = false;

		loadGels();
		loadLUT( myPDF, 0 );
		normalizePDF( myPDF );

		// setup the list of strings for the menu
		// gelStrings = new const char * [ gelList.size() ];
		// for ( int i = 0; i < gelList.size(); i++ ) {
		// 	char temp[ 1024 ];
		//
		// 	gelStrings[ i ] = new const char [ 1024 ]( temp );
		// 	sprintf( gelStrings[ i ], "%s", gelList[ i ].label.c_str() );
		// }

		const size_t numStrings = gelList.size();
		gelStrings = ( const char ** ) malloc( numStrings * sizeof( const char * ) );

		// Loop through the list and copy each string
		for ( size_t i = 0; i < numStrings; i++ ) {
			// Allocate memory for each string and copy it
			gelStrings[ i ] = ( const char * ) malloc( strlen( gelList[ i ].label.c_str() ) + 1 );
			strcpy( ( char * ) gelStrings[ i ], gelList[ i ].label.c_str() );  // Copy the string
			cout << "adding " << gelStrings[ i ] << endl;
		}
	}

	// if we have flagged an entry for removal...
	static int flaggedForRemoval = -1;
	if ( flaggedForRemoval != -1 ) {
		myState.pickedGels.erase( myState.pickedGels.begin() + flaggedForRemoval );
		flaggedForRemoval = -1;
	}

	ImGui::Begin( "LightConstructor - Gel Filter Builder" );

	// string edit for the label
	ImGui::InputText( "filename", myState.name, sizeof( myState.name ) );

	// save button
	ImGui::SameLine();
	if ( ImGui::Button( "Save" ) ) {
		preview.Save( "../src/data/spectraLUT/Preprocessed/" + string( myState.name ) + ".png" );
	}

	// curve texture preview
	ImGui::Text( "" );
	const int w = ImGui::GetContentRegionAvail().x;
	ImGui::Image( ( ImTextureID ) ( void * ) intptr_t( config.textureManager->Get( "Filtered PDF Preview" ) ), ImVec2( w, w * ( 256.0f ) / ( 450.0f ) ) );
	ImGui::Text( "" );

	static bool needsUpdate = true;

	// source distribution picker
	ImGui::Combo( ( string( "Light Type" ) ).c_str(), &myState.pickedLUT, LUTFilenames, numLUTs );
	needsUpdate |= ImGui::IsItemEdited();

	// for gels
	for ( int i = 0; i < myState.pickedGels.size(); i++ ) {
		string iString = to_string( i );

		ImGui::Separator();

		// show gel picker
		ImGui::Combo( ( "Gel##" + iString ).c_str(), &myState.pickedGels[ i ], gelStrings, gelList.size() );
		needsUpdate |= ImGui::IsItemEdited();

		ImGui::SameLine();
		if ( ImGui::Button( ( "Randomize##" + iString  ).c_str() ) ) {
			rngi gelPick = rngi( 0, gelList.size() - 1 );
			myState.pickedGels[ i ] = gelPick();
			needsUpdate = true;
		}

		ImGui::SameLine();
		if ( ImGui::Button( ( "Remove##" + iString  ).c_str() ) ) {
			flaggedForRemoval = i;
			needsUpdate = true;
		}

		// show selected gel preview color
		vec3 col = gelList[ myState.pickedGels[ i ] ].color;
		if ( ImGui::ColorButton( ( "##ColorSquare" + iString ).c_str(), ImColor( col.r, col.g, col.b ), ImGuiColorEditFlags_NoAlpha, ImVec2(16, 16 ) ) ) {}
		ImGui::SameLine();

		// show selected gel description
		// ImGui::SameLine();
		ImGui::TextWrapped( "%s", gelList[ myState.pickedGels[ i ] ].description.c_str() );

	}

	// button to add a new gel to the stack
	if ( ImGui::Button( "Add Gel" ) ) {
		needsUpdate = true;
		myState.pickedGels.emplace_back();
	}

	if ( needsUpdate ) {
		updateGelStack( myPDF, myState );
		needsUpdate = false;
	}

	ImGui::End();
}

inline std::vector< uint32_t > lightBufferDataA;
inline std::vector< vec4 > lightBufferDataB;

inline void SetupLightBuffer ( AetherConfig &config ) {
	lightBufferDataB.clear();
	// then we also need some information for each individual light...
	for ( int i = 0; i < config.numLights; i++ ) {
		// the 3x vec4's specifying a light for the GPU process...
		lightBufferDataB.push_back( vec4( config.lights[ i ].emitterType, config.lights[ i ].pickedLUT, 0.0f, 0.0f ) );
		lightBufferDataB.push_back( config.lights[ i ].emitterParams[ 0 ] );
		lightBufferDataB.push_back( config.lights[ i ].emitterParams[ 1 ] );
		lightBufferDataB.emplace_back( 0.0f );

		// and the cached previous frame values, so we can lerp (dumb lerp for this stuff probably fine)
		lightBufferDataB.push_back( vec4( config.lights[ i ].cachedEmitterType, config.lights[ i ].cachedPickedLUT, 0.0f, 0.0f ) );
		lightBufferDataB.push_back( config.lights[ i ].cachedEmitterParams[ 0 ] );
		lightBufferDataB.push_back( config.lights[ i ].cachedEmitterParams[ 1 ] );
		lightBufferDataB.emplace_back( 0.0f );
	}

	std::vector< uint32_t > lightBufferDataConcat;
	for ( int i = 0; i < config.maxLights; i++ ) {
		lightBufferDataConcat.push_back( lightBufferDataA[ i ] );
	}
	for ( int i = 0; i < config.numLights * 8; i++ ) {
		lightBufferDataConcat.push_back( bit_cast< uint32_t >( lightBufferDataB[ i ].x ) );
		lightBufferDataConcat.push_back( bit_cast< uint32_t >( lightBufferDataB[ i ].y ) );
		lightBufferDataConcat.push_back( bit_cast< uint32_t >( lightBufferDataB[ i ].z ) );
		lightBufferDataConcat.push_back( bit_cast< uint32_t >( lightBufferDataB[ i ].w ) );
	}

	// and sending the latest data to the GPU
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, config.lightBuffer );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 4 * lightBufferDataConcat.size(), ( GLvoid * ) &lightBufferDataConcat[ 0 ], GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, config.lightBuffer );
}

inline void SetupImportanceSampling_lights ( AetherConfig &config ) {
	config.lightListDirty = false;
	static bool firstTime = true;
	if ( firstTime ) {
		// create the texture
		firstTime = false;

		glGenBuffers ( 1, &config.lightBuffer );

		// ================================================================================================================
		{ // visualizing the light list importance structure
			textureOptions_t opts;
			opts.dataType = GL_RGBA8;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.width = 32;
			opts.height = 32;
			opts.textureType = GL_TEXTURE_2D;

			config.textureManager->Add( "Light Importance Visualizer", opts );
		}

		// ================================================================================================================
		// some dummy lights...
		for ( int i = 0; i < 3; i++ ) {
			config.AddRandomLight();
		}

		// ================================================================================================================
		{ // we need some colors to visualize the light buffer...
			rng pick( 0.0f, 1.0f );
			for ( int x = 0; x < 1024; x++ ) {
				config.visualizerColors[ x ] = vec4( pick(), pick(), pick(), 1.0f );
			}
		}
	}

// using the current configuration of the lights, we need to create an importance sampling structure
	// total power helps us inform how the buffer is populated, based on the relative contribution of each light
	float totalPower = 0.0f;
	std::vector< float > powers;
	for ( int l = 0; l < config.numLights; l++ ) {
		totalPower += config.lights[ l ].power;
		powers.push_back( totalPower );
	}

	// this only fires when the light list is edited, so I'm going to do this kind of a dumb way, using a uniform distribution to pick
	lightBufferDataA.clear();
	rng pick = rng( 0.0f, totalPower );
	for ( int i = 0; i < config.maxLights; i++ ) {
		float thresh = pick();
		int j = 0;
		for ( ; j < powers.size(); j++ ) {
			if ( thresh <= powers[ j ] ) { // we threw a dart and now have run out to it and passed it. Take this result.
				break; // because we are generating numbers 0.0f to totalPower, we will land in one of these bins, with frequency weighted by their relative magnitude in "power"
			}
		}
		lightBufferDataA.push_back( j );
	}

	// this first part of the buffer, I want to visualize...
	Image_4U importanceVisualizer( 64 * 5 + 1, 16 * 5 + 1 );
	for ( uint32_t y = 0; y < 16; y++ ) {
		for ( uint32_t x = 0; x < 64; x++ ) {
			int index = y * 64 + x;
			int pickedLight = lightBufferDataA[ index ];
			color_4U color, black;
			color[ red ] = config.visualizerColors[ pickedLight ].r * 255;
			color[ green ] = config.visualizerColors[ pickedLight ].g * 255;
			color[ blue ] = config.visualizerColors[ pickedLight ].b * 255;
			black[ alpha ] = color[ alpha ] = 255;
			black[ red ] = black[ green ] = black[ blue ] = 0;

			for ( uint32_t bY = 0; bY < 4; bY++ ) {
				for ( uint32_t bX = 0; bX < 6; bX++ ) {
					if ( bY == 0 || bX == 0 || bY == 5 || bX == 5 ) {
						importanceVisualizer.SetAtXY( 5 * x + bX, 5 * y + bY, black );
					} else {
						importanceVisualizer.SetAtXY( 5 * x + bX, 5 * y + bY, color );
					}
				}
			}
		}
	}
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, config.textureManager->Get( "Light Importance Visualizer" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, importanceVisualizer.Width(), importanceVisualizer.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, importanceVisualizer.GetImageDataBasePtr() );

	SetupLightBuffer( config );
}

inline void AetherSimUpdate ( AetherConfig &config ) {
	if ( config.runSimToggle ) {
		const GLuint shader = ( *config.shaders )[ "Sim" ];
		glUseProgram( shader );

		// environment setup
		rngi wangSeeder = rngi( 0, 1000000 );
		glUniform1ui( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );

		static rngi blueSeeder( 0, 512 );
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), blueSeeder(), blueSeeder() );

		static rng frameJitter( 0.0f, 1.0f );
		glUniform1f( glGetUniformLocation( shader, "frame" ), config.frame + frameJitter() );
		glUniform3i( glGetUniformLocation( shader, "dimensions" ), config.dimensions.x, config.dimensions.y, config.dimensions.z );

		config.textureManager->BindTexForShader( "Blue Noise", "blueNoise", shader, 0 );
		config.textureManager->BindImageForShader( "XTally", "bufferImageX", shader, 2 );
		config.textureManager->BindImageForShader( "YTally", "bufferImageY", shader, 3 );
		config.textureManager->BindImageForShader( "ZTally", "bufferImageZ", shader, 4 );
		config.textureManager->BindImageForShader( "Count", "bufferImageCount", shader, 5 );
		config.textureManager->BindTexForShader( "iCDF", "iCDFtex", shader, 6 );

		glDispatchCompute( 1, 16, 16 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
}