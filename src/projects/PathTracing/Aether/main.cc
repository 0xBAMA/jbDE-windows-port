#include "../../../engine/engine.h"

struct AetherConfig_t {
	ivec2 dims = ivec2( 1280, 768 );

	uint32_t autoExposureBufferDim = 0;
	uint32_t autoExposureMipLevels = 0;
	float autoExposureBase = 10000000.0f;

	// raytrace state
	uint64_t pathsRun = 0;
	uint64_t maxPaths = 1000000;

	// interface stuff
	vec2 mappedMousePos = vec2( 0.0f );
	int pickedLight = 9;
};

class Aether final : public engineBase { // sample derived from base engine class
public:
	Aether () { Init(); OnInit(); PostInit(); }
	~Aether () { Quit(); }

	AetherConfig_t AetherConfig;
	bool screenshotRequested = false;
	int screenshotIndex = -1;

	std::vector< string > LUTFilenames = { "AmberLED", "2700kLED", "6500kLED", "Candle", "Flourescent1", "Flourescent2", "Flourescent3", "Halogen", "HPMercury",
		"HPSodium1", "HPSodium2", "LPSodium", "Incandescent", "MetalHalide1", "MetalHalide2", "SkyBlueLED", "SulphurPlasma", "Sunlight", "Xenon" };

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// image prep
			shaders[ "Draw" ] = computeShader( "../src/projects/PathTracing/Aether/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Simulate" ] = computeShader( "../src/projects/PathTracing/Aether/shaders/simulate.cs.glsl" ).shaderHandle;
			shaders[ "Autoexposure Prep" ] = computeShader( "../src/projects/PathTracing/Aether/shaders/autoexposurePrep.cs.glsl" ).shaderHandle;
			shaders[ "Autoexposure" ] = computeShader( "../src/projects/PathTracing/Aether/shaders/autoexposure.cs.glsl" ).shaderHandle;

			// buffer image
			textureOptions_t opts;
			opts.dataType		= GL_R32I;
			opts.width			= AetherConfig.dims.x;
			opts.height			= AetherConfig.dims.y;
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			// parallel averaging via atomic adds in the line drawing function
			textureManager.Add( "Field X Tally", opts );
			textureManager.Add( "Field Y Tally", opts );
			textureManager.Add( "Field Z Tally", opts );
			textureManager.Add( "Field Count", opts );

		// additional buffer used for autoexposure
			// round up the dimensions
			AetherConfig.autoExposureBufferDim = nextPowerOfTwo( std::max( AetherConfig.dims.x, AetherConfig.dims.y ) );

			opts.width			= AetherConfig.autoExposureBufferDim;
			opts.height			= AetherConfig.autoExposureBufferDim;
			opts.dataType		= GL_R32F;
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			textureManager.Add( "Field Max", opts );

			// create the mip levels explicitly... we want to be able to sample the texel (0,0) of the highest mip of the texture for the autoexposure term
			int level = 0;
			int d = AetherConfig.autoExposureBufferDim;
			Image_4F zeroesF( d, d );
			while ( d >= 1 ) {
				d /= 2; level++;
				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Field Max" ) );
				glTexImage2D( GL_TEXTURE_2D, level, GL_R32F, d, d, 0, getFormat( GL_R32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );
			}
			AetherConfig.autoExposureMipLevels = level;

			// setup the importance sampled emission spectra stuff
			string LUTPath = "../src/data/spectraLUT/Preprocessed/";
			Image_1F inverseCDF( 1024, LUTFilenames.size() );

			for ( int i = 0; i < LUTFilenames.size(); i++ ) {
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
			opts.width = 1024;
			opts.height = LUTFilenames.size();
			opts.dataType = GL_R32F;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.textureType = GL_TEXTURE_2D;
			opts.pixelDataType = GL_FLOAT;
			opts.initialData = inverseCDF.GetImageDataBasePtr();
			textureManager.Add( "iCDF", opts );
		}
	}

	void Reset () {
		ZoneScoped;
		AetherConfig.pathsRun = 0;
		textureManager.ZeroTexture2D( "Field X Tally" );
		textureManager.ZeroTexture2D( "Field Y Tally" );
		textureManager.ZeroTexture2D( "Field Z Tally" );
		textureManager.ZeroTexture2D( "Field Count" );
		glMemoryBarrier( GL_ALL_BARRIER_BITS );
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		if ( inputHandler.getState( KEY_T ) ) {
			screenshotRequested = true;
		}

		if ( inputHandler.getState( KEY_R ) ) {
			Reset();
		}
	}

