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

			// force nearest filtering on the display texture, regardless of config setting
			textureManager.SetFilterMinMag( "Display Texture", GL_NEAREST, GL_NEAREST );

			// load up the existing ship textures
			textureOptions_t opts;
			opts.dataType = GL_RGBA8;
			opts.textureType = GL_TEXTURE_2D;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.borderColor = { 0.0f, 0.0f, 0.0f, 0.0f };

		// this stuff will move onto the universeController as the base ship models
			{
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

			// and the tiny font table (partial coverage: 3x3 alphas, 3x7 numerals... 3x5 punctuation, mostly, basically vertically centered - spaces between letters need to be added manually, so glyphs need to be 4 pixels wide in practice)
			Image_4U tinyFont( "../src/utils/fonts/fontRenderer/tinyFontPartial.png" );
			opts.width = tinyFont.Width();
			opts.height = tinyFont.Height();
			opts.initialData = tinyFont.GetImageDataBasePtr();
			opts.minFilter = GL_NEAREST;
			opts.magFilter = GL_NEAREST;
			opts.dataType = GL_RGBA8UI;
			textureManager.Add( "TinyFont", opts );
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

		shaders[ "BVH Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/bvh.cs.glsl" ).shaderHandle; // eventually atlas the textures etc to facilitate batching... not super important right now
		glObjectLabel( GL_PROGRAM, shaders[ "BVH Draw" ], -1, string( "BVH Draw" ).c_str() );

		// similar structure to the existing text renderer... drawing to an intermediate buffer... then blending with the contents of the buffer, in passes
		/*
		shaders[ "Text Draw" ] = computeShader().shaderHandle;
		shaders[ "Line Draw" ] = computeShader().shaderHandle;
		shaders[ "Line Draw Composite" ] = computeShader().shaderHandle; // todo
		*/
	}

	void ImguiPass () {
		ZoneScoped;

		TonemapControlsWindow();

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
		// rendering objects in the sector via the BVH, instead of sprites
			// need atlas texture
			// need atlas texture index SSBO (4 values per id -> [2D basePoint, 2D textureSize])
			// need the BVH nodes
			// need the BVH triangles
			// need the BVH payload (points, texcoords with texture ID for atlas SSBO map in the z coord)
			// need the accumulator texture, to write the result

		// having this BVH is actually very nice, because it means that I can have this model of all objects in the sector available on CPU and GPU
			// low geometric complexity of the ship bounding boxes means that this will scale to large object counts easily, even having to be updated every frame
			// occlusion solution via symmetric z scaling of the bounding boxes... smaller ships are taller bboxes, so viewed from above, they are encountered first, closer
				// this is used to place larger ships and stations beneath smaller ships, very similar to a simple 2D primitive draw order
				// symmetric scaling of the bounding boxes means that everything exists in the z=0 plane... you can do 2D logic there
			// this pass only uses it for rendering... but gameplay can use it for any sort of AI usage, simulated sensors, weapon guidance and collision
		}

		if ( 0 ) {
			scopedTimer Start( "Line And Text Drawing" );
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

			// DrawBlockMenu( "Test", textRenderer, textureManager );

			DrawInfoLog( textRenderer, textureManager );

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
