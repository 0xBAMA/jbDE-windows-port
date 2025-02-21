#include "../../../engine/engine.h"

class Adam final : public engineBase { // sample derived from base engine class
public:
	Adam () { Init(); OnInit(); PostInit(); }
	~Adam () { Quit(); }

	const int wInitial = 1024;
	const int hInitial = 1024;
	int numLevels = 0;


	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "../src/projects/SignalProcessing/Adam/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Up Mip" ] = computeShader( "../src/projects/SignalProcessing/Adam/shaders/upmip.cs.glsl" ).shaderHandle;

			int w = wInitial;
			int h = hInitial;

			// creating a square image
			textureOptions_t opts;
			opts.textureType = GL_TEXTURE_2D;
			opts.width = w;
			opts.height = h;
			opts.dataType = GL_RGBA32F;
			textureManager.Add( "Adam Color", opts );

			opts.dataType = GL_R32UI;
			textureManager.Add( "Adam Count", opts );

			Image_4U zeroesU( w, h );
			Image_4F zeroesF( w, h );

			int level = 0;
			while ( h >= 1 ) {
				// we half on both dimensions at once... don't have enough levels available for splitting resolution alternately on x and y at each step
				h /= 2; w /= 2; level++;

				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Adam Color" ) );
				glTexImage2D( GL_TEXTURE_2D, level, GL_RGBA32F, w, h, 0, getFormat( GL_RGBA32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );

				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Adam Count" ) );
				glTexImage2D( GL_TEXTURE_2D, level, GL_R32UI, w, h, 0, getFormat( GL_R32UI ), GL_UNSIGNED_BYTE, ( void * ) zeroesU.GetImageDataBasePtr() );
			}

			numLevels = level;

			// now that we have the textures setup, OnUpdate will hook and compute the mips
			NewOffsets();
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// get new data into the input handler
		inputHandler.update();

		if ( inputHandler.getState4( KEY_R ) == KEYSTATE_RISING ) {
			offsets.clear();
		}

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

			// do the mip sweep, so everything propagates to higher mip levels
			MipSweep();

			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
			glUniform1f( glGetUniformLocation( shader, "percentDone" ), 1.0f - ( float( offsets.size() ) / float( wInitial * hInitial ) ) );
			textureManager.BindTexForShader( "Adam Count", "adamCount", shader, 2 );
			textureManager.BindTexForShader( "Adam Color", "adamColor", shader, 3 );

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

		// are there offsets left?
		if ( offsets.size() > 0 ) {
			// doing 16 pixels per frame... that's a full 256x256 update in 4096 frames = ~68 seconds
			for ( int i = 0; i < 1024; i++ ) {
				// write data for one more pixel, at that offset...
				ivec2 loc = offsets[ offsets.size() - 1 ];

				// put a 1 into the pixel for the count...
				uint32_t dataU = 1;
				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Adam Count" ) );
				glTexSubImage2D( GL_TEXTURE_2D, 0, loc.x, loc.y, 1, 1, getFormat( GL_R32UI ), GL_UNSIGNED_INT, &dataU );

				// put a color sample of a circular mask into the pixel for the color...
				rng col = rng( 0.0f, 1.0f );
				vec4 dataC = ( fmod( glm::distance( vec2( loc ), vec2( wInitial / 2.0f, hInitial / 2.0f ) ), 100.0f ) < 35.0f ) ? vec4( ( loc.x % 80 < 40 ) ? 0.0f : 1.0f, col(), ( loc.x % 80 < 40 ) ? 1.0f : 0.0f, 1.0f ) : vec4( 0.0f, 0.0f, 0.0f, 1.0f );

				// static Image_4F testImage( "test2.png" );
				// Image_4F::color col = testImage.GetAtXY( loc.x, loc.y );
				// vec4 dataC = vec4( col[ red ], col[ green ], col[ blue ], 1.0f );

				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Adam Color" ) );
				glTexSubImage2D( GL_TEXTURE_2D, 0, loc.x, loc.y, 1, 1, getFormat( GL_RGBA32F ), GL_FLOAT, &dataC );

				// pop that entry off the list
				offsets.pop_back();
			}
		} else {
			const int w = wInitial;
			const int h = hInitial;

			// clear the first layer of the buffer (this propagates to all levels, next mip sweep)
			static Image_4U zeroesU( w, h );
			static Image_4F zeroesF( w, h );

			glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Adam Color" ) );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, getFormat( GL_RGBA32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );

			glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Adam Count" ) );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, w, h, 0, getFormat( GL_R32UI ), GL_UNSIGNED_BYTE, ( void * ) zeroesU.GetImageDataBasePtr() );

			// generate new offsets
			NewOffsets();
		}
	}

	std::vector< ivec2 > offsets;
	void NewOffsets () {
		for ( int x = 0; x < wInitial; x++ ) {
			for ( int y = 0; y < hInitial; y++ ) {
				offsets.push_back( ivec2( x, y ) );
			}
		}
		// shuffle it around a bit
		static auto rng = std::default_random_engine {};
		std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
		std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
		std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
	}

	void MipSweep () {
		scopedTimer Start( "MIP" );
		int w = wInitial / 2;
		int h = hInitial / 2;
		const GLuint shader = shaders[ "Up Mip" ];
		glUseProgram( shader );
		for ( int n = 0; n < numLevels - 1; n++ ) { // for num mips minus 1

			// bind the appropriate levels for N and N+1 (starting with N=0... to N=...? )
			textureManager.BindImageForShader( "Adam Color", "adamColorN", shader, 2, n );
			textureManager.BindImageForShader( "Adam Color", "adamColorNPlus1", shader, 3, n + 1 );

			textureManager.BindImageForShader( "Adam Count", "adamCountN", shader, 4, n );
			textureManager.BindImageForShader( "Adam Count", "adamCountNPlus1", shader, 5, n + 1 );

			// dispatch the compute shader( 1x1x1 groupsize for simplicity )
			glDispatchCompute( w, h, 1 );
			w /= 2;
			h /= 2;
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
	Adam engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
