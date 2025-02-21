#include "../../../engine/engine.h"

struct CantorDustConfig_t {

	// adjusting the falloff for the histogram values
	float histogramDisplayPower = 0.4f;
	float histogramDisplayScale = 0.3f;

	// which one are we looking at?
	int selectedDataset = 1;

	// windowing the bytes - where do you start, and how many you do
	uint32_t byteOffset = 0;
	uint32_t numBytes = 0;

	// if the data changes, we need to rebuild the histogram
	bool needsUpdate = true;

	// byte vectors for the files
	std::vector< std::vector< unsigned char > > data;

	// string filenames to load
	std::vector< string > filenames = {
		"../src/projects/SignalProcessing/CantorDust/main.cc",								// this file
		"../bin/CantorDust.exe"//,																	// the application generated from this file
		//"./TODO.md",																		// markdown document with lots of URLs
		//"../../Downloads/544141-1-2004-ranger-ford-stock-stock-cragar-soft-8-black.webp",	// random webp
		//"../../Downloads/noctilucentia/noctilucentia.exe",									// wrighter's demo
		//"../PolynomialOptics-Source.zip",													// zip file
		//"../EXRs/mosaic_tunnel_4k.exr",														// exr image format
		//"../2024-07-20 18-22-27.mp4"														// mp4 video format
	};

	GLuint dataBuffer;
	GLuint histogram2DMax;
	GLuint histogram3DMax;
};

class CantorDust final : public engineBase { // sample derived from base engine class
public:
	CantorDust () { Init(); OnInit(); PostInit(); }
	~CantorDust () { Quit(); }

