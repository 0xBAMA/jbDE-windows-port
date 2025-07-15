#include "../../../engine/engine.h"

struct path2DConfig_t {
	GLuint maxBuffer = 0;
	ivec2 dims = ivec2( 2880 / 1, 1800 / 1 );

	uint32_t autoExposureBufferDim = 0;
};

class path2D final : public engineBase { // sample derived from base engine class
public:
	path2D () { Init(); OnInit(); PostInit(); }
	~path2D () { Quit(); }

	path2DConfig_t path2DConfig;
	bool screenshotRequested = false;
	int screenshotIndex = -1;

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
			opts.dataType		= GL_R32I;
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

		// additional buffer used for autoexposure
			// round up the dimensions
			path2DConfig.autoExposureBufferDim = nextPowerOfTwo( std::max( path2DConfig.dims.x, path2DConfig.dims.y ) );

			opts.width			= path2DConfig.autoExposureBufferDim;
			opts.height			= path2DConfig.autoExposureBufferDim;
			opts.dataType		= GL_R32F;
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			textureManager.Add( "Field Max", opts );

			// create the mip levels explicitly... we want to be able to sample the texel (0,0) of the highest mip of the texture for the autoexposure term
			int level = 0;
			int d = path2DConfig.autoExposureBufferDim;
			Image_4F zeroesF( d, d );
			while ( d >= 1 ) {
				d /= 2; d /= 2; level++;
				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Field Max" ) );
				glTexImage2D( GL_TEXTURE_2D, level, GL_R32F, d, d, 0, getFormat( GL_R32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );
			}
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		if ( inputHandler.getState( KEY_T ) ) {
			cout << "Screenshot Requested" << endl;
			screenshotRequested = true;
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

		ImGui::Begin( "Screenshot Window" );
		if ( ImGui::Button(  "Screenshot" ) ) {
			cout << "Screenshot Requested" << endl;
			screenshotRequested = true;
		}
		ImGui::End();

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

		if ( screenshotRequested == true ) {
			screenshotRequested = false;
			cout << "Attempting Screenshot" << endl;
			const GLuint tex = textureManager.Get( "Display Texture" );
			uvec2 dims = textureManager.GetDimensions( "Display Texture" );
			std::vector< float > imageBytesToSave;
			imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
			glBindTexture( GL_TEXTURE_2D, tex );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
			Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
			screenshot.RGBtoSRGB();
			screenshot.FlipVertical();
			const string filename = ( ( screenshotIndex != -1 ) ? ( string( "frames/" ) + fixedWidthNumberString( screenshotIndex, 4 ) ) :
				string( "Path2D_Forward-" ) + timeDateString() ) + string( ".png" );
			screenshot.Save( filename, Image_4F::backend::LODEPNG );
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

		/*
		static int t = 1711;
		static int samples = 0;
		static bool resetRequested = false;
		if ( resetRequested ) {
			resetRequested = false;
			textureManager.ZeroTexture2D( "Field X Tally" );
			textureManager.ZeroTexture2D( "Field Y Tally" );
			textureManager.ZeroTexture2D( "Field Z Tally" );
			textureManager.ZeroTexture2D( "Field Count" );
		}
		if ( samples++ == 200 ) {
			samples = 0;
			t--;
			if ( t == 1690 ) {
				pQuit = true;
			}
			screenshotRequested = true;
			screenshotIndex = t;
			resetRequested = true;
		}
		*/
		// glUniform1f( glGetUniformLocation( shader, "t" ), SDL_GetTicks() / 5000.0f );
		glUniform1f( glGetUniformLocation( shader, "t" ), 0.0f );
		glUniform1i( glGetUniformLocation( shader, "rngSeed" ), wangSeeder() );

		rng offset = rng( 0, 512 );
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), offset(), offset() );

		textureManager.BindTexForShader( "Blue Noise", "blueNoise", shader, 0 );
		textureManager.BindImageForShader( "Field X Tally", "bufferImageX", shader, 2 );
		textureManager.BindImageForShader( "Field Y Tally", "bufferImageY", shader, 3 );
		textureManager.BindImageForShader( "Field Z Tally", "bufferImageZ", shader, 4 );
		textureManager.BindImageForShader( "Field Count", "bufferImageCount", shader, 5 );

		// glDispatchCompute( ( path2DConfig.dims.x + 15 ) / 16, ( path2DConfig.dims.y + 15 ) / 16, 1 );
		glDispatchCompute( 6, 6, 3 );
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
		inputHandler.update();
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
