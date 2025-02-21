#include "../../../engine/engine.h"

struct CausticConfig_t {
	GLuint maxBuffer;

};

class Caustic final : public engineBase { // sample derived from base engine class
public:
	Caustic () { Init(); OnInit(); PostInit(); }
	~Caustic () { Quit(); }

	CausticConfig_t CausticConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// image prep
			shaders[ "Draw" ] = computeShader( "../src/projects/PathTracing/Caustic/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Simulate" ] = computeShader( "../src/projects/PathTracing/Caustic/shaders/simulate.cs.glsl" ).shaderHandle;

			// field max, single value
			constexpr uint32_t countValue = 0;
			glGenBuffers( 1, &CausticConfig.maxBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, CausticConfig.maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 1, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, CausticConfig.maxBuffer );

			// buffer image
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= 1024;
			opts.height			= 1024;
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			textureManager.Add( "Field", opts );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		// const uint8_t * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		// const SDL_Keymod k	= SDL_GetModState();
		// const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

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

		// if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // prep accumumator texture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			textureManager.BindImageForShader( "Field", "bufferImage", shaders[ "Draw" ], 2 );
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
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		// clear out the buffers
		//static constexpr uint32_t zeroes[ 1024 * 1024 ] = { 0 };

		// glBindBuffer( GL_SHADER_STORAGE_BUFFER, CausticConfig.maxBuffer );
		// glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &zeroes[ 0 ], GL_DYNAMIC_COPY );
		// glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, CausticConfig.maxBuffer );

		//glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Field" ) );
		//glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, 1024, 1024, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &zeroes[ 0 ] );
		//glMemoryBarrier( GL_ALL_BARRIER_BITS );

		textureManager.ZeroTexture2D( "Field" );

		// run the shader to run some rays
		static rngi wangSeeder = rngi( 0, 100000 );
		const GLuint shader = shaders[ "Simulate" ];
		glUseProgram( shader );
		glUniform1f( glGetUniformLocation( shader, "t" ), SDL_GetTicks() / 5000.0f );
		glUniform1i( glGetUniformLocation( shader, "rngSeed" ), wangSeeder() );
		textureManager.BindImageForShader( "Field", "bufferImage", shader, 0 );
		glDispatchCompute( ( 1024 ) / 16, ( 1024 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
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
	Caustic engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
