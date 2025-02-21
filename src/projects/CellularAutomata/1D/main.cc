#include "../../../engine/engine.h"

class engineDemo final : public engineBase { // sample derived from base engine class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "../src/projects/CellularAutomata/1D/shaders/draw.cs.glsl" ).shaderHandle;

			Image_4U test( 10000, 10000 );

			// colors representing the two states
			color_4U white( { 255, 255, 255, 255 } );
			color_4U black( {   0,   0,   0, 255 } );

			// seeding the first row
			rng pick = rng( 0.0f, 1.0f );
			for ( uint32_t x = 0; x < test.Width(); x++ ) {
				test.SetAtXY( x, 0, ( pick() < 0.5f ) ? white : black );
				// test.SetAtXY( x, 0, ( x == 400 ) ? white : black );
			}

			// // generate the rule (8 bits)
			// uint8_t rule;
			// for ( int i = 0; i < 8; i++ ) {
			// 	rule += ( pick() < 0.5f ) ? ( 1 << i ) : 0;
			// }

			rngi ruleGen = rngi( 0, 255 );
			rng flipChance = rng( 0.0f, 1.0f );
			rng thresh = rng( 0.0f, 0.00001f );
			// for ( uint8_t rule = 0; rule < 255; rule++ ) {
			uint8_t rule = 99;
			// uint8_t rule = 183;
				string ruleString;
				for ( int i = 0; i < 8; i++ ) {
					ruleString += ( rule & ( 1 << i ) ) ? "1" : "0";
				}
				std::reverse( ruleString.begin(), ruleString.end() );

				// evaluate the rule, down the height of the image
				for ( uint32_t y = 1; y < test.Height(); y++ ) {
					for ( uint32_t x = 0; x < test.Width(); x++ ) { // per row
						if ( flipChance() < thresh() )
							rule = ruleGen(); // new rule

						bool samples[ 3 ] = {
							( test.GetAtXY( x - 1, y - 1 ) == white ),
							( test.GetAtXY( x,     y - 1 ) == white ),
							( test.GetAtXY( x + 1, y - 1 ) == white )
						};
						uint8_t sum = (
							( samples[ 0 ] ? 4 : 0 ) +
							( samples[ 1 ] ? 2 : 0 ) +
							( samples[ 2 ] ? 1 : 0 ) );
						uint8_t ruleTestMask = ( 1 << sum );
						// test.SetAtXY( x, y, ( ( rule & ruleTestMask ) != 0 ) ? white : black );

						test.SetAtXY( x, y, ( ( flipChance() < 0.001f ) ? ( ( rule & ruleTestMask ) != 0 ) : ( ( rule & ruleTestMask ) == 0 ) ) ? white : black );

						// cout << "seeing " << ( samples[ 0 ] ? "1" : "0" ) << ( samples[ 1 ] ? "1" : "0" ) << ( samples[ 2 ] ? "1" : "0" ) << " (" << to_string( sum ) << "), rule " << to_string( rule ) << " is " << ruleString << " resulting in " << ( ( ( rule & ruleTestMask ) != 0 ) ? "living" : "dead" ) << endl;
					}
				}

				test.Save( "test" + to_string( rule ) + ".png" );
			// }

			config.oneShot = true;
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

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		// draw some shit - need to add a hello triangle to this, so I have an easier starting point for raster stuff
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
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
	engineDemo engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
