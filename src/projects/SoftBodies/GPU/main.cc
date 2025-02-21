#include "../../../engine/engine.h"

struct node_t {
	float nodeMass;
	vec3 position;
	vec3 velocity;
	uint32_t anchored; // anchored only needs 1 bit - extra bits for number of connected nodes?

	// how do we want to represent the list of connected nodes?

};

class SoftBodiesGPU final : public engineBase {
public:
	SoftBodiesGPU () { Init(); OnInit(); PostInit(); }
	~SoftBodiesGPU () { Quit(); }

	GLuint OffsetSSBO;
	GLuint DataSSBOs[ 2 ];

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );
			// something to put some basic data in the accumulator texture
			shaders[ "Dummy Draw" ] = computeShader( "./src/projects/SoftBodies/GPU/shaders/dummyDraw.cs.glsl" ).shaderHandle;

		// create the initial data

			// populate the simulation model
			std::vector< node_t > softbodyModel;
				// ...

			// linearize the data, calculate offsets
			std::vector< float > data;
			std::vector< uint32_t > offsets;
				// ...

			// calculate the size of the data
			size_t numBytesData = data.size() * sizeof( float );
			size_t numBytesOffsets = offsets.size() * sizeof( uint32_t );

			// create two SSBOs, front buffer/back buffer for sim points
			glCreateBuffers( 2, &DataSSBOs[ 0 ] );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, DataSSBOs[ 0 ] );
			glBufferData( GL_SHADER_STORAGE_BUFFER, numBytesData, ( GLvoid * ) &data[ 0 ], GL_DYNAMIC_COPY );

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, DataSSBOs[ 1 ] );
			glBufferData( GL_SHADER_STORAGE_BUFFER, numBytesData, ( GLvoid * ) &data[ 0 ], GL_DYNAMIC_COPY );

			// create an SSBO with a set of parallel prefix sum offsets ( since each node can have a variable size footprint )
				// usage: to get the offset into the data SSBO for node N, access offsets[ N ]
			glCreateBuffers( 1, &OffsetSSBO );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, OffsetSSBO );
			glBufferData( GL_SHADER_STORAGE_BUFFER, numBytesOffsets, ( GLvoid * ) &offsets[ 0 ], GL_STATIC_READ );

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// const uint8_t * state = SDL_GetKeyboardState( NULL );
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
		// draw some shit
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
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
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
	SoftBodiesGPU engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
