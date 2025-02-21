#include "../../../engine/engine.h"
#include "bvh.h"

class BVHtest final : public engineBase { // sample derived from base engine class
public:
	BVHtest () { Init(); OnInit(); PostInit(); }
	~BVHtest () { Quit(); }

	testRenderer_t renderer;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "../src/projects/PathTracing/BVHtest/shaders/draw.cs.glsl" ).shaderHandle;

			// create the image buffer for GPU display
			textureOptions_t opts;
			opts.width			= renderer.imageBuffer.Width();
			opts.height			= renderer.imageBuffer.Height();
			opts.dataType		= GL_RGBA32F;
			opts.minFilter		= GL_LINEAR;
			opts.magFilter		= GL_LINEAR;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			opts.pixelDataType	= GL_FLOAT;
			opts.initialData	= ( void * ) renderer.imageBuffer.GetImageDataBasePtr();
			textureManager.Add( "Image Buffer", opts );

			// == Load the Model =====================================================================
			terminal.addCommand( { "LoadModel" }, {},
				[=] ( args_t args ) {
					auto tStart = std::chrono::system_clock::now();

					// load the model
					renderer.accelerationStructure.Init();
					aabb_t bounds = renderer.accelerationStructure.Load();

					// report timing + stats
					auto tStop = std::chrono::system_clock::now();
					float timeTaken = std::chrono::duration_cast< std::chrono::microseconds >( tStop - tStart ).count() / 1000.0f;
					terminal.addHistoryLine( terminal.csb.append( "Model Loading finished in " + to_string( timeTaken ) + "ms" ).flush() );
					terminal.addHistoryLine( terminal.csb.append( "Model contains " + to_string( renderer.accelerationStructure.triangleList.size() ) + " triangles" ).flush() );
					terminal.addHistoryLine( terminal.csb.append( "Bounds:" ).flush() );
					terminal.addHistoryLine( terminal.csb.append( " X: " ).append( to_string( bounds.mins.x ) ).append( " to " ).append( to_string( bounds.maxs.x ) ).flush() );
					terminal.addHistoryLine( terminal.csb.append( " Y: " ).append( to_string( bounds.mins.y ) ).append( " to " ).append( to_string( bounds.maxs.y ) ).flush() );
					terminal.addHistoryLine( terminal.csb.append( " Z: " ).append( to_string( bounds.mins.z ) ).append( " to " ).append( to_string( bounds.maxs.z ) ).flush() );
				}, "Load the model from disk." );

			// == Build the BVH ======================================================================
			terminal.addCommand( { "BuildBVH" }, {},
				[=] ( args_t args ) {
					auto tStart = std::chrono::system_clock::now();

					// use the loaded model to build a bvh
					renderer.accelerationStructure.BuildTree();

					// report how long it took + some stats
					auto tStop = std::chrono::system_clock::now();
					float timeTaken = std::chrono::duration_cast< std::chrono::microseconds >( tStop - tStart ).count() / 1000.0f;
					terminal.addHistoryLine( terminal.csb.append( "BVH Build finished in " + to_string( timeTaken ) + "ms" ).flush() );

				}, "Build the BVH from the currently loaded model." );

			// == Render an Image ====================================================================
			terminal.addCommand( { "RenderImage" }, {},
				[=] ( args_t args ) {
					auto tStart = std::chrono::system_clock::now();

					// add some cvars (with defaults) for setting the following:
						// viewer position
						// viewer direction
						// ...

					// run the accelerated traversal
					renderer.acceleratedTraversal();

					// then put the result in the "Image Buffer" texture, to look at it
					glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Image Buffer" ) );
					glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, renderer.imageBuffer.Width(), renderer.imageBuffer.Height(), 0, GL_RGBA, GL_FLOAT, ( void * ) renderer.imageBuffer.GetImageDataBasePtr() );

					// maybe also save to disk, tbd

					auto tStop = std::chrono::system_clock::now();
					float timeTaken = std::chrono::duration_cast< std::chrono::microseconds >( tStop - tStart ).count() / 1000.0f;
					terminal.addHistoryLine( terminal.csb.append( "Render Image finished in " + to_string( timeTaken ) + "ms" ).flush() );

				}, "Render an image from the built BVH." );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal
		terminal.update( inputHandler );
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
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );
			// glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
			textureManager.BindImageForShader( "Image Buffer", "CPUTexture", shader, 2 );
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

			// clear buffer
			textRenderer.Clear();

			// show timestamp
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			// show terminal, if active - active check happens inside
			textRenderer.drawTerminal( terminal );

			// composite down to the display tex
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
	BVHtest engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
