#include "../../../engine/engine.h"

class GPUSpherePack final : public engineBase { // sample derived from base engine class
public:
	GPUSpherePack () { Init(); OnInit(); PostInit(); }
	~GPUSpherePack () { Quit(); }

	bool swap = false;
	// ivec3 bufferDims = ivec3( 640, 360, 64 );
	ivec3 bufferDims = ivec3( 512, 256, 64 );
	vec2 viewOffset = vec2( 0.0f );
	float scale = 500.0f;

	GLuint readoutBuffer = 0;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "../src/projects/SignalProcessing/GPUSpherePack/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Update" ] = computeShader( "../src/projects/SignalProcessing/GPUSpherePack/shaders/update.cs.glsl" ).shaderHandle;
			shaders[ "Readout" ] = computeShader( "../src/projects/SignalProcessing/GPUSpherePack/shaders/readout.cs.glsl" ).shaderHandle;

			// create the buffer texture
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32UI;
			opts.textureType	= GL_TEXTURE_3D;
			opts.width			= bufferDims.x;
			opts.height			= bufferDims.y;
			opts.depth			= bufferDims.z;
			textureManager.Add( "Buffer 0", opts );
			textureManager.Add( "Buffer 1", opts );

			{
				const GLuint shader = shaders[ "Update" ];
				glUseProgram( shader );

				static rngi wangSeeder( 1, 4000000000 );
				glUniform1ui( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );

				glUniform1i( glGetUniformLocation( shader, "resetFlag" ), 1 );
				textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "0" : "1" ), "bufferTexture", shader, 2 );
				textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "1" : "0" ), "bufferTexture", shader, 3 );
				swap = !swap;

				glDispatchCompute( ( bufferDims.x + 7 ) / 8, ( bufferDims.y + 7 ) / 8, ( bufferDims.z + 7 ) / 8 );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

				glUniform1i( glGetUniformLocation( shader, "resetFlag" ), 1 );
				textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "0" : "1" ), "bufferTexture", shader, 2 );
				textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "1" : "0" ), "bufferTexture", shader, 3 );
				swap = !swap;

				glDispatchCompute( ( bufferDims.x + 7 ) / 8, ( bufferDims.y + 7 ) / 8, ( bufferDims.z + 7 ) / 8 );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}

			{ // creating the readout buffer
				// need 4 bytes per channel, and 2 channels...
				glCreateBuffers( 1, &readoutBuffer );
				glBindBuffer( GL_SHADER_STORAGE_BUFFER, readoutBuffer );
				glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( float ) * 2 * bufferDims.x * bufferDims.y * bufferDims.z, NULL, GL_DYNAMIC_COPY );
				glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, readoutBuffer );
			}
		}
	}

	void BlockCapture () {

		vector< uint8_t > data;
		data.resize( bufferDims.x * bufferDims.y * bufferDims.z * 4 * 2, 0 );

		{ // get the data from the GPU
			// invoke the shader to prepare the readout buffer
			glUseProgram( shaders[ "Readout" ] );
			textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "0" : "1" ), "bufferTexture", shaders[ "Readout" ], 3 );
			glDispatchCompute( ( bufferDims.x + 7 ) / 8, ( bufferDims.y + 7 ) / 8, ( bufferDims.z + 7 ) / 8 );
			glMemoryBarrier( GL_ALL_BARRIER_BITS );

			// readout the buffer
			glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, 8 * bufferDims.x * bufferDims.y * bufferDims.z, &data[ 0 ] );
		}

		// encode information into a voxel block - we can keep 1 byte per channel losslessly in a PNG
		Image_4U seedBlock( bufferDims.x * 2, bufferDims.y * bufferDims.z );
		for ( int i = 0; i < 4 * 2 * bufferDims.x * bufferDims.y * bufferDims.z; i++ ) {
			seedBlock.GetImageDataBasePtr()[ i ] = data[ i ];
		}

		// save it back out
		seedBlock.Save( "seedBlock.png" );
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		const bool shift = inputHandler.getState( KEY_LEFT_SHIFT ) || inputHandler.getState( KEY_RIGHT_SHIFT );

		// zoom in and out
		if ( inputHandler.getState( KEY_EQUALS ) ) { scale /= shift ? 0.9f : 0.99f; }
		if ( inputHandler.getState( KEY_MINUS ) ) { scale *= shift ? 0.9f : 0.99f; }

		// vim-style x,y offsetting
		if ( inputHandler.getState( KEY_H ) ) { viewOffset.x += shift ? 5.0f : 1.0f; }
		if ( inputHandler.getState( KEY_J ) ) { viewOffset.y -= shift ? 5.0f : 1.0f; }
		if ( inputHandler.getState( KEY_K ) ) { viewOffset.y += shift ? 5.0f : 1.0f; }
		if ( inputHandler.getState( KEY_L ) ) { viewOffset.x -= shift ? 5.0f : 1.0f; }

		if ( inputHandler.getState( KEY_T ) ) { BlockCapture(); }
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
			GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			static rngi noiseOffset = rngi( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

			const glm::mat3 inverseBasisMat = inverse( glm::mat3( -trident.basisX, -trident.basisY, -trident.basisZ ) );
			glUniformMatrix3fv( glGetUniformLocation( shader, "invBasis" ), 1, false, glm::value_ptr( inverseBasisMat ) );

			glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
			glUniform2f( glGetUniformLocation( shader, "viewOffset" ), viewOffset.x, viewOffset.y );

			textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "0" : "1" ), "bufferTexture", shader, 3 );

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
		// compute dispatch for every voxel
		const GLuint shader = shaders[ "Update" ];
		glUseProgram( shader );

		for ( int i = 0; i < 20; i++ ) {
			static rngi wangSeeder( 1, 4000000000 );
			glUniform1ui( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );

			glUniform1i( glGetUniformLocation( shader, "resetFlag" ), 0 );
			textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "0" : "1" ), "bufferTexture", shader, 2 );
			textureManager.BindImageForShader( string( "Buffer " ) + string( swap ? "1" : "0" ), "bufferTexture", shader, 3 );
			swap = !swap;

			glDispatchCompute( ( bufferDims.x + 7 ) / 8, ( bufferDims.y + 7 ) / 8, ( bufferDims.z + 7 ) / 8 );
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

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal (active check happens inside)
		terminal.update( inputHandler );

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
	GPUSpherePack engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
