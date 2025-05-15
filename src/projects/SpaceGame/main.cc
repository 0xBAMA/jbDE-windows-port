#include "../../engine/engine.h"

#include "game.h"

class SpaceGame final : public engineBase {
public:
	SpaceGame () { config.forceResolution = ivec2( 720, 480 ); Init(); OnInit(); PostInit(); }
	~SpaceGame () { Quit(); }

	universeController controller;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			shaders[ "Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/draw.cs.glsl" ).shaderHandle;

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

	}

	void ImguiPass () {
		ZoneScoped;

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			static rngi noiseOffset( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

			vec2 v = controller.ship.GetVelocityVector();
			glUniform2f( glGetUniformLocation( shader, "velocityVector" ), v.x, v.y );

			vec2 p = controller.ship.GetPositionVector();
			glUniform2f( glGetUniformLocation( shader, "positionVector" ), p.x, p.y );

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

		{
			scopedTimer Start( "Text Rendering" );

			DrawMenuBasics( textRenderer, textureManager );

			// show terminal, if active - check happens inside
			textRenderer.Clear();
			// textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.drawTerminal( terminal );
			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
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
		controller.ship.Update( inputHandler );
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

int main ( int argc, char *argv[] ) {
	SpaceGame engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
