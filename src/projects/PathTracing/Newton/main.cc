#include "../../../engine/engine.h"
#include "shaders/lib/shaderWrapper.h"
#include "light.h"

class Newton final : public engineBase { // sample derived from base engine class
public:
	Newton () { Init(); OnInit(); PostInit(); }
	~Newton () { Quit(); }

	// list of lights
	static constexpr int maxLights = 1024;
	int numLights = 2;
	lightSpec lights[ maxLights ];
	vec4 visualizerColors[ maxLights ];

	// load model triangles
	SoftRast s;
	vec3 sceneMins = vec3( 1e30f );
	vec3 sceneMaxs = vec3( -1e30f );
	void sceneBboxGrowToInclude ( vec3 p ) {
		// update bbox minimums, with this new point
		sceneMins.x = std::min( sceneMins.x, p.x );
		sceneMins.y = std::min( sceneMins.y, p.y );
		sceneMins.z = std::min( sceneMins.z, p.z );
		// and same for bbox maximums
		sceneMaxs.x = std::max( sceneMaxs.x, p.x );
		sceneMaxs.y = std::max( sceneMaxs.y, p.y );
		sceneMaxs.z = std::max( sceneMaxs.z, p.z );
	}

	struct triangle_t {
		int idx;
		vec3 p0, p1, p2; // positions
		vec3 t0, t1, t2; // texcoords
		vec3 c0, c1, c2; // colors
		vec3 n0, n1, n2; // normals
	};

	vector< triangle_t > triangleList;
	GLuint bvhNodeBuffer = 0;
	GLuint bvhDataBuffer = 0;
	GLuint triDataBuffer = 0;
	GLuint lightBuffer = 0;
	void LoadModel () {

		// I'm not sure how I'm handling specification of materials, yet... can we get that from the OBJ? if so, it probably makes sense to drop the SoftRast layer and do it directly...

		s.LoadModel( "../src/projects/PathTracing/Newton/Model/chamber.obj", "../src/projects/PathTracing/Newton/Model/" );
		triangleList.resize( 0 );

		for ( auto& triangle : s.triangles ) {
			triangle_t loaded;

			// vertex positions
			loaded.p0 = triangle.p0;
			loaded.p1 = triangle.p1;
			loaded.p2 = triangle.p2;
			sceneBboxGrowToInclude( triangle.p0 );
			sceneBboxGrowToInclude( triangle.p1 );
			sceneBboxGrowToInclude( triangle.p2 );

			// vertex colors
			loaded.c0 = triangle.c0;
			loaded.c1 = triangle.c1;
			loaded.c2 = triangle.c2;

			// vertex texcoords, not yet important
			// loaded.texcoord0 = triangle.t0;
			// loaded.texcoord1 = triangle.t1;
			// loaded.texcoord2 = triangle.t2;

			// vertex normals in n0, n1, n2... not sure how to use that in a pathtracer
			// loaded.normal = normalize( triangle.n0 + triangle.n1 + triangle.n2 );

			loaded.idx = triangleList.size();

			// and push it onto the list
			triangleList.push_back( loaded );
		}
	}

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			const string basePath = "../src/projects/PathTracing/Newton/shaders/";
			shaders[ "Draw" ]	= computeShader( basePath + "draw.cs.glsl" ).shaderHandle;
			shaders[ "Trace" ]	= computeShader( basePath + "trace.cs.glsl" ).shaderHandle;

			// ================================================================================================================
			// loading a model...
			LoadModel();

			// create a copy of the data with just the information required by the BVH builder
			vector< tinybvh::bvhvec4 > triangleData;
			for ( auto& t : triangleList ) {
				triangleData.push_back( tinybvh::bvhvec4( t.p0.x, t.p0.y, t.p0.z, t.idx ) );
				triangleData.push_back( tinybvh::bvhvec4( t.p1.x, t.p1.y, t.p1.z, t.idx ) );
				triangleData.push_back( tinybvh::bvhvec4( t.p2.x, t.p2.y, t.p2.z, t.idx ) );
			}
			tinybvh::BVH8_CWBVH sceneBVH;
			sceneBVH.BuildHQ( &triangleData[ 0 ], triangleData.size() / 3 );

			// uploading the buffer to the GPU
			Tick();

