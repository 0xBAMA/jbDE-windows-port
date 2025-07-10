#include "../../../engine/engine.h"

struct path2DConfig_t {
	GLuint maxBuffer;
	ivec2 dims = ivec2( 2880 / 4, 1800 / 4 );
};

class path2D final : public engineBase { // sample derived from base engine class
public:
	path2D () { Init(); OnInit(); PostInit(); }
	~path2D () { Quit(); }

	path2DConfig_t path2DConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// image prep
			shaders[ "Draw" ] = computeShader( "../src/projects/PathTracing/path2D_forward/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Simulate" ] = computeShader( "../src/projects/PathTracing/path2D_forward/shaders/simulate.cs.glsl" ).shaderHandle;

			// field max, single value
			constexpr uint32_t countValue = 0;
			glGenBuffers( 1, &path2DConfig.maxBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, path2DConfig.maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 1, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, path2DConfig.maxBuffer );

			// buffer image
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32UI;
			opts.width			= path2DConfig.dims.x;
			opts.height			= path2DConfig.dims.y;
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			// parallel averaging via atomic adds in the line drawing function
			textureManager.Add( "Field X Tally", opts );
			textureManager.Add( "Field Y Tally", opts );
			textureManager.Add( "Field Z Tally", opts );
			textureManager.Add( "Field Count", opts );
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
	}

	void ComputePasses () {
		ZoneScoped;

		{ // prep accumumator texture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			textureManager.BindTexForShader( "Field X Tally", "bufferImageX", shaders[ "Draw" ], 2 );
			textureManager.BindTexForShader( "Field Y Tally", "bufferImageY", shaders[ "Draw" ], 3 );
			textureManager.BindTexForShader( "Field Z Tally", "bufferImageZ", shaders[ "Draw" ], 4 );
			textureManager.BindTexForShader( "Field Count", "bufferImageCount", shaders[ "Draw" ], 5 );

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

		// run the shader to run some rays
		static rngi wangSeeder = rngi( 0, 1000000 );
		const GLuint shader = shaders[ "Simulate" ];
		glUseProgram( shader );
		glUniform1f( glGetUniformLocation( shader, "t" ), SDL_GetTicks() / 5000.0f );
		glUniform1i( glGetUniformLocation( shader, "rngSeed" ), wangSeeder() );

		textureManager.BindImageForShader( "Field X Tally", "bufferImageX", shader, 2 );
		textureManager.BindImageForShader( "Field Y Tally", "bufferImageY", shader, 2 );
		textureManager.BindImageForShader( "Field Z Tally", "bufferImageZ", shader, 2 );
		textureManager.BindImageForShader( "Field Count", "bufferImageCount", shader, 2 );

		glDispatchCompute( ( path2DConfig.dims.x + 15 ) / 16, ( path2DConfig.dims.y + 15 ) / 16, 1 );
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
	path2D engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