	CantorDustConfig_t CantorDustConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "../src/projects/SignalProcessing/CantorDust/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Update" ] = computeShader( "../src/projects/SignalProcessing/CantorDust/shaders/update.cs.glsl" ).shaderHandle;
			shaders[ "Block" ] = computeShader( "../src/projects/SignalProcessing/CantorDust/shaders/block.cs.glsl" ).shaderHandle;

			// load up all the byte streams
			CantorDustConfig.data.resize( CantorDustConfig.filenames.size() + 1 );
			for ( uint32_t i = 0; i < CantorDustConfig.filenames.size(); i++ ) {
				const string filename = CantorDustConfig.filenames[ i ];
				std::vector< unsigned char > bytes;
				CantorDustConfig.data[ i ].reserve( std::filesystem::file_size( std::filesystem::path( filename ) ) );
				std::ifstream file( filename, ios_base::in | ios_base::binary );
				CantorDustConfig.data[ i ].insert( CantorDustConfig.data[ i ].end(), istreambuf_iterator< char >{ file }, {} );

				uint32_t pad = 3 - ( CantorDustConfig.data[ i ].size() % 4 );
				for ( uint32_t i = 0; i < pad; i++ ) {
					// need to pad with 1-3 zeroes, depending on the situation
					CantorDustConfig.data[ i ].push_back( 0 );
				}
			}

			const int bindex = CantorDustConfig.filenames.size();
			for ( uint32_t z = 0; z <= 255; z++ ) {
				for ( uint32_t y = 0; y <= 255; y++ ) {
					for ( uint32_t x = 0; x <= 255; x++ ) {
						CantorDustConfig.data[ bindex ].push_back( x );
						CantorDustConfig.data[ bindex ].push_back( y );
						CantorDustConfig.data[ bindex ].push_back( z );
					}
				}
			}
			uint32_t pad = 3 - ( CantorDustConfig.data[ bindex ].size() % 4 );
			for ( uint32_t i = 0; i < pad; i++ ) {
				// need to pad with 1-3 zeroes, depending on the situation
				CantorDustConfig.data[ bindex ].push_back( 0 );
			}

			// going to now be passing that into an SSBO, and doing the analysis on the GPU
			// CantorDustConfig.selectedDataset

			// setup a texture to hold the 2D histogram
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= 256;
			opts.height			= 256;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "Histogram2D", opts );

			// and the 3D
			opts.depth			= 256;
			opts.textureType	= GL_TEXTURE_3D;
			textureManager.Add( "Histogram3D", opts );

			// and the buffer to hold the DDA result
			opts.dataType		= GL_R16F;
			opts.width			= config.width - 300; // cropped for the data display
			opts.height			= config.height;
			opts.depth			= 1;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "DDA Result", opts );

			// create the SSBO for the data
			glGenBuffers( 1, &CantorDustConfig.dataBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, CantorDustConfig.dataBuffer );
			uint32_t byteCount = CantorDustConfig.data[ CantorDustConfig.selectedDataset ].size();

			CantorDustConfig.numBytes = byteCount; // update numBytes
			void * data = ( void * ) CantorDustConfig.data[ CantorDustConfig.selectedDataset ].data();
			glBufferData( GL_SHADER_STORAGE_BUFFER, byteCount, data, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, CantorDustConfig.dataBuffer );

			// buffers for the histogram max values
			constexpr uint32_t countValue = 0;
			glGenBuffers( 1, &CantorDustConfig.histogram2DMax );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, CantorDustConfig.histogram2DMax );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, CantorDustConfig.histogram2DMax );

			glGenBuffers( 1, &CantorDustConfig.histogram3DMax );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, CantorDustConfig.histogram3DMax );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, CantorDustConfig.histogram3DMax );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// get new data into the input handler
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

		{
			CantorDustConfig.byteOffset += 256;
			CantorDustConfig.byteOffset = CantorDustConfig.byteOffset % CantorDustConfig.data[ CantorDustConfig.selectedDataset ].size();
			CantorDustConfig.numBytes = 256 * 128;
			CantorDustConfig.needsUpdate = true;
		}

		if ( CantorDustConfig.needsUpdate ) {
			CantorDustConfig.needsUpdate = false;

			scopedTimer Start( "Update" );
			const GLuint shader = shaders[ "Update" ];
			glUseProgram( shader );

			glUniform1ui( glGetUniformLocation( shader, "byteOffset" ), CantorDustConfig.byteOffset );
			glUniform1ui( glGetUniformLocation( shader, "numBytes" ), CantorDustConfig.numBytes );
			textureManager.BindImageForShader( "Histogram2D", "dataTexture2D", shader, 2 );
			textureManager.BindImageForShader( "Histogram3D", "dataTexture3D", shader, 3 );

			// reset the max
			constexpr uint32_t countValue = 0;
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, CantorDustConfig.histogram2DMax );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, CantorDustConfig.histogram2DMax );

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, CantorDustConfig.histogram3DMax );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, CantorDustConfig.histogram3DMax );

			// reset the buffers
			textureManager.ZeroTexture2D( "Histogram2D" );
			textureManager.ZeroTexture3D( "Histogram3D" );

			const uint32_t numBytes = CantorDustConfig.data[ CantorDustConfig.selectedDataset ].size();
			const uint32_t workgroupsRoundedUp = ( numBytes + 63 ) / 64;
			glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1u ), 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{
			scopedTimer Start( "DDA Block Prep" );
			const GLuint shader = shaders[ "Block" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1000.0f );

			textureManager.BindImageForShader( "Histogram2D", "dataTexture2D", shader, 2 );
			textureManager.BindImageForShader( "Histogram3D", "dataTexture3D", shader, 3 );
			textureManager.BindImageForShader( "DDA Result", "blockImage", shader, 4 );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1ui( glGetUniformLocation( shader, "byteOffset" ), CantorDustConfig.byteOffset );
			glUniform1ui( glGetUniformLocation( shader, "numBytes" ), CantorDustConfig.numBytes );

			glUniform1f( glGetUniformLocation( shader, "histogramDisplayScale" ), CantorDustConfig.histogramDisplayScale );
			glUniform1f( glGetUniformLocation( shader, "histogramDisplayPower" ), CantorDustConfig.histogramDisplayPower );

			textureManager.BindImageForShader( "Histogram2D", "dataTexture2D", shader, 2 );
			textureManager.BindImageForShader( "Histogram3D", "dataTexture3D", shader, 3 );
			textureManager.BindImageForShader( "DDA Result", "blockImage", shader, 4 );

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
	CantorDust engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
