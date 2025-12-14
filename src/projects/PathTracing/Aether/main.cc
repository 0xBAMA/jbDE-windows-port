#include "aether.h"

class Aether final : public engineBase { // sample derived from base engine class
public:
	Aether () { Init(); OnInit(); PostInit(); }
	~Aether () { Quit(); }

	AetherConfig aetherConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// handles
			aetherConfig.textureManager = &textureManager;
			aetherConfig.shaders = &shaders;

			// something to put some basic data in the accumulator texture
			CompileShaders( aetherConfig );

			// tally textures for X, Y, Z... "count" also, tbd if that's relevant
			CreateTextures( aetherConfig );

			// importance sampling setup for the lights types
			SetupImportanceSampling_lightTypes( aetherConfig );

			// importance sampling setup for the specific list of light configurations
			SetupImportanceSampling_lights( aetherConfig ); // needs to be re-called upon any changes to the lighting config

			// what's the plan for autoexposure?

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

		// config window for the lights
		LightConfigWindow( aetherConfig );

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
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			const glm::mat3 inverseBasisMat = inverse( glm::mat3( -trident.basisX, -trident.basisY, -trident.basisZ ) );
			glUniformMatrix3fv( glGetUniformLocation( shader, "invBasis" ), 1, false, glm::value_ptr( inverseBasisMat ) );

			static rngi wangSeeder( 0, 1000000 );
			glUniform1i( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );

			static rngi blueSeeder( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), blueSeeder(), blueSeeder() );

			glUniform3i( glGetUniformLocation( shader, "dimensions" ), aetherConfig.dimensions.x, aetherConfig.dimensions.y, aetherConfig.dimensions.z );

			textureManager.BindTexForShader( "XTally", "bufferImageX", shader, 2 );
			textureManager.BindTexForShader( "YTally", "bufferImageY", shader, 3 );
			textureManager.BindTexForShader( "ZTally", "bufferImageZ", shader, 4 );
			textureManager.BindTexForShader( "Count", "bufferImageCount", shader, 5 );

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

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		if ( trident.Dirty() ) {
			ResetAccumulator( aetherConfig );
		}

		// if we've changed the light setup
		if ( aetherConfig.lightListDirty ) {
			// we need to rebuild the importance sampling structure
			SetupImportanceSampling_lights( aetherConfig );
		}

		// run the simulation...

		// any autoexposure update?

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
	Aether engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
