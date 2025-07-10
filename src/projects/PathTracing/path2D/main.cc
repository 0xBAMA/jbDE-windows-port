#include "../../../engine/engine.h"
#include "tileDispenser.h"

struct path2DConfig_t {
	GLuint maxBuffer;
	ivec2 dims = ivec2( 1920 * 2, 1080 * 2 );

	tileDispenser dispenser;
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
			shaders[ "Draw" ] = computeShader( "../src/projects/PathTracing/path2D/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Simulate" ] = computeShader( "../src/projects/PathTracing/path2D/shaders/simulate.cs.glsl" ).shaderHandle;

			// field max, single value
			constexpr uint32_t countValue = 0;
			glGenBuffers( 1, &path2DConfig.maxBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, path2DConfig.maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 1, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, path2DConfig.maxBuffer );

			// buffer image
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32F;
			opts.width			= path2DConfig.dims.x;
			opts.height			= path2DConfig.dims.y;
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			textureManager.Add( "Field", opts );

			path2DConfig.dispenser = tileDispenser( 256, path2DConfig.dims.x, path2DConfig.dims.y );
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
			textureManager.BindTexForShader( "Field", "bufferImage", shaders[ "Draw" ], 2 );
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

		// get a screenshot after N frames
		if ( path2DConfig.dispenser.tileListPasses == 4096 ) {
			const GLuint tex = textureManager.Get( "Display Texture" );
			uvec2 dims = textureManager.GetDimensions( "Display Texture" );
			std::vector< float > imageBytesToSave;
			imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
			glBindTexture( GL_TEXTURE_2D, tex );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
			Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
			screenshot.RGBtoSRGB();
			const string filename = string( "Path2D-" ) + timeDateString() + string( ".png" );
			screenshot.Save( filename, Image_4F::backend::LODEPNG );
			pQuit = true;
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.DrawBlackBackedColorString( 3, to_string( path2DConfig.dispenser.tileListPasses ) + "/4096", GOLD );
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

		for ( int i = 0; i < 5; i++ ) {
			ivec2 tileOffset = path2DConfig.dispenser.GetTile();
			glUniform2i( glGetUniformLocation( shader, "tileOffset" ), tileOffset.x, tileOffset.y );
			glUniform1f( glGetUniformLocation( shader, "t" ), SDL_GetTicks() / 5000.0f );
			glUniform1i( glGetUniformLocation( shader, "rngSeed" ), wangSeeder() );
			textureManager.BindImageForShader( "Field", "bufferImage", shader, 2 );
			glDispatchCompute( ( path2DConfig.dispenser.tileSize + 15 ) / 16, ( path2DConfig.dispenser.tileSize + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
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