			glCreateBuffers( 1, &bvhNodeBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, bvhNodeBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sceneBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ), ( GLvoid * ) sceneBVH.bvh8Data, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, bvhNodeBuffer );
			glObjectLabel( GL_BUFFER, bvhNodeBuffer, -1, string( "CWBVH Node Data" ).c_str() );
			cout << "CWBVH8 Node Data is " << GetWithThousandsSeparator( sceneBVH.usedBlocks * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

			glCreateBuffers( 1, &bvhDataBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, bvhDataBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sceneBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ), ( GLvoid * ) sceneBVH.bvh8Tris, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, bvhDataBuffer );
			glObjectLabel( GL_BUFFER, bvhDataBuffer, -1, string( "CWBVH Tri Data" ).c_str() );
			cout << "CWBVH8 Triangle Data is " << GetWithThousandsSeparator( sceneBVH.idxCount * 3 * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

			glCreateBuffers( 1, &triDataBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, triDataBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, triangleData.size() * sizeof( tinybvh::bvhvec4 ), ( GLvoid* ) &triangleData[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, triDataBuffer );
			glObjectLabel( GL_BUFFER, triDataBuffer, -1, string( "Actual Triangle Data" ).c_str() );
			cout << "Triangle Test Data is " << GetWithThousandsSeparator( triangleData.size() * sizeof( tinybvh::bvhvec4 ) ) << " bytes" << endl;

			float msTakenBufferBVH = Tock();
			cout << endl << "BVH passed to GPU in " << msTakenBufferBVH / 1000.0f << "s\n";

			// ================================================================================================================
			// emission spectra LUT textures, packed together
			{
				textureOptions_t opts;
				string LUTPath = "../src/data/spectraLUT/Preprocessed/";
				Image_1F inverseCDF( 1024, numLUTs );

				for ( int i = 0; i < numLUTs; i++ ) {
					Image_4F pdfLUT( LUTPath + LUTFilenames[ i ] + ".png" );

					// First step is populating the cumulative distribution function... "how much of the curve have we passed" (accumulated integral)
					std::vector< float > cdf;
					float cumSum = 0.0f;
					for ( int x = 0; x < pdfLUT.Width(); x++ ) {
						float sum = 0.0f;
						for ( int y = 0; y < pdfLUT.Height(); y++ ) {
							// invert because lut uses dark for positive indication... maybe fix that
							sum += 1.0f - pdfLUT.GetAtXY( x, y ).GetLuma();
						}
						// increment cumulative sum and CDF
						cumSum += sum;
						cdf.push_back( cumSum );
					}

					// normalize the CDF values by the final value during CDF sweep
					std::vector< vec2 > CDFpoints;
					for ( int x = 0; x < pdfLUT.Width(); x++ ) {
						// compute the inverse CDF with the aid of a series of 2d points along the curve
						// adjust baseline for our desired range -> 380nm to 830nm, we have 450nm of data
						CDFpoints.emplace_back( x + 380, cdf[ x ] / cumSum );
					}

					for ( int x = 0; x < 1024; x++ ) {
						// each pixel along this strip needs a value of the inverse CDF
						// this is the intersection with the line defined by the set of segments in the array CDFpoints
						float normalizedPosition = ( x + 0.5f ) / 1024.0f;
						for ( int p = 0; p < CDFpoints.size(); p++ )
							if ( p == ( CDFpoints.size() - 1 ) ) {
								inverseCDF.SetAtXY( x, i, color_1F( { CDFpoints[ p ].x } ) );
							} else if ( CDFpoints[ p ].y >= normalizedPosition ) {
								inverseCDF.SetAtXY( x, i, color_1F( { RangeRemap( normalizedPosition,
										CDFpoints[ p - 1 ].y, CDFpoints[ p ].y, CDFpoints[ p - 1 ].x, CDFpoints[ p ].x ) } ) );
								break;
							}
					}
				}

				// we now have the solution for the LUT
				opts.width = 1024;
				opts.height = numLUTs;
				opts.dataType = GL_R32F;
				opts.minFilter = GL_LINEAR;
				opts.magFilter = GL_LINEAR;
				opts.textureType = GL_TEXTURE_2D;
				opts.pixelDataType = GL_FLOAT;
				opts.initialData = inverseCDF.GetImageDataBasePtr();
				textureManager.Add( "iCDF", opts );
			}

			// ================================================================================================================
			{ // film plane buffer
				textureOptions_t opts;

				opts.textureType = GL_TEXTURE_2D;
				opts.dataType = GL_R32UI;
				opts.minFilter = GL_NEAREST;
				opts.magFilter = GL_NEAREST;
				opts.width = 1920 * 3; // red, green, and blue sensitivities
				opts.height = 1080;

				textureManager.Add( "Film Plane", opts );
			}
			// ================================================================================================================
			{ // I want to do something to visualize the light distribution...
				textureOptions_t opts;
				opts.dataType = GL_RGBA8;
				opts.minFilter = GL_LINEAR;
				opts.magFilter = GL_LINEAR;
				opts.width = 32;
				opts.height = 32;
				opts.textureType = GL_TEXTURE_2D;

				textureManager.Add( "Light Importance Visualizer", opts );
			}

			// some dummy lights...
			lights[ 0 ].emitterParams[ 0 ].x += 1.0f;
			lights[ 0 ].pickedLUT = 3;

			lights[ 1 ].emitterParams[ 0 ].x -= 1.0f;
			lights[ 1 ].pickedLUT = 5;

			{ // we need some colors to visualize the light buffer...
				rng pick( 0.0f, 1.0f );
				for ( int x = 0; x < 1024; x++ ) {
					visualizerColors[ x ] = vec4( pick(), pick(), pick(), 1.0f );
				}
				PrepLightBuffer();
			}

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

	}

	void PrepLightBuffer () {
		// we want to go through the list of lights, and create a data structure to assist with importance sampling them
		if ( !lightBuffer ) {
			glGenBuffers ( 1, &lightBuffer );
		}

		// I'm going to go ahead and say, we are not supporting massive dynamic range... lights will need to have power levels relative to one another that are not more than about 3 orders of magnitude (1000x)
			// this is because we are constructing a list of light indices... these indices are collected such that more powerful lights are more commonly represented in the set. Uniformly picking amongst the
			// elements of the set will therefore constitute a form of importance sampling, whereby more powerful lights are picked to emit rays more frequently.

		// the buffer consists of two parts:
			// 1024 ints, the shuffled list of light indices with weighted representation
			// a list of lights, with at least as many as the maximum index specified in the indices portion... otherwise, it will try to refer to nonexistent data when trying to reference that entry

		// total power helps us inform how the buffer is populated, based on the relative contribution of each light
		float totalPower = 0.0f;
		std::vector< float > powers;
		for ( int l = 0; l < numLights; l++ ) {
			totalPower += lights[ l ].power;
			powers.push_back( totalPower );
		}

		// this only fires when the light list is edited, so I'm going to do this kind of a dumb way, using a uniform distribution to pick
		rng pick = rng( 0.0f, totalPower );
		std::vector< uint32_t > lightBufferDataA;
		for ( int i = 0; i < maxLights; i++ ) {
			float thresh = pick();
			int j = 0;
			for ( ; j < powers.size(); j++ ) {
				if ( thresh <= powers[ j ] ) { // we threw a dart and now have run out to it and passed it. Take this result.
					break; // because we are generating numbers 0.0f to totalPower, we will land in one of these bins, with frequency weighted by their relative magnitude in "power"
				}
			}
			lightBufferDataA.push_back( j );
		}

		// this first part of the buffer, I want to visualize...
		Image_4U importanceVisualizer( 64 * 5 + 1, 16 * 5 + 1 );
		for ( uint32_t y = 0; y < 16; y++ ) {
			for ( uint32_t x = 0; x < 64; x++ ) {
				int index = y * 64 + x;
				int pickedLight = lightBufferDataA[ index ];
				color_4U color, black;
				color[ red ] = visualizerColors[ pickedLight ].r * 255;
				color[ green ] = visualizerColors[ pickedLight ].g * 255;
				color[ blue ] = visualizerColors[ pickedLight ].b * 255;
				black[ alpha ] = color[ alpha ] = 255;
				black[ red ] = black[ green ] = black[ blue ] = 0;

				for ( uint32_t bY = 0; bY < 6; bY++ ) {
					for ( uint32_t bX = 0; bX < 6; bX++ ) {
						if ( bY == 0 || bX == 0 || bY == 5 || bX == 5 ) {
							importanceVisualizer.SetAtXY( 5 * x + bX, 5 * y + bY, black );
						} else {
							importanceVisualizer.SetAtXY( 5 * x + bX, 5 * y + bY, color );
						}
					}
				}
			}
		}
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Light Importance Visualizer" ) );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, importanceVisualizer.Width(), importanceVisualizer.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, importanceVisualizer.GetImageDataBasePtr() );

		// then we also need some information for each individual light...
		std::vector< vec4 > lightBufferDataB;
		for ( int i = 0; i < numLights; i++ ) {
			// the 3x vec4's specifying a light for the GPU process...
			lightBufferDataB.push_back( vec4( lights[ i ].emitterType, lights[ i ].pickedLUT, 0.0f, 0.0f ) );
			lightBufferDataB.push_back( lights[ i ].emitterParams[ 0 ] );
			lightBufferDataB.push_back( lights[ i ].emitterParams[ 1 ] );
			lightBufferDataB.push_back( vec4( 0.0f ) );
		}

		std::vector< uint32_t > lightBufferDataConcat;
		for ( int i = 0; i < maxLights; i++ ) {
			lightBufferDataConcat.push_back( lightBufferDataA[ i ] );
		}
		for ( int i = 0; i < numLights * 4; i++ ) {
			lightBufferDataConcat.push_back( bit_cast< uint32_t >( lightBufferDataB[ i ].x ) );
			lightBufferDataConcat.push_back( bit_cast< uint32_t >( lightBufferDataB[ i ].y ) );
			lightBufferDataConcat.push_back( bit_cast< uint32_t >( lightBufferDataB[ i ].z ) );
			lightBufferDataConcat.push_back( bit_cast< uint32_t >( lightBufferDataB[ i ].w ) );
		}

		// and sending the latest data to the GPU
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, lightBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, 4 * lightBufferDataConcat.size(), ( GLvoid * ) &lightBufferDataConcat[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, lightBuffer );
	}

	void ImguiPass () {
		ZoneScoped;
		bool bufferDirty = false;
		static int flaggedForRemoval = -1; // this will run next frame when I want to remove an entry from the list, to avoid imgui confusion
		if ( flaggedForRemoval != -1 && flaggedForRemoval < numLights ) {
			// remove it from the list by bumping the remainder of the list up
			for ( int i = flaggedForRemoval; i < numLights; i++ ) {
				lights[ i ] = lights[ i + 1 ];
			}

			// reset trigger and decrement light count
			flaggedForRemoval = -1;
			numLights--;
			if ( numLights < 1024 )
				lights[ numLights ] = lightSpec(); // zero out the entry

			bufferDirty = true;
		}

		{
			ImGui::Begin( "Light Config" );

			// visualizer of the light buffer importance structure...
			ImGui::Text( "" );
			const int w = ImGui::GetContentRegionAvail().x;
			ImGui::Image( ( ImTextureID ) ( void * ) intptr_t( textureManager.Get( "Light Importance Visualizer" ) ), ImVec2( w, w * ( 16.0f * 5.0f + 1.0f ) / ( 64.0f * 5.0f + 1.0f ) ) );
			ImGui::Text( "" );

			// iterate through the list of lights, and allow manipulation of state on each one
			for ( int l = 0; l < numLights; l++ ) {
				const string lString = string( "##" ) + to_string( l );

				ImGui::Text( "" );
				// ImGui::Text( ( string( "Light " ) + to_string( l ) ).c_str() );
				ImGui::Indent();

				// color used for visualization
				ImGui::ColorEdit4( ( string( "MyColor" ) + lString ).c_str(), ( float* ) &visualizerColors[ l ], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel );
				bufferDirty |= ImGui::IsItemEdited();

				ImGui::SameLine();
				ImGui::InputTextWithHint( ( string( "Light Name" ) + lString ).c_str(), "Enter a name for this light, if you would like. Not used for anything other than organization.", lights[ l ].label, 256 );
				ImGui::SliderFloat( ( string( "Power" ) + lString ).c_str(), &lights[ l ].power, 0.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
				// this will latch, so we basically get a big chained or that triggers if any of the conditionals like this one got triggered
				bufferDirty |= ImGui::IsItemEdited();
				ImGui::Combo( ( string( "Light Type" ) + lString ).c_str(), &lights[ l ].pickedLUT, LUTFilenames, numLUTs ); // may eventually do some kind of scaled gaussians for user-configurable RGB triplets...
				bufferDirty |= ImGui::IsItemEdited();
				ImGui::Combo( ( string( "Emitter Type" ) + lString ).c_str(), &lights[ l ].emitterType, emitterTypes, numEmitters );
				bufferDirty |= ImGui::IsItemEdited();
				ImGui::Text( "Emitter Settings:" );
				switch ( lights[ l ].emitterType ) {
				case 0: // point emitter

					// need to set the 3D point location
					ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &lights[ l ].emitterParams[ 0 ][ 0 ], -10.0f, 10.0f, "%.3f" );
					bufferDirty |= ImGui::IsItemEdited();

					break;

				case 1: // cauchy beam emitter

					// need to set the 3D emitter location
					ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &lights[ l ].emitterParams[ 0 ][ 0 ], -10.0f, 10.0f, "%.3f" );
					bufferDirty |= ImGui::IsItemEdited();
					// need to set the 3D direction - tbd how this is going to go, euler angles?
					ImGui::SliderFloat3( ( string( "Direction" ) + lString ).c_str(), ( float * ) &lights[ l ].emitterParams[ 1 ][ 0 ], -10.0f, 10.0f, "%.3f" );
					bufferDirty |= ImGui::IsItemEdited();
					// need to set the scale factor for the angular spread
					ImGui::SliderFloat( ( string( "Angular Spread" ) + lString ).c_str(), &lights[ l ].emitterParams[ 0 ][ 3 ], 0.0001f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
					bufferDirty |= ImGui::IsItemEdited();

					break;

				case 2: // laser disk

					// need to set the 3D emitter location
					ImGui::SliderFloat3( ( string( "Position" ) + lString ).c_str(), ( float * ) &lights[ l ].emitterParams[ 0 ][ 0 ], -10.0f, 10.0f, "%.3f" );
					bufferDirty |= ImGui::IsItemEdited();
					// need to set the 3D direction (defining disk plane)
					ImGui::SliderFloat3( ( string( "Direction" ) + lString ).c_str(), ( float * ) &lights[ l ].emitterParams[ 1 ][ 0 ], -10.0f, 10.0f, "%.3f" );
					bufferDirty |= ImGui::IsItemEdited();
					// need to set the radius of the disk being used
					ImGui::SliderFloat( ( string( "Radius" ) + lString ).c_str(), &lights[ l ].emitterParams[ 0 ][ 3 ], 0.0001f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
					bufferDirty |= ImGui::IsItemEdited();

					break;

				case 3: // uniform line emitter

					// need to set the 3D location of points A and B
					ImGui::SliderFloat3( ( string( "Position A" ) + lString ).c_str(), ( float * ) &lights[ l ].emitterParams[ 0 ][ 0 ], -10.0f, 10.0f, "%.3f" );
					bufferDirty |= ImGui::IsItemEdited();
					ImGui::SliderFloat3( ( string( "Position B" ) + lString ).c_str(), ( float * ) &lights[ l ].emitterParams[ 1 ][ 0 ], -10.0f, 10.0f, "%.3f" );
					bufferDirty |= ImGui::IsItemEdited();

					break;

				default:
					ImGui::Text( "Invalid Emitter Type" );
					break;
				}
				if ( ImGui::Button( ( string( "Remove" ) + lString ).c_str() ) ) {
					bufferDirty = true;
					flaggedForRemoval = l;
				}
				ImGui::Text( "" );
				ImGui::Unindent();
				ImGui::Separator();
			}

			// option to add a new light
			ImGui::Text( "" );
			if ( ImGui::Button( " + Add Light " ) ) {
				// add a new light with default settings
				bufferDirty = true;
				numLights++;
			}

			ImGui::End();

			// if anything on the light list got edited this frame, we need to redo the light buffer for the GPU
			if ( bufferDirty ) {
				PrepLightBuffer();
			}
		}

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

		{ // draw the current state of the film buffer into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
			textureManager.BindImageForShader( "Film Plane", "filmPlaneImage", shaders[ "Draw" ], 3 );
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

		// run some rays
		glUseProgram( shaders[ "Trace" ] );

		// environment setup
		rngi wangSeeder = rngi( 0, 1000000 );
		glUniform1ui( glGetUniformLocation( shaders[ "Trace" ], "seedValue" ), wangSeeder() );

		textureManager.BindImageForShader( "iCDF", "lightICDF", shaders[ "Trace" ], 2 );
		textureManager.BindTexForShader( "iCDF", "lightICDF", shaders[ "Trace" ], 2 );
		textureManager.BindImageForShader( "Film Plane", "filmPlaneImage", shaders[ "Trace" ], 3 );

		glDispatchCompute( 16, 16, 1 );

		// what's the plan for autoexposure stuff?

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

// #pragma comment( linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup" )

int main ( int argc, char *argv[] ) {
	Newton engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
