#include "../../../engine/engine.h"

class GPUSpherePack final : public engineBase { // sample derived from base engine class
public:
	GPUSpherePack () { Init(); OnInit(); PostInit(); }
	~GPUSpherePack () { Quit(); }

	bool swap = false;
	// ivec3 bufferDims = ivec3( 640, 360, 64 );
	ivec3 bufferDims = ivec3( 512, 256, 64 );
	vec2 viewOffset = vec2( 0.0f );
	float scale = 500.0f;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "../src/projects/SignalProcessing/GPUSpherePack/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Update" ] = computeShader( "../src/projects/SignalProcessing/GPUSpherePack/shaders/update.cs.glsl" ).shaderHandle;

			// create the buffer texture
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32UI;
			opts.textureType	= GL_TEXTURE_3D;
			opts.width			= bufferDims.x;
			opts.height			= bufferDims.y;
			opts.depth			= bufferDims.z;
			textureManager.Add( "Buffer 0", opts );
			textureManager.Add( "Buffer 1", opts );

			{
				const GLuint shader = shaders[ "Update" ];
				glUseProgram( shader );

				static rngi wangSeeder( 1, 4000000000 );
				glUniform1ui( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );

				glUniform1i( glGetUniformLocation( shader, "resetFlag" ), 1 );
				textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "0" : "1" ), "bufferTexture", shader, 2 );
				textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "1" : "0" ), "bufferTexture", shader, 3 );
				swap = !swap;

				glDispatchCompute( ( bufferDims.x + 7 ) / 8, ( bufferDims.y + 7 ) / 8, ( bufferDims.z + 7 ) / 8 );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

				glUniform1i( glGetUniformLocation( shader, "resetFlag" ), 1 );
				textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "0" : "1" ), "bufferTexture", shader, 2 );
				textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "1" : "0" ), "bufferTexture", shader, 3 );
				swap = !swap;

				glDispatchCompute( ( bufferDims.x + 7 ) / 8, ( bufferDims.y + 7 ) / 8, ( bufferDims.z + 7 ) / 8 );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}
		}
	}

	void HandleCustomEvents () {
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
			GLuint shader = shaders[ "Draw" ];
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
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
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
	GPUSpherePack engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
