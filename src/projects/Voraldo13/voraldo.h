#include "../../engine/engine.h"

#include "dataStructs.h"
#include "menuEntry.h"

class Voraldo13 final : public engineBase {
//==============================================================================
public:
	Voraldo13 () { Init(); OnInit(); PostInit(); }
	~Voraldo13 () { Quit(); }
	
//==============================================================================

	menuContainer menu;	// contains the menu entries ( labels + interface layout blocks )
	renderState render;	// render settings

//==============================================================================

	void SwapBlocks();
	void setColorMipmapFlag();
	void setLightMipmapFlag();
	bool mipmapFlagColor = true;
	bool mipmapFlagLight = true;
	void genColorMipmap();
	void genLightMipmap();

//==============================================================================
// these were previously done via #defines... no reason to hardcode them
	// they should be dynamic at runtime

	float SSFactor = 1.0f;		// supersampling factor, for scaling the accumulator
	uint32_t blockSize = 256;	// size of the block

//==============================================================================

	void CompileShaders();
	void CreateTextures();

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// compile the shaders
			CompileShaders();

			// create the textures
			CreateTextures();
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

		{ // draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

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
			// scopedTimer Start( "Trident" );
			// trident.Update( textureManager.Get( "Display Texture" ) );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
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
