#include "../../engine/engine.h"

class Verdure final : public engineBase { // sample derived from base engine class
public:
	Verdure () { Init(); OnInit(); PostInit(); }
	~Verdure () { Quit(); }

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

	// generator parameters
	float spherePadPercent = 3.0f;

	// view parameters
	float scale = 3.0f;
	float blendAmount = 0.75f;
	vec2 uvOffset = vec2( 0.0f );
	float DoFDistance = 2.0f;
	float DoFRadius = 10.0f;
	bool screenshotRequested = false;

	// parameters for 3 lights
	vec2 thetaPhi_lightDirection[ 3 ] = { vec2( 0.0f ) };
	float lightJitter[ 3 ] = { 0.0f };
	vec3 lightColors[ 3 ] = { vec3( 1.0f ) };
	float lightBrightness[ 3 ] = { 1.0f };
	ivec3 lightEnable = ivec3( 1, 0, 0 );

	// the running deque of jittered light positions
	std::deque< vec3 >lightDirectionQueue[ 3 ];

	// parameters for the palette and generator
	int selectedPalette = 0;
	float paletteMin = 0.0f;
	float paletteMax = 1.0f;
	float maxDisplacement = 0.01f;
	int maxGrassBlades = 1000000;
	float heightmapHeightScalar = 1.0f;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			CompileShaders();

		// buffer for the triangles and nodes data on the GPU
			glGenBuffers( 1, &cwbvhNodesDataBuffer );
			glGenBuffers( 1, &cwbvhTrisDataBuffer );
			glGenBuffers( 1, &triangleData );

		// creating the same set of buffers for the grass
			glGenBuffers( 1, &cwbvhNodesDataBuffer_grass );
			glGenBuffers( 1, &cwbvhTrisDataBuffer_grass );
			glGenBuffers( 1, &triangleData_grass );

			// picking an initial palette
			palette::PickRandomPalette( true );
			selectedPalette = palette::PaletteIndex;

			// generate the ground, grass, and buffer it to the GPU
			GenerateLandscape();

