#include "../../engine/engine.h"

class engineDemo final : public engineBase { // sample derived from base engine class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	GLuint pointBuffer;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// =============================================================================================================
			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "../src/projects/CrystalViewer/shaders/draw.cs.glsl" ).shaderHandle;
			// shaders[ "PointSplat" ] = computeShader( "../src/projects/CrystalViewer/shaders/pointSplat.cs.glsl" ).shaderHandle;
			// =============================================================================================================

			// texture(s) to splat into
			{
				textureOptions_t opts;
				opts.width = 1280;
				opts.height = 720;
				opts.depth = 128;
				opts.dataType = GL_R32UI;
				opts.textureType = GL_TEXTURE_3D;
				textureManager.Add( "SplatBuffer", opts );
			}

			// buffer for the points
			{
				glCreateBuffers( 1, &pointBuffer );
				glBindBuffer( GL_SHADER_STORAGE_BUFFER, pointBuffer );

				// loading the data from disk... it's a linear array of mat4's, so let's go ahead and process it down to vec4's by transforming p0 by that mat4
				constexpr vec4 p0 = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
				std::vector< vec4 > crystalPoints;

				Image_4U matrixBuffer( "../Crystals/crystalModelTest.png" );
				const int numPoints = ( matrixBuffer.Height() - 1 ) * 1024; // 1024 mat4's per row, small crop for safety
				crystalPoints.resize( numPoints );

				mat4 *dataAsMat4s = ( mat4 * ) matrixBuffer.GetImageDataBasePtr();
				for ( int i = 0; i < numPoints; ++i ) {
					crystalPoints[ i ] = dataAsMat4s[ i ] * p0;
				}

				glBufferData( GL_SHADER_STORAGE_BUFFER, crystalPoints.size() * sizeof( vec4 ), crystalPoints.data(), GL_DYNAMIC_COPY );
				glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, pointBuffer );
			}

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

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code

		// we know if something happened and we need to redraw...
		// static int numDraws = 16;
		// if ( trident.Dirty() || ( numDraws > 0 ) ) {
			// we need to resplat points
			// numDraws--;

			if ( trident.Dirty() ) {
				textureManager.ZeroTexture3D( "SplatBuffer" );
				textureManager.ZeroTexture2D( "Accumulator" );
			}

			const GLuint shader = shaders[ "PointSplat" ];
			glUseProgram( shader );
			const int workgroupsRoundedUp = ( numPoints + 63 ) / 64;
			glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( trident.basisX ) );
			glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( trident.basisY ) );
			glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( trident.basisZ ) );
			static int n = numPoints;
			glUniform1i( glGetUniformLocation( shader, "n" ), n );

			textureManager.BindImageForShader( "SplatBuffer", "SplatBuffer", shader, 2 );
			glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
		// }
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

// #pragma comment( linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup" )

int main ( int argc, char *argv[] ) {
	engineDemo engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
