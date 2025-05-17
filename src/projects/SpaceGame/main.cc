#include "../../engine/engine.h"

#include "game.h"

class SpaceGame final : public engineBase {
public:
	SpaceGame () { config.forceResolution = ivec2( 720, 480 ); Init(); OnInit(); PostInit(); }
	~SpaceGame () { Quit(); }

	universeController controller;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );
			tonemap.tonemapMode = 0;

			CompileShaders();

			// load up the existing ship textures
			textureOptions_t opts;
			opts.dataType = GL_RGBA8;
			opts.textureType = GL_TEXTURE_2D;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.borderColor = { 0.0f, 0.0f, 0.0f, 0.0f };

			Image_4U ship1( "../src/projects/SpaceGame/ship1.png" );
			opts.width = ship1.Width();
			opts.height = ship1.Height();
			opts.initialData = ship1.GetImageDataBasePtr();
			textureManager.Add( "Ship1", opts );
			// pass the handles also to some sprite management layer...

			Image_4U ship2( "../src/projects/SpaceGame/ship2.png" );
			opts.width = ship2.Width();
			opts.height = ship2.Height();
			opts.initialData = ship2.GetImageDataBasePtr();
			textureManager.Add( "Ship2", opts );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		if ( inputHandler.getState4( KEY_Y ) == KEYSTATE_RISING ) {
			CompileShaders();
			logHighPriority( "Shaders Recompiled" );
		}
	}

	void CompileShaders () {
		shaders[ "Background Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/draw.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Background Draw" ], -1, string( "Background Draw" ).c_str() );

		shaders[ "Sprite Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/sprite.cs.glsl" ).shaderHandle; // eventually atlas the textures etc to facilitate batching... not super important right now
		glObjectLabel( GL_PROGRAM, shaders[ "Sprite Draw" ], -1, string( "Sprite Draw" ).c_str() );

		// similar structure to text renderer... drawing to an intermediate buffer... then blending with the contents of the buffer
			// shaders[ "Line Draw" ] = computeShader().shaderHandle; // todo
			// shaders[ "Line Draw Composite" ] = computeShader().shaderHandle; // todo
	}

	void ImguiPass () {
		ZoneScoped;

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

		{
			scopedTimer Start( "Background Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Background Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			static rngi noiseOffset( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

			vec2 v = controller.ship.GetVelocityVector();
			glUniform2f( glGetUniformLocation( shader, "velocityVector" ), v.x, v.y );

			vec2 p = controller.ship.GetPositionVector();
			glUniform2f( glGetUniformLocation( shader, "positionVector" ), p.x, p.y );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{
			scopedTimer Start( "Sprite Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Sprite Draw" ];
			glUseProgram( shader );

			vec2 v = controller.ship.GetVelocityVector();
			textureManager.BindTexForShader( "Ship1", "selectedTexture", shader, 2 );
			glUniform1f( glGetUniformLocation( shader, "angle" ), atan2( v.x, v.y ) );
			glUniform1f( glGetUniformLocation( shader, "scale" ), 1.0f / controller.ship.stats.size );
			glUniform2f( glGetUniformLocation( shader, "offset" ), v.x, v.y );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			/*
			textureManager.BindTexForShader( "Ship2", "selectedTexture", shader, 2 );
			glUniform1f( glGetUniformLocation( shader, "angle" ), 0.001f * SDL_GetTicks() );
			glUniform1f( glGetUniformLocation( shader, "scale" ), 1.0f / 0.2f );
			glUniform2f( glGetUniformLocation( shader, "offset" ), 2.0f, 2.0f );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			*/
		}

		if ( 0 ) {
			scopedTimer Start( "Line Drawing" );
			GLuint shader = shaders[ "Line Draw" ];
			glUseProgram( shader );

			// dispatch over a set of lines

			shader = shaders[ "Line Draw Composite" ];
			glUseProgram( shader );

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

		{
			scopedTimer Start( "Text Rendering" );

			DrawMenuBasics( textRenderer, textureManager );

			// show terminal, if active - check happens inside
			textRenderer.Clear();
			// textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.drawTerminal( terminal );
			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
		}

		{ // show trident with current orientation
			// scopedTimer Start( "Trident" );
			// trident.Update( textureManager.Get( "Display Texture" ) );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
		controller.ship.Update( inputHandler );
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
	SpaceGame engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
