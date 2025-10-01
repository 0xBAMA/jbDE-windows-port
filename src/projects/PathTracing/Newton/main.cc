#include "../../../engine/engine.h"
#include "shaders/lib/shaderWrapper.h"

class Newton final : public engineBase { // sample derived from base engine class
public:
	Newton () { Init(); OnInit(); PostInit(); }
	~Newton () { Quit(); }

	std::vector< string > LUTFilenames = { "AmberLED", "2700kLED", "6500kLED", "Candle", "Flourescent1", "Flourescent2", "Flourescent3", "Halogen", "HPMercury",
		"HPSodium1", "HPSodium2", "LPSodium", "Incandescent", "MetalHalide1", "MetalHalide2", "SkyBlueLED", "SulphurPlasma", "Sunlight", "Xenon" };

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			const string basePath = "../src/projects/PathTracing/Newton/shaders/";
			shaders[ "Draw" ]	= computeShader( basePath + "draw.cs.glsl" ).shaderHandle;
			shaders[ "Trace" ]	= computeShader( basePath + "trace.cs.glsl" ).shaderHandle;

			// emission spectra LUT textures, packed together
			textureOptions_t opts;
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

			// buffer for ray states (N rays * M bounces * 64 byte struct)

			// film plane buffer

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

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

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
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
			textRenderer.Clear();
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			// show terminal, if active - check happens inside
			textRenderer.drawTerminal( terminal );

			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		// run some rays

		// run the autoexposure stuff

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

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal (active check happens inside)
		terminal.update( inputHandler );

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

// #pragma comment( linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup" )

int main ( int argc, char *argv[] ) {
	Newton engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
