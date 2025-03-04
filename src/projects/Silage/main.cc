#include "../../engine/engine.h"

class engineDemo final : public engineBase { // sample derived from base engine class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	// application data
	tinybvh::BVH8_CWBVH terrainBVH;
	tinybvh::BVH8_CWBVH grassBVH;

	// node and triangle data specifically for the BVH
	GLuint cwbvhNodesDataBuffer;
	GLuint cwbvhTrisDataBuffer;

	// vertex data for the individual triangles, in a usable format
	GLuint triangleData;

	// view parameters
	float scale = 3.0f;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			CompileShaders();

			uint32_t size = 1024;

			particleEroder p;
			p.InitWithDiamondSquare( size );
			//p.InitWithPerlin( size ); // this sucks, needs work

			// probably copy original model image here, so we can compute height deltas, determine areas where sediment would collect
			Image_1F modelCache( p.model );

			const int numSteps = 10;
			for ( int i = 0; i < numSteps; i++ )
				p.Erode( 5000 ), cout << "\rstep " << i << " / " << numSteps;
			cout << "\rerosion step finished          " << endl;
			p.model.Autonormalize();

			/* variance clamping... something
			// I want to do something to remove abnormally brighter pixels...
				// if a pixel is much brighter than neighbors, take the average of the neigbors
			for ( uint32_t y = 0; y < p.model.Width(); y++ ) {
				for ( uint32_t x = 0; x < p.model.Height(); x++ ) {
					// grab pixel height

					// grab 8 samples of neighbor height
						// std deviation? need to detect edges and skip them because they will have bad data, 0's
					
					// if pixel height is significantly different than the average of the neighbors

				}
			}
			*/

			// build the triangle list
			std::vector< tinybvh::bvhvec4 > terrainTriangles;
			std::vector< float > heightDeltas;

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

					vec4 heightValuesPreErode = vec4(
						modelCache.GetAtXY( x, y )[ red ], // A
						modelCache.GetAtXY( x + 1, y )[ red ], // B
						modelCache.GetAtXY( x, y + 1 )[ red ], // C
						modelCache.GetAtXY( x + 1, y + 1 )[ red ]  // D
					);

					//heightDeltas.push_back( ( ( heightValues.r + heightValues.g + heightValues.b + heightValues.a ) / 4.0f ) -
					//	( ( heightValuesPreErode.r + heightValuesPreErode.g + heightValuesPreErode.b + heightValuesPreErode.a ) / 4.0f ) );

					float heightValue = ( heightValues.r + heightValues.g + heightValues.b + heightValues.a ) / 4.0f;
					heightDeltas.push_back( heightValue );

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
					terrainTriangles.push_back( tinybvh::bvhvec4( xPositions.x, yPositions.x, heightValues.x, heightValue ) );
					terrainTriangles.push_back( tinybvh::bvhvec4( xPositions.w, yPositions.w, heightValues.w, heightValue ) );
					terrainTriangles.push_back( tinybvh::bvhvec4( xPositions.z, yPositions.z, heightValues.z, heightValue ) );

					// ABD
					terrainTriangles.push_back( tinybvh::bvhvec4( xPositions.x, yPositions.x, heightValues.x, heightValue ) );
					terrainTriangles.push_back( tinybvh::bvhvec4( xPositions.y, yPositions.y, heightValues.y, heightValue ) );
					terrainTriangles.push_back( tinybvh::bvhvec4( xPositions.w, yPositions.w, heightValues.w, heightValue ) );
				}
			}

			// consider adding skirts... or maybe just reject backface hits

			// build the BVH from the triangle list
			terrainBVH.BuildHQ( &terrainTriangles[ 0 ], terrainTriangles.size() / 3 );

			// test some rays
			Image_4F output( 1920, 1080 );
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

					if ( ray.hit.t < BVH_FAR ) {
						float heightDelta = heightDeltas[ ray.hit.prim / 2 ];

						color_4F color;

						color[ red ] = color[ green ] = color[ blue ] = heightDelta;
						color[ alpha ] = 1.0f;

						output.SetAtXY( x, y, color );
					}
				}
			}
			output.Save( "test.png" );

		// buffer the triangles and nodes data to the GPU

			glGenBuffers( 1, &cwbvhNodesDataBuffer );
			glGenBuffers( 1, &cwbvhTrisDataBuffer );
			glGenBuffers( 1, &triangleData );

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhNodesDataBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, terrainBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ), ( GLvoid * ) terrainBVH.bvh8Data, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, cwbvhNodesDataBuffer );
			glObjectLabel( GL_BUFFER, cwbvhNodesDataBuffer, -1, string( "Terrain CWBVH Node Data" ).c_str() );
			cout << "Terrain CWBVH8 Node Data is " << GetWithThousandsSeparator( terrainBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhTrisDataBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, terrainBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ), ( GLvoid * ) terrainBVH.bvh8Tris, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, cwbvhTrisDataBuffer );
			glObjectLabel( GL_BUFFER, cwbvhTrisDataBuffer, -1, string( "Terrain CWBVH Tri Data" ).c_str() );
			cout << "Terrain CWBVH8 Triangle Data is " << GetWithThousandsSeparator( terrainBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, triangleData );
			glBufferData( GL_SHADER_STORAGE_BUFFER, terrainTriangles.size() * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) &terrainTriangles[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, triangleData );
			glObjectLabel( GL_BUFFER, triangleData, -1, string( "Actual Terrain Triangle Data" ).c_str() );
			cout << "Terrain Triangle Test Data is " << GetWithThousandsSeparator( terrainTriangles.size() * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;
		}
	}

	void CompileShaders () {
		shaders[ "Draw" ] = computeShader( "../src/projects/Silage/shaders/draw.cs.glsl" ).shaderHandle;
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		if ( inputHandler.getState4( KEY_Y ) == KEYSTATE_RISING ) {
			CompileShaders();
		}

		SDL_Event event;
		SDL_PumpEvents();
		while ( SDL_PollEvent( &event ) ) {
			ImGui_ImplSDL3_ProcessEvent( &event ); // imgui event handling
			pQuit = config.oneShot || // swap out the multiple if statements for a big chained boolean setting the value of pQuit
				( event.type == SDL_EVENT_QUIT ) ||
				( event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID( window.window ) ) ||
				( event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE && SDL_GetModState() & SDL_KMOD_SHIFT );
			if ( ( event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE ) || ( event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_X1 ) ) {
				quitConfirm = !quitConfirm; // this has to stay because it doesn't seem like ImGui::IsKeyReleased is stable enough to use
			}

			// handling scrolling
			if ( event.type == SDL_EVENT_MOUSE_WHEEL && !ImGui::GetIO().WantCaptureMouse ) {
				scale = std::clamp( scale - event.wheel.y * ( ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 0.07f : 0.01f ), 0.005f, 5.0f );
			}
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

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			const glm::mat3 inverseBasisMat = inverse( glm::mat3( -trident.basisX, -trident.basisY, -trident.basisZ ) );
			glUniformMatrix3fv( glGetUniformLocation( shader, "invBasis" ), 1, false, glm::value_ptr( inverseBasisMat ) );
			glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			static rngi noiseOffset = rngi( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			bindSets[ "Postprocessing" ].apply();
			const GLuint shader = shaders[ "Tonemap" ];
			glUseProgram( shader );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering timestamp - required texture binds/shader stuff is handled internally
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
