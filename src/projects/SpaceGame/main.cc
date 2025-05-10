#include "../../engine/engine.h"
#include "sim.h"

class SpaceGame final : public engineBase { // sample derived from base engine class
public:
	SpaceGame () { config.forceResolution = ivec2( 640, 360 ); Init(); OnInit(); PostInit(); }
	~SpaceGame () { Quit(); }

	spaceGameData_t spaceGameData;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Dummy Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/draw.cs.glsl" ).shaderHandle;

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// new data into the input handler
		inputHandler.update();

	}

	void ImguiPass () {
		ZoneScoped;

		// =============================================================
		ImGui::Begin( "Window" );
		// =============================================================
		ImGui::Text( "Workers have resources:" );
		ImGui::Indent();
		for ( int i = 0; i < NUM_WORKERS; i++ ) {
			std::stringstream ss;
			if ( i < 10 ) ss << " ";
			ss << "| ";
			for ( int j = 0; j < NUM_RESOURCES; j++ ) {
				// listen, I don't even want to hear about it
				ss << std::setprecision( 2 ) << std::setw( 8 ) << std::setfill( '.' ) << std::fixed << spaceGameData.workers[ i ].ledger[ j ] << " | ";
			}
			ImGui::Text( "Worker %d has %s", i, ss.str().c_str() );
		}
		ImGui::Unindent();
		// =============================================================
		ImGui::Text( "Markets have resources:" );
		ImGui::Indent();
		for ( int i = 0; i < NUM_MARKETS; i++ ) {
			std::stringstream ss;
			if ( i < 10 ) ss << " ";
			ss << "| ";
			for ( int j = 0; j < NUM_RESOURCES; j++ ) {
				// listen, I don't even want to hear about it
				ss << std::setprecision( 2 ) << std::setw( 8 ) << std::setfill( '.' ) << std::fixed << spaceGameData.markets[ i ].ledger[ j ] << " | ";
			}
			ImGui::Text( "Market %d has %s", i, ss.str().c_str() );
		}
		ImGui::Unindent();
		// =============================================================
		ImGui::Text( "Sources have resources:" );
		ImGui::Indent();
		for ( int i = 0; i < NUM_SOURCES; i++ ) {
			std::stringstream ss;
			if ( i < 10 ) ss << " ";
			ss << "| ";
			for ( int j = 0; j < NUM_RESOURCES; j++ ) {
				// listen, I don't even want to hear about it
				ss << std::setprecision( 2 ) << std::setw( 8 ) << std::setfill( '.' ) << std::fixed << spaceGameData.sources[ i ].dropAmounts[ j ] << " | ";
			}
			ImGui::Text( "Source %d (%.2f remaining) gives %s", i, spaceGameData.sources[ i ].amountLeft, ss.str().c_str() );
		}
		ImGui::Unindent();
		// =============================================================
		spaceGameData.update();

		// if ( ImGui::Button( "Reset" ) ) {
			// but also you can call functions etc
			// spaceGameData
		// }

		ImGui::End();
		// =============================================================

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		// QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		// draw some shit - need to add a hello triangle to this, so I have an easier starting point for raster stuff
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

		// shader to apply dithering
			// ...

		// other postprocessing
			// ...

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
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
		DrawAPIGeometry();			// draw any API geometry desired
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
	SpaceGame engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