	void ImguiPass () {
		ZoneScoped;
		if ( tonemap.showTonemapWindow ) {
			TonemapControlsWindow();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		ImGui::Begin( "Debug Window" );
		if ( ImGui::Button(  "Screenshot" ) ) {
			screenshotRequested = true;
		}
		ImGui::Text( "Rays Comlete: %d / %d", AetherConfig.pathsRun, AetherConfig.maxPaths );
		ImGui::Text( "Mouse Position: %d %d", inputHandler.getMousePos().x, inputHandler.getMousePos().y );
		{
			// mouse remapping to match the shader
			AetherConfig.mappedMousePos = vec2( inputHandler.getMousePos().x, config.height - inputHandler.getMousePos().y ) + vec2( 0.5f );
			AetherConfig.mappedMousePos /= vec2( float( config.width ), float( config.height ) );
			AetherConfig.mappedMousePos -= vec2( 0.025f, 0.15f );
			AetherConfig.mappedMousePos *= 1.5f;
			AetherConfig.mappedMousePos.x = RangeRemap( std::clamp( AetherConfig.mappedMousePos.x, 0.0f, 1.0f ), 0.0f, 1.0f, -AetherConfig.dims.x, AetherConfig.dims.x );
			AetherConfig.mappedMousePos.y = RangeRemap( AetherConfig.mappedMousePos.y, 1.0f, 0.0f, -AetherConfig.dims.y, AetherConfig.dims.y );
		}
		ImGui::Text( "Remapped Mouse Position: %.2f %.2f", AetherConfig.mappedMousePos.x, AetherConfig.mappedMousePos.y );

		static bool firstTime = true;
		static char lightNames[ 4096 ] = { 0 };
		if ( firstTime ) {
			firstTime = false;
			string writeString;
			int i = 0;
			for ( const auto & LUTFilename : LUTFilenames ) {
				for ( const auto & c : LUTFilename ) {
					lightNames[ i ] = c; i++;
				}
				lightNames[ i ] = 0; i++;
			}
		}
		ImGui::Combo( "##pickedLight", &AetherConfig.pickedLight, lightNames );
		ImGui::End();

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void ComputePasses () {
		ZoneScoped;

		static int frame = 0;
		if ( false ) {
			scopedTimer Start( "Autoexposure" );
			{	// clear the texture with the max value
				textureManager.ZeroTexture2D( "Field Max" );
			}

			{	// populate mip 0 with "proposed pixel brightness values" with no autoexposure setting
					// potential future optimization here, do textureGathers during this step and mip 0 can be half res
				const GLuint shader = shaders[ "Autoexposure Prep" ];
				glUseProgram( shader );
				textureManager.BindImageForShader( "Field Max", "fieldMax", shader, 0 );
				textureManager.BindTexForShader( "Field Y Tally", "bufferImageY", shader, 2 );
				textureManager.BindTexForShader( "Field Count", "bufferImageCount", shader, 4 );
				glUniform1f( glGetUniformLocation( shader, "autoExposureBase" ), AetherConfig.autoExposureBase );
				glDispatchCompute( ( AetherConfig.dims.x + 15 ) / 16, ( AetherConfig.dims.y + 15 ) / 16, 1 );
				glMemoryBarrier( GL_ALL_BARRIER_BITS );
			}

			{	// mip propagation of brightness max
				int d = AetherConfig.autoExposureBufferDim / 2;
				const GLuint shader = shaders[ "Autoexposure" ];
				glUseProgram( shader );
				for ( int n = 0; n < AetherConfig.autoExposureMipLevels - 1; n++ ) { // for num mips minus 1

					// bind the appropriate levels for N and N+1 (starting with N=0... to N=...? ) double bind of texture version... yeah
					textureManager.BindTexForShader( "Field Max", "layerN", shader, 0 );
					textureManager.BindImageForShader( "Field Max", "layerN", shader, 0, n );
					textureManager.BindImageForShader( "Field Max", "layerNPlus1", shader, 1, n + 1 );

					// dispatch the compute shader( 1x1x1 groupsize for simplicity )
					glDispatchCompute( d, d, 1 );
					glMemoryBarrier( GL_ALL_BARRIER_BITS );
					d /= 2;
				}
				// postcondition is that the top mip's single texel contains the field max, and we can access that during drawing to get it into a reasonable range
			}
		}

		{ // prep accumumator texture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			textureManager.BindTexForShader( "Field X Tally", "bufferImageX", shaders[ "Draw" ], 2 );
			textureManager.BindTexForShader( "Field Y Tally", "bufferImageY", shaders[ "Draw" ], 3 );
			textureManager.BindTexForShader( "Field Z Tally", "bufferImageZ", shaders[ "Draw" ], 4 );
			textureManager.BindTexForShader( "Field Count", "bufferImageCount", shaders[ "Draw" ], 5 );
			textureManager.BindImageForShader( "Field Max", "fieldMax", shaders[ "Draw" ], 6 );
			glUniform1i( glGetUniformLocation( shaders[ "Draw" ], "autoExposureTexOffset" ), AetherConfig.autoExposureMipLevels );
			glUniform1f( glGetUniformLocation( shaders[ "Draw" ], "autoExposureBase" ), AetherConfig.autoExposureBase );
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

		if ( screenshotRequested == true ) {
			screenshotRequested = false;
			cout << "Attempting Screenshot" << endl;
			const GLuint tex = textureManager.Get( "Display Texture" );
			uvec2 dims = textureManager.GetDimensions( "Display Texture" );
			std::vector< float > imageBytesToSave;
			imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
			glBindTexture( GL_TEXTURE_2D, tex );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
			Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
			screenshot.RGBtoSRGB();
			screenshot.FlipVertical();
			const string filename = ( ( screenshotIndex != -1 ) ? ( string( "frames/" ) + fixedWidthNumberString( screenshotIndex, 4 ) ) :
				string( "Aether-" ) + timeDateString() ) + string( ".png" );
			screenshot.Save( filename, Image_4F::backend::LODEPNG );
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );

			progressBar p;
			p.done = float( AetherConfig.pathsRun / 10000.0f );
			p.total = float( AetherConfig.maxPaths / 10000.0f );
			textRenderer.DrawProgressBarString( 36, p );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		if ( AetherConfig.pathsRun < AetherConfig.maxPaths ) {
			ZoneScoped; scopedTimer Start( "Update" );

			// run the shader to run some rays
			static rngi wangSeeder = rngi( 0, 1000000 );
			const GLuint shader = shaders[ "Simulate" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "t" ), 0.0f );
			glUniform1i( glGetUniformLocation( shader, "rngSeed" ), wangSeeder() );

			rng offset = rng( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), offset(), offset() );

			glUniform2f( glGetUniformLocation( shader, "mousePos" ),
				std::clamp( AetherConfig.mappedMousePos.x, -float( AetherConfig.dims.x ), float( AetherConfig.dims.x ) ),
				std::clamp( AetherConfig.mappedMousePos.y, -float( AetherConfig.dims.y ), float( AetherConfig.dims.y ) ) );

			glUniform1i( glGetUniformLocation( shader, "pickedLight" ), AetherConfig.pickedLight );

			textureManager.BindTexForShader( "Blue Noise", "blueNoise", shader, 0 );
			textureManager.BindImageForShader( "Field X Tally", "bufferImageX", shader, 2 );
			textureManager.BindImageForShader( "Field Y Tally", "bufferImageY", shader, 3 );
			textureManager.BindImageForShader( "Field Z Tally", "bufferImageZ", shader, 4 );
			textureManager.BindImageForShader( "Field Count", "bufferImageCount", shader, 5 );
			textureManager.BindTexForShader( "iCDF", "iCDFtex", shader, 6 );

			// glDispatchCompute( ( AetherConfig.dims.x + 15 ) / 16, ( AetherConfig.dims.y + 15 ) / 16, 1 );
			glDispatchCompute( 3, 3, 1 );
			AetherConfig.pathsRun += 3 * 3 * 1 * 16 * 16 * 1;
			glMemoryBarrier( GL_ALL_BARRIER_BITS );
		}
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
		inputHandler.update();
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
	Aether engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
