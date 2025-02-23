#include "../../engine/engine.h"

class engineDemo final : public engineBase { // sample derived from base engine class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	// application data
	tinybvh::BVH bvh;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Dummy Draw" ] = computeShader( "../src/projects/Silage/shaders/draw.cs.glsl" ).shaderHandle;

			uint32_t size = 1024;

			particleEroder p;
			p.InitWithDiamondSquare( size );
			for ( int i = 0; i < 20; i++ )
				p.Erode( 5000 ), cout << "step " << i << " / 20" << endl;
			p.model.Autonormalize();

			// build the triangle list
			std::vector< tinybvh::bvhvec4 > triangles;

			for ( uint32_t y = 0; y < p.model.Width() - 1; y++ ) {
				for ( uint32_t x = 0; x < p.model.Height() - 1; x++ ) {
					// four corners of a square
					/*	C	@=======@ D
							|      /|
							|     / |
							|    /  |
							|   /   |
							|  /    |
							| /     |
							|/      |
						A	@=======@ B --> X */
					vec4 heightValues = vec4(
						p.model.GetAtXY(     x,     y )[ red ], // A
						p.model.GetAtXY( x + 1,     y )[ red ], // B
						p.model.GetAtXY(     x, y + 1 )[ red ], // C
						p.model.GetAtXY( x + 1, y + 1 )[ red ]  // D
					);

					vec4 xPositions = vec4(
						RangeRemap( x, 0, p.model.Width(), -1.0f, 1.0f ),
						RangeRemap( x + 1, 0, p.model.Width(), -1.0f, 1.0f ),
						RangeRemap( x, 0, p.model.Width(), -1.0f, 1.0f ),
						RangeRemap( x + 1, 0, p.model.Width(), -1.0f, 1.0f )
					);

					vec4 yPositions = vec4(
						RangeRemap( y, 0, p.model.Height(), -1.0f, 1.0f ),
						RangeRemap( y, 0, p.model.Height(), -1.0f, 1.0f ),
						RangeRemap( y + 1, 0, p.model.Height(), -1.0f, 1.0f ),
						RangeRemap( y + 1, 0, p.model.Height(), -1.0f, 1.0f )
					);

					// ADC
					triangles.push_back( tinybvh::bvhvec4( xPositions.x, yPositions.x, heightValues.x, 0.0f ) );
					triangles.push_back( tinybvh::bvhvec4( xPositions.w, yPositions.w, heightValues.w, 0.0f ) );
					triangles.push_back( tinybvh::bvhvec4( xPositions.z, yPositions.z, heightValues.z, 0.0f ) );

					// ABD
					triangles.push_back( tinybvh::bvhvec4( xPositions.x, yPositions.x, heightValues.x, 0.0f ) );
					triangles.push_back( tinybvh::bvhvec4( xPositions.y, yPositions.y, heightValues.y, 0.0f ) );
					triangles.push_back( tinybvh::bvhvec4( xPositions.w, yPositions.w, heightValues.w, 0.0f ) );
				}
			}

			// consider adding skirts... or maybe just reject backface hits

			// build the BVH from the triangle list
			bvh.Build( &triangles[ 0 ], triangles.size() / 3 );

			// test some rays
			Image_4F output( 640, 360 );
			int maxSteps = 0;
			for ( int x = 0; x < output.Width(); x++ ) {
				for ( int y = 0; y < output.Height(); y++ ) {
					vec2 uv = vec2(
						4.0f * RangeRemap( x, 0.0f, output.Width(), -0.64f, 0.64f ),
						4.0f * RangeRemap( y, 0.0f, output.Height(), -0.36f, 0.36f )
					);

					vec3 origin = ( glm::angleAxis( 0.5f, vec3( 1.0f ) ) * vec4( uv, -3.0f, 0.0f ) ).xyz();
					vec3 direction = -( glm::angleAxis( 0.5f, vec3( 1.0f ) ) * vec4( 0.0f, 0.0f, -3.0f, 0.0f ) ).xyz();

					tinybvh::bvhvec3 O( origin.x, origin.y, origin.z );
					tinybvh::bvhvec3 D( direction.x, direction.y, direction.z );
					tinybvh::Ray ray( O, D );

					int steps = bvh.Intersect( ray );
					maxSteps = std::max( steps, maxSteps );
					// printf( "std: nearest intersection: %f (found in %i traversal steps).\n", ray.hit.t, steps );

					color_4F col;
					col[ red ] = 0.0f;
					col[ green ] = steps / 64.0f;
					col[ blue ] = steps / 64.0f;
					col[ alpha ] = 255.0f;

					if ( ray.hit.t < BVH_FAR ) {
						col[ red ] = exp( -0.03f * ray.hit.t );
					}
					output.SetAtXY( x, y, col );
				}
			}
			output.Save( "test.png" );
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

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
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

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
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
	engineDemo engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
