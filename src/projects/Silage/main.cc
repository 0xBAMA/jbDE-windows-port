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
	// the set for the grass
	GLuint cwbvhNodesDataBuffer_grass;
	GLuint cwbvhTrisDataBuffer_grass;

	// vertex data for the individual triangles, in a usable format
	GLuint triangleData;
	// and for the grass (also includes additional data)
	GLuint triangleData_grass;

	// view parameters
	float scale = 3.0f;
	vec2 thetaPhi_lightDirection = vec2( 0.0f, 0.0f );
	vec2 uvOffset = vec2( 0.0f );

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
			// I want to do something to remove abnormally brighter pixels... this is relevant for high iteration counts on the erosion, for some reason
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

			auto boundsCheck = [] ( tinybvh::bvhvec4 A, tinybvh::bvhvec4 B, tinybvh::bvhvec4 C ) -> bool {
				const vec3 center = vec3( 0.0f );
				const float radiusWithPad = 1.01f; // 1% pad

				return	distance( vec3( A.x, A.y, A.z ), center ) < radiusWithPad &&
						distance( vec3( B.x, B.y, B.z ), center ) < radiusWithPad &&
						distance( vec3( C.x, C.y, C.z ), center ) < radiusWithPad;
			};

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

					tinybvh::bvhvec4 A = tinybvh::bvhvec4( xPositions.x, yPositions.x, heightValues.x, heightValue );
					tinybvh::bvhvec4 B = tinybvh::bvhvec4( xPositions.y, yPositions.y, heightValues.y, heightValue );
					tinybvh::bvhvec4 C = tinybvh::bvhvec4( xPositions.z, yPositions.z, heightValues.z, heightValue );
					tinybvh::bvhvec4 D = tinybvh::bvhvec4( xPositions.w, yPositions.w, heightValues.w, heightValue );

					// ADC
					if ( boundsCheck( A, D, C ) ) {
						terrainTriangles.push_back( A );
						terrainTriangles.push_back( D );
						terrainTriangles.push_back( C );
					}

					// ABD
					if ( boundsCheck( A, B, D ) ) {
						terrainTriangles.push_back( A );
						terrainTriangles.push_back( B );
						terrainTriangles.push_back( D );
					}
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

					int steps = terrainBVH.Intersect( ray );
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

		// creating the same set of buffers for the grass

			glGenBuffers( 1, &cwbvhNodesDataBuffer_grass );
			glGenBuffers( 1, &cwbvhTrisDataBuffer_grass );
			glGenBuffers( 1, &triangleData_grass );

			// choosing grass locations
			std::vector< tinybvh::bvhvec4 > grassTriangles;

			rng pick = rng( -1.0f, 1.0f );
			rng adjust = rng( 0.8f, 1.618f );
			rng palettePick = rng( 0.0f, 1.0f );
			rng clip = rng( 0.0f, 0.5f );
			float boxSize = 0.001f;
			float zMultiplier = 20.0f;
			PerlinNoise per;

			palette::PickRandomPalette( true );

			// effectively just rejection sampling
			while ( ( grassTriangles.size() / 3 ) < 2000000 ) {

				// shooting a ray from above
				tinybvh::bvhvec3 O( pick(), pick(), 3.0f );
				tinybvh::bvhvec3 D( 0.0f, 0.0f, -1.0f );
				tinybvh::Ray ray( O, D );

				// if ( ( per.noise( O.x * 10.0f, O.y * 10.0f, 0.0f ) ) > clip() ) continue;
				float noiseRead = per.noise( O.x * 10.0f, O.y * 10.0f, 0.0f ) - clip();
				if ( noiseRead < 0.0f ) continue;

				int steps = terrainBVH.Intersect( ray );

				glm::quat rot = glm::angleAxis( 3.14f * pick(), vec3( 0.0f, 0.0f, 1.0f ) ); // basisX is the axis, therefore remains untransformed

				// good hit on terrain, and it is inside the snowglobe
				if ( ray.hit.t < BVH_FAR && distance( vec3( 0.0f ), vec3( O.x, O.y, 3.0f ) + ray.hit.t * vec3( 0.0f, 0.0f, -1.0f ) ) < 1.0f ) {
					float zMul = zMultiplier * adjust() * noiseRead;
					vec3 offset0 = ( rot * vec4( boxSize, 0.0f, zMul * boxSize, 0.0f ) ).xyz();
					vec3 offset1 = ( rot * vec4( -boxSize, boxSize, 0.0f, 0.0f ) ).xyz();
					vec3 offset2 = ( rot * vec4( 0.0f, -boxSize, -zMul * boxSize, 0.0f ) ).xyz();
					vec3 color = palette::paletteRef( palettePick() );

					grassTriangles.push_back( tinybvh::bvhvec4( O.x + offset0.x, O.y + offset0.y, 3.0f - ray.hit.t + offset0.z, color.x ) );
					grassTriangles.push_back( tinybvh::bvhvec4( O.x + offset1.x, O.y + offset1.y, 3.0f - ray.hit.t + offset1.z, color.y ) );
					grassTriangles.push_back( tinybvh::bvhvec4( O.x + offset2.x, O.y + offset2.y, 3.0f - ray.hit.t + offset2.z, color.z ) );
				}
			}

			grassBVH.BuildHQ( &grassTriangles[ 0 ], grassTriangles.size() / 3 );

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhNodesDataBuffer_grass );
			glBufferData( GL_SHADER_STORAGE_BUFFER, grassBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) grassBVH.bvh8Data, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, cwbvhNodesDataBuffer_grass );
			glObjectLabel( GL_BUFFER, cwbvhNodesDataBuffer_grass, -1, string( "Grass CWBVH Node Data" ).c_str() );
			cout << "Grass CWBVH8 Node Data is " << GetWithThousandsSeparator( grassBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, cwbvhTrisDataBuffer_grass );
			glBufferData( GL_SHADER_STORAGE_BUFFER, grassBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) grassBVH.bvh8Tris, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, cwbvhTrisDataBuffer_grass );
			glObjectLabel( GL_BUFFER, cwbvhTrisDataBuffer_grass, -1, string( "Grass CWBVH Tri Data" ).c_str() );
			cout << "Grass CWBVH8 Triangle Data is " << GetWithThousandsSeparator( grassBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, triangleData_grass );
			glBufferData( GL_SHADER_STORAGE_BUFFER, grassTriangles.size() * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) &grassTriangles[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 5, triangleData_grass );
			glObjectLabel( GL_BUFFER, triangleData_grass, -1, string( "Actual Grass Triangle Data" ).c_str() );
			cout << "Grass Triangle Test Data is " << GetWithThousandsSeparator( grassTriangles.size() * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;
		}
	}

	void CompileShaders () {
		shaders[ "Draw" ] = computeShader( "../src/projects/Silage/shaders/draw.cs.glsl" ).shaderHandle;
	vec3 GetLightDirection () {
		vec3 dir = vec3( 1.0f, 0.0f, 0.0f );
		dir = glm::rotate( dir, thetaPhi_lightDirection.y, glm::vec3( 0.0f, 1.0f, 0.0f ) );
		dir = glm::rotate( dir, thetaPhi_lightDirection.x, glm::vec3( 0.0f, 0.0f, 1.0f ) );
		return dir;
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		if ( inputHandler.getState4( KEY_Y ) == KEYSTATE_RISING ) {
			CompileShaders();
		}

		// TODO: setup inputHandler_t interface to something more like this
		if ( !ImGui::GetIO().WantCaptureMouse ) {
			ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
			ImGui::ResetMouseDragDelta();
			uvOffset -= vec2( currentMouseDrag.x, currentMouseDrag.y );
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
				float scaleCache = scale;
				scale = std::clamp( scale - event.wheel.y * ( ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 0.07f : 0.01f ), 0.005f, 5.0f );
				uvOffset = uvOffset * ( scaleCache / scale ); // adjust offset by the ratio, keeps center intact
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

		ImGui::Begin( "Controls" );
		ImGui::SliderFloat( "Theta", &thetaPhi_lightDirection.x, -pi, pi );
		ImGui::SliderFloat( "Phi", &thetaPhi_lightDirection.y, -pi / 2.0f, pi / 2.0f );
		ImGui::End();
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
			glUniform2i( glGetUniformLocation( shader, "uvOffset" ), uvOffset.x, uvOffset.y );
			glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
			vec3 lightDirection = GetLightDirection();
			glUniform3f( glGetUniformLocation( shader, "lightDirection" ), lightDirection.x, lightDirection.y, lightDirection.z );

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