			// initial pump of the light direction queue
			for ( int i = 0; i < 16; i++ ) {
				PushLightDirections();
			}
		}
	}

	void CompileShaders () {
		shaders[ "Draw" ] = computeShader( "../src/projects/Verdure/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "Grass" ] = computeShader( "../src/projects/Verdure/shaders/grass.cs.glsl" ).shaderHandle;
	}

	void GenerateLandscape () {
		Tick();
		uint32_t size = 1024;

		particleEroder p;
		p.InitWithDiamondSquare( size );
		//p.InitWithPerlin( size ); // this sucks, needs work

		// probably copy original model image here, so we can compute height deltas, determine areas where sediment would collect
		Image_1F modelCache( p.model );

		const int numSteps = 20;
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

		auto boundsCheck = [=] ( tinybvh::bvhvec4 A, tinybvh::bvhvec4 B, tinybvh::bvhvec4 C ) -> bool {
			const vec3 center = vec3( 0.0f );
			const float radiusWithPad = 1.0f + 0.01f * spherePadPercent; // padding to avoid edge issues

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
				) / heightmapHeightScalar;

				vec4 heightValuesPreErode = vec4(
					modelCache.GetAtXY( x, y )[ red ], // A
					modelCache.GetAtXY( x + 1, y )[ red ], // B
					modelCache.GetAtXY( x, y + 1 )[ red ], // C
					modelCache.GetAtXY( x + 1, y + 1 )[ red ]  // D
				) / heightmapHeightScalar;

				heightDeltas.push_back( ( ( heightValues.r + heightValues.g + heightValues.b + heightValues.a ) / 4.0f ) -
					( ( heightValuesPreErode.r + heightValuesPreErode.g + heightValuesPreErode.b + heightValuesPreErode.a ) / 4.0f ) );

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

				tinybvh::bvhvec4 A = tinybvh::bvhvec4( xPositions.x, yPositions.x, heightValues.x, 0.0f );
				tinybvh::bvhvec4 B = tinybvh::bvhvec4( xPositions.y, yPositions.y, heightValues.y, 0.0f );
				tinybvh::bvhvec4 C = tinybvh::bvhvec4( xPositions.z, yPositions.z, heightValues.z, 0.0f );
				tinybvh::bvhvec4 D = tinybvh::bvhvec4( xPositions.w, yPositions.w, heightValues.w, 0.0f );

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

		// build the BVH from the triangle list
		terrainBVH.BuildHQ( &terrainTriangles[ 0 ], terrainTriangles.size() / 3 );

		/*
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
		*/

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

		// choosing grass locations
		std::vector< tinybvh::bvhvec4 > grassTrianglesBVH;
		std::vector< tinybvh::bvhvec4 > grassTriangles; // including a fourth value, with the base point from which to displace

		rng pick = rng( -1.0f, 1.0f );
		rng adjust = rng( 0.8f, 2.618f );
		rng palettePick = rng( paletteMin, paletteMax );
		rng clip = rng( 0.0f, 0.5f );
		float boxSize = 0.001f;
		float zMultiplier = 20.0f;
		PerlinNoise per;

		palette::PaletteIndex = selectedPalette;

		// effectively just rejection sampling
		while ( ( grassTrianglesBVH.size() / 3 ) < maxGrassBlades ) {

			// shooting a ray from above
			tinybvh::bvhvec3 O( pick(), pick(), 3.0f );
			tinybvh::bvhvec3 D( 0.0f, 0.0f, -1.0f );
			tinybvh::Ray ray( O, D );

			// if ( ( per.noise( O.x * 10.0f, O.y * 10.0f, 0.0f ) ) > clip() ) continue;
			float noiseRead = per.noise( O.x * 10.0f, O.y * 10.0f, 0.0f ) * per.noise( O.x * 33.0f, O.y * 33.0f, 0.4f ) + clip();
			// float noiseRead = 1.0f;
			if ( noiseRead < 0.01f ) continue;

			int steps = terrainBVH.Intersect( ray );

			glm::quat rot = glm::angleAxis( 3.14f * pick(), vec3( 0.0f, 0.0f, 1.0f ) );

			// good hit on terrain, and it is inside the snowglobe
			if ( ray.hit.t < BVH_FAR && distance( vec3( 0.0f ), vec3( O.x, O.y, 3.0f ) + ray.hit.t * vec3( 0.0f, 0.0f, -1.0f ) ) < ( 1.0f + 0.01f * spherePadPercent ) ) {

				float zMul = zMultiplier * adjust() * noiseRead;
				vec3 offset0 = ( rot * vec4( boxSize, 0.0f, zMul * boxSize, 0.0f ) ).xyz();
				vec3 offset1 = ( rot * vec4( -boxSize, boxSize, 0.0f, 0.0f ) ).xyz();
				vec3 offset2 = ( rot * vec4( 0.0f, -boxSize, -zMul * boxSize, 0.0f ) ).xyz();
				vec3 color = palette::paletteRef( palettePick() );

				tinybvh::bvhvec4 v0 = tinybvh::bvhvec4( O.x + offset0.x, O.y + offset0.y, 3.0f - ray.hit.t + offset0.z, color.x );
				tinybvh::bvhvec4 v1 = tinybvh::bvhvec4( O.x + offset1.x, O.y + offset1.y, 3.0f - ray.hit.t + offset1.z, color.y );
				tinybvh::bvhvec4 v2 = tinybvh::bvhvec4( O.x + offset2.x, O.y + offset2.y, 3.0f - ray.hit.t + offset2.z, color.z );

				// these need to be placed to also include the displacement sphere
				{
					vec3 mins = vec3(  1000.0f );
					vec3 maxs = vec3( -1000.0f );

					// expand to also include the displacement sphere
					mins.x = min( min( v0.x, v1.x ), min( v2.x, v0.x - maxDisplacement ) );
					maxs.x = max( max( v0.x, v1.x ), max( v2.x, v0.x + maxDisplacement ) );

					mins.y = min( min( v0.y, v1.y ), min( v2.y, v0.y - maxDisplacement ) );
					maxs.y = max( max( v0.y, v1.y ), max( v2.y, v0.y + maxDisplacement ) );

					// mins.z = min( min( v0.z, v1.z ), min( v2.z, v0.z - maxDisplacement ) );
					// maxs.z = max( max( v0.z, v1.z ), max( v2.z, v0.z + maxDisplacement ) );
					mins.z = min( min( v0.z, v1.z ), v2.z );
					maxs.z = max( max( v0.z, v1.z ), v2.z );

				// place a triangle that spans the bounding box
					grassTrianglesBVH.push_back( tinybvh::bvhvec4( maxs.x, mins.y, maxs.z, 0.0f ) );
					grassTrianglesBVH.push_back( tinybvh::bvhvec4( mins.x, maxs.y, mins.z, 0.0f ) );
					grassTrianglesBVH.push_back( tinybvh::bvhvec4( maxs.x, mins.y, mins.z, 0.0f ) );
				}

				// this is the triangle... plus a cached base point, where noise is sampled + base for displacement
				grassTriangles.push_back( v0 );
				grassTriangles.push_back( v1 );
				grassTriangles.push_back( v2 );
				grassTriangles.push_back( v2 );
			}
		}

		grassBVH.BuildHQ( &grassTrianglesBVH[ 0 ], grassTrianglesBVH.size() / 3 );

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

		float msTaken = Tock();
		cout << "Operation Complete in " << msTaken / 1000.0f << " seconds" << endl;
	}

	void PushLightDirections () {
		// jitter allows for the resolving of soft shadows
		rngN jitter[ 3 ] = {
			rngN( 0.0f, lightJitter[ 0 ] ),
			rngN( 0.0f, lightJitter[ 1 ] ),
			rngN( 0.0f, lightJitter[ 2 ] )
		};
		vec3 dir[ 3 ];
		for ( int i = 0; i < 3; i++ ) {
			dir[ i ] = vec3( 1.0f, 0.0f, 0.0f );
			dir[ i ] = glm::rotate( dir[ i ], thetaPhi_lightDirection[ i ].y + jitter[ i ](), glm::vec3( 0.0f, 1.0f, 0.0f ) );
			dir[ i ] = glm::rotate( dir[ i ], thetaPhi_lightDirection[ i ].x + jitter[ i ](), glm::vec3( 0.0f, 0.0f, 1.0f ) );
			lightDirectionQueue[ i ].push_back( normalize( dir[ i ] ) );
		}
	}

	void Screenshot( string label, bool srgbConvert, bool fullDepth ) {
		const GLuint tex = textureManager.Get( label );
		uvec2 dims = textureManager.GetDimensions( label );
		std::vector< float > imageBytesToSave;
		imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
		glBindTexture( GL_TEXTURE_2D, tex );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
		Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
		if ( srgbConvert == true ) {
			screenshot.RGBtoSRGB();
		}
		screenshot.FlipVertical();
		const string filename = string( "Verdure-" ) + timeDateString() + string( fullDepth ? ".exr" : ".png" );
		screenshot.Save( filename, fullDepth ? Image_4F::backend::TINYEXR : Image_4F::backend::LODEPNG );
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
		ImGui::SeparatorText( "Lights" );
		ImGui::Text( "Key Light" );
		ImGui::SameLine();
		ImGui::Checkbox( "Enable##key", ( bool* ) &lightEnable.x );
		ImGui::SliderFloat( "Theta##key", &thetaPhi_lightDirection[ 0 ].x, -pi, pi );
		ImGui::SliderFloat( "Phi##key", &thetaPhi_lightDirection[ 0 ].y, -pi / 2.0f, pi / 2.0f );
		ImGui::SliderFloat( "Jitter##key", &lightJitter[ 0 ], 0.0f, 0.3f );
		ImGui::ColorEdit3( "Color##key", ( float* ) &lightColors[ 0 ], ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel );
		ImGui::SliderFloat( "Brightness##key", &lightBrightness[ 0 ], 0.0f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );

		ImGui::Text( " " );
		ImGui::Text( "Fill Light" );
		ImGui::SameLine();
		ImGui::Checkbox( "Enable##fill", ( bool* ) &lightEnable.y );
		ImGui::SliderFloat ( "Theta##fill", &thetaPhi_lightDirection[ 1 ].x, -pi, pi );
		ImGui::SliderFloat ( "Phi##fill", &thetaPhi_lightDirection[ 1 ].y, -pi / 2.0f, pi / 2.0f );
		ImGui::SliderFloat ( "Jitter##fill", &lightJitter[ 1 ], 0.0f, 0.3f );
		ImGui::ColorEdit3( "Color##fill", ( float* ) &lightColors[ 1 ], ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel );
		ImGui::SliderFloat( "Brightness##fill", &lightBrightness[ 1 ], 0.0f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );

		ImGui::Text( " " );
		ImGui::Text( "Back Light" );
		ImGui::SameLine();
		ImGui::Checkbox( "Enable##back", ( bool* ) &lightEnable.z );
		ImGui::SliderFloat ( "Theta##back", &thetaPhi_lightDirection[ 2 ].x, -pi, pi );
		ImGui::SliderFloat ( "Phi##back", &thetaPhi_lightDirection[ 2 ].y, -pi / 2.0f, pi / 2.0f );
		ImGui::SliderFloat ( "Jitter##back", &lightJitter[ 2 ], 0.0f, 0.3f );
		ImGui::ColorEdit3( "Color##back", ( float* ) &lightColors[ 2 ], ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel );
		ImGui::SliderFloat( "Brightness##back", &lightBrightness[ 2 ], 0.0f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );

		ImGui::Text( " " );
		ImGui::SeparatorText( "Frame Parameters" );
		if ( ImGui::Button( "Capture" ) ) {
			screenshotRequested = true;
		}
		ImGui::SliderFloat( "Blend Amount", &blendAmount, 0.75f, 0.99f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderFloat( "Thin Lens Focus Distance", &DoFDistance, 0.1f, 6.0f, "%.5f" );
		ImGui::SliderFloat( "Thin Lens Defocus Amount", &DoFRadius, 0.1f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );

		// regen model dialog
		ImGui::Text( " " );
		ImGui::SeparatorText( "Generator" );
		if ( ImGui::Button( "Regen" ) ) {
			GenerateLandscape();
		}
		ImGui::Separator();
		ImGui::Text( " " );
		{
			ImGui::SliderFloat( "Min", &paletteMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Max", &paletteMax, 0.0f, 1.0f );

			static std::vector< const char* > paletteLabels;
			if ( paletteLabels.size() == 0 ) {
				for ( auto& entry : palette::paletteListLocal ) {
					// copy to a cstr for use by imgui
					char* d = new char[ entry.label.length() + 1 ];
					std::copy( entry.label.begin(), entry.label.end(), d );
					d[ entry.label.length() ] = '\0';
					paletteLabels.push_back( d );
				}
			}

			ImGui::Combo( ( string( "Palette## " ) ).c_str(), &selectedPalette, paletteLabels.data(), paletteLabels.size() );
			bool isUpdated = ImGui::IsItemEdited();

			ImGui::SameLine();
			if ( ImGui::Button( "Random" ) ) {
				palette::PickRandomPalette( true );
				selectedPalette = palette::PaletteIndex;
			}

			const size_t paletteSize = palette::paletteListLocal[ selectedPalette ].colors.size();
			ImGui::Text( "  Contains %.3lu colors:", palette::paletteListLocal[ palette::PaletteIndex ].colors.size() );
			// handle max < min
			float minVal = paletteMin;
			float maxVal = paletteMax;
			float realSelectedMin = std::min( minVal, maxVal );
			float realSelectedMax = std::max( minVal, maxVal );
			size_t minShownIdx = std::floor( realSelectedMin * ( paletteSize - 1 ) );
			size_t maxShownIdx = std::ceil( realSelectedMax * ( paletteSize - 1 ) );

			bool finished = false;
			for ( int y = 0; y < 8; y++ ) {
				if ( !finished ) {
					ImGui::Text( " " );
				}
				for ( int x = 0; x < 32; x++ ) {
					// terminate when you run out of colors
					const uint32_t index = x + 32 * y;
					if ( index >= paletteSize ) {
						finished = true;
						// goto terminate;
					}
					// show color, or black if past the end of the list
					ivec4 color = ivec4( 0 );
					if ( !finished ) {
						color = ivec4( palette::paletteListLocal[ selectedPalette ].colors[ index ], 255 );
						// determine if it is in the active range
						if ( index < minShownIdx || index > maxShownIdx ) {
							color.a = 64; // dim inactive entries
						}
					}
					if ( color.a != 0 ) {
						ImGui::SameLine();
						ImGui::TextColored( ImVec4( color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f ), "@" );
					}
				}
			}
		}

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
			glUniform1f( glGetUniformLocation( shader, "blendAmount" ), blendAmount );
			glUniform1f( glGetUniformLocation( shader, "DoFRadius" ), DoFRadius );
			glUniform1f( glGetUniformLocation( shader, "DoFDistance" ), DoFDistance );
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
			
			// get a new light direction in the list... get rid of the last one
			PushLightDirections();
			lightDirectionQueue[ 0 ].pop_front();
			lightDirectionQueue[ 1 ].pop_front();
			lightDirectionQueue[ 2 ].pop_front();

			// construct a vector of the data to send to GPU
			vec3 lightDirections0[ 16 ];
			vec3 lightDirections1[ 16 ];
			vec3 lightDirections2[ 16 ];
			for ( int i = 0; i < 16; i++ ) {
				lightDirections0[ i ] = lightDirectionQueue[ 0 ][ i ];
				lightDirections1[ i ] = lightDirectionQueue[ 1 ][ i ];
				lightDirections2[ i ] = lightDirectionQueue[ 2 ][ i ];
			}

			// Light enable flags
			glUniform3iv( glGetUniformLocation( shader, "lightEnable" ), 1, ( const GLint* ) &lightEnable );

			// Key Light
			glUniform3fv( glGetUniformLocation( shader, "lightDirections0" ), 16, glm::value_ptr( lightDirections0[ 0 ] ) );
			glUniform4f( glGetUniformLocation( shader, "lightColor0" ), lightColors[ 0 ].x, lightColors[ 0 ].y, lightColors[ 0 ].z, lightBrightness[ 0 ] );

			// Fill Light
			glUniform3fv( glGetUniformLocation( shader, "lightDirections1" ), 16, glm::value_ptr( lightDirections1[ 0 ] ) );
			glUniform4f( glGetUniformLocation( shader, "lightColor1" ), lightColors[ 1 ].x, lightColors[ 1 ].y, lightColors[ 1 ].z, lightBrightness[ 1 ] );

			// Back Light
			glUniform3fv( glGetUniformLocation( shader, "lightDirections2" ), 16, glm::value_ptr( lightDirections2[ 0 ] ) );
			glUniform4f( glGetUniformLocation( shader, "lightColor2" ), lightColors[ 2 ].x, lightColors[ 2 ].y, lightColors[ 2 ].z, lightBrightness[ 2 ] );

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

		if ( screenshotRequested ) {
			screenshotRequested = false;
			Screenshot( "Display Texture", true, false );
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

		// run the grass update shader for maxGrassBlades
		const GLuint shader = shaders[ "Grass" ];
		glUseProgram( shader );

		static rngi noiseOffset = rngi( 0, 512 );
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

		const float time = SDL_GetTicks() / 100.0f;
		glUniform3f( glGetUniformLocation( shader, "noiseOffset0" ), 0.0f, 0.0f, time / 10.0f );
		glUniform3f( glGetUniformLocation( shader, "noiseOffset1" ), 0.0f, time / 9.0f, 0.0f );
		glUniform3f( glGetUniformLocation( shader, "noiseOffset2" ), time / 13.0f, 0.0f, 0.0f );
		glUniform3f( glGetUniformLocation( shader, "noiseScalars" ), 1.5f, 2.5f, 3.5f );
		glUniform3f( glGetUniformLocation( shader, "displacementScalars" ), maxDisplacement, maxDisplacement, 0.0f );

		glDispatchCompute( 64, ( maxGrassBlades + 63 ) / 64, 1 );
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
	Verdure engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
