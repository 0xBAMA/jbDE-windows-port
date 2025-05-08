#include "../../engine/engine.h"

#include "DLA.h"

class DLA final : public engineBase { // sample derived from base engine class
public:
	DLA () { Init(); OnInit(); PostInit(); }
	~DLA () { Quit(); }

	DLAModelGPU d;

	float zoom = 2.0f;
	float verticalOffset = 2.0f;
	vec3 viewerPosition = vec3( 6.0f, 6.0f, 6.0f );

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "../src/projects/DLA/shaders/draw.cs.glsl" ).shaderHandle;

			/*
			const int numThreads = 4;
			std::thread threads[ numThreads ]; // create thread pool
			for ( int id = 0; id < numThreads; id++ ) { // do work
				threads[ id ] = std::thread(
					[ this, id ]() {

						DLAModelCPU d;
						d.Init();
						d.threadIDX = id;
						d.RunBatch( 10000 );

					}
				);
			}

			for ( int id = 0; id <= numThreads; id++ )
				threads[ id ].join();
			*/

			d.textureManager = &textureManager;
			d.Init();
			d.Run( 100 );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		const bool* state = SDL_GetKeyboardState( NULL );

		if ( state[ SDL_SCANCODE_RIGHT ] ) {
			glm::quat rot = glm::angleAxis( -0.01f, vec3( 0.0f, 0.0f, 1.0f ) );
			viewerPosition = ( rot * vec4( viewerPosition, 0.0f ) ).xyz();
		}

		if ( state[ SDL_SCANCODE_LEFT ] ) {
			glm::quat rot = glm::angleAxis( 0.01f, vec3( 0.0f, 0.0f, 1.0f ) );
			viewerPosition = ( rot * vec4( viewerPosition, 0.0f ) ).xyz();
		}

		if ( state[ SDL_SCANCODE_UP ] ) {
			verticalOffset += 0.1f;
		}

		if ( state[ SDL_SCANCODE_DOWN ] ) {
			verticalOffset -= 0.1f;
		}

		// zoom in and out with plus/minus
		if ( state[ SDL_SCANCODE_MINUS ] ) {
			zoom += 0.1f;
		}

		if ( state[ SDL_SCANCODE_EQUALS ] ) {
			zoom -= 0.1f;
		}

		if ( state[ SDL_SCANCODE_R ] ) {
			textureManager.ZeroTexture3D( "DLA Texture" );
			d.ResetParticles();
			d.ResetField();
		}

		if ( state[ SDL_SCANCODE_Y ] ) {
			d.ReloadShaders();
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
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			textureManager.BindImageForShader( "DLA Texture", "DLATexture", shader, 2 );
			glUniform1f( glGetUniformLocation( shader, "zoom" ), zoom );
			glUniform1f( glGetUniformLocation( shader, "verticalOffset" ), verticalOffset );
			glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( viewerPosition ) );
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

		// shader to apply dithering
			// ...

		// other postprocessing
			// ...

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

		d.Run( 1 );
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
	DLA engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
