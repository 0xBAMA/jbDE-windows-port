#include "../../../engine/engine.h"

struct path2DConfig_t {
	GLuint maxBuffer = 0;
	ivec2 dims = ivec2( 2880 / 1, 1800 / 1 );

	uint32_t autoExposureBufferDim = 0;
	uint32_t autoExposureMipLevels = 0;
	float autoExposureBase = 1600000.0f;
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
			shaders[ "Autoexposure Prep" ] = computeShader( "../src/projects/PathTracing/path2D_forward/shaders/autoexposurePrep.cs.glsl" ).shaderHandle;
			shaders[ "Autoexposure" ] = computeShader( "../src/projects/PathTracing/path2D_forward/shaders/autoexposure.cs.glsl" ).shaderHandle;

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
				d /= 2; level++;
				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Field Max" ) );
				glTexImage2D( GL_TEXTURE_2D, level, GL_R32F, d, d, 0, getFormat( GL_R32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );
			}
			path2DConfig.autoExposureMipLevels = level;


			// Image_4F pdfLUT( "../src/projects/PathTracing/path2D_forward/LUT/Preprocessed/Uniform.png" );
			// Image_4F pdfLUT( "../src/projects/PathTracing/path2D_forward/LUT/Preprocessed/Incandescent.png" );


			// loading the emission spectra LUTs
			Image_4F pdfLUT( "../src/projects/PathTracing/path2D_forward/LUT/Preprocessed/Flourescent.png" );

			// First step is populating the cumulative distribution function... "how much of the curve have we passed" (accumulated integral)
			std::vector< float > cdf;
			float cumSum = 0.0f;
			for ( int x = 0; x < pdfLUT.Width(); x++ ) {
				float sum = 0.0f;
				for ( int y = 0; y < pdfLUT.Height(); y++ ) {
				// invert because lut uses dark for positive indication... maybe fix that
					sum += 1.0f - pdfLUT.GetAtXY( x, y ).GetLuma();
				}
				// increment cumulative sum and CDF
				cumSum += sum;
				cdf.push_back( cumSum );
			}

			// normalize the CDF values by the final value during CDF sweep
			std::vector< vec2 > CDFpoints;
			for ( int x = 0; x < pdfLUT.Width(); x++ ) {
			// compute the inverse CDF with the aid of a series of 2d points along the curve
				// adjust baseline for our desired range -> 380nm to 830nm, we have 450nm of data
				CDFpoints.emplace_back( x + 380, cdf[ x ] / cumSum );
			}

			Image_1F inverseCDF( 1024, 1 );
			for ( int x = 0; x < 1024; x++ ) {
				// each pixel along this strip needs a value of the inverse CDF
					// this is the intersection with the line defined by the set of segments in the array CDFpoints
				float normalizedPosition = ( x + 0.5f ) / 1024.0f;
				for ( int p = 0; p < CDFpoints.size(); p++ )
					if ( p == ( CDFpoints.size() - 1 ) ) {
						inverseCDF.SetAtXY( x, 0, color_1F( { CDFpoints[ p ].x } ) );
					} else if ( CDFpoints[ p ].y >= normalizedPosition ) {
						inverseCDF.SetAtXY( x, 0, color_1F( { RangeRemap( normalizedPosition,
								CDFpoints[ p - 1 ].y, CDFpoints[ p ].y, CDFpoints[ p - 1 ].x, CDFpoints[ p ].x ) } ) );
						break;
					}
			}

			// we now have the solution for the LUT
			opts.width = 1024;
			opts.height = 1;
			opts.dataType = GL_R32F;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.textureType = GL_TEXTURE_2D;
			opts.pixelDataType = GL_FLOAT;
			opts.initialData = inverseCDF.GetImageDataBasePtr();
			textureManager.Add( "iCDF", opts );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		if ( inputHandler.getState( KEY_T ) ) {
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
			screenshotRequested = true;
		}
		ImGui::End();

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void ComputePasses () {
		ZoneScoped;

		{
			scopedTimer Start( "Autoexposure" );
			{	// clear the texture with the max value
				textureManager.ZeroTexture2D( "Field Max" );
			}

			{	// populate mip 0 with "proposed pixel brightness values" with no autoexposure setting
					// potential future optimization here, do textureGathers during this step and mip 0 can be half res
				const GLuint shader = shaders[ "Autoexposure Prep" ];
				glUseProgram( shader );
				textureManager.BindImageForShader( "Field Max", "fieldMax", shader, 0 );
				textureManager.BindTexForShader( "Field Y Tally", "bufferImageY", shader, 2 );
				textureManager.BindTexForShader( "Field Count", "bufferImageCount", shader, 4 );
				glUniform1f( glGetUniformLocation( shader, "autoExposureBase" ), path2DConfig.autoExposureBase );
				glDispatchCompute( ( path2DConfig.dims.x + 15 ) / 16, ( path2DConfig.dims.y + 15 ) / 16, 1 );
				glMemoryBarrier( GL_ALL_BARRIER_BITS );
			}

			{	// mip propagation of brightness max
				int d = path2DConfig.autoExposureBufferDim / 2;
				const GLuint shader = shaders[ "Autoexposure" ];
				glUseProgram( shader );
				for ( int n = 0; n < path2DConfig.autoExposureMipLevels - 1; n++ ) { // for num mips minus 1

					// bind the appropriate levels for N and N+1 (starting with N=0... to N=...? ) double bind of texture version... yeah
					textureManager.BindTexForShader( "Field Max", "layerN", shader, 0 );
					textureManager.BindImageForShader( "Field Max", "layerN", shader, 0, n );
					textureManager.BindImageForShader( "Field Max", "layerNPlus1", shader, 1, n + 1 );

					// dispatch the compute shader( 1x1x1 groupsize for simplicity )
					glDispatchCompute( d, d, 1 );
					glMemoryBarrier( GL_ALL_BARRIER_BITS );
					d /= 2;
				}
				// postcondition is that the top mip's single texel contains the field max, and we can access that during drawing to get it into a reasonable range
			}
		}

		{ // prep accumumator texture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			textureManager.BindTexForShader( "Field X Tally", "bufferImageX", shaders[ "Draw" ], 2 );
			textureManager.BindTexForShader( "Field Y Tally", "bufferImageY", shaders[ "Draw" ], 3 );
			textureManager.BindTexForShader( "Field Z Tally", "bufferImageZ", shaders[ "Draw" ], 4 );
			textureManager.BindTexForShader( "Field Count", "bufferImageCount", shaders[ "Draw" ], 5 );
			textureManager.BindImageForShader( "Field Max", "fieldMax", shaders[ "Draw" ], 6 );
			glUniform1i( glGetUniformLocation( shaders[ "Draw" ], "autoExposureTexOffset" ), path2DConfig.autoExposureMipLevels );
			glUniform1f( glGetUniformLocation( shaders[ "Draw" ], "autoExposureBase" ), path2DConfig.autoExposureBase );
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
		textureManager.BindTexForShader( "iCDF", "iCDFtex", shader, 6 );

		// glDispatchCompute( ( path2DConfig.dims.x + 15 ) / 16, ( path2DConfig.dims.y + 15 ) / 16, 1 );
		// glDispatchCompute( 6, 6, 3 );
		glDispatchCompute( 2, 2, 2 );
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
