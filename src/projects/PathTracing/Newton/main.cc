#include "../../../engine/engine.h"
#include "shaders/lib/shaderWrapper.h"

class Newton final : public engineBase { // sample derived from base engine class
public:
	Newton () { Init(); OnInit(); PostInit(); }
	~Newton () { Quit(); }

	std::vector< string > LUTFilenames = { "AmberLED", "2700kLED", "6500kLED", "Candle", "Flourescent1", "Flourescent2", "Flourescent3", "Halogen", "HPMercury",
		"HPSodium1", "HPSodium2", "LPSodium", "Incandescent", "MetalHalide1", "MetalHalide2", "SkyBlueLED", "SulphurPlasma", "Sunlight", "Xenon" };

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

			float msTakenBufferTerrainBVH = Tock();
			totalTime += msTakenBufferTerrainBVH;
			cout << endl << "Terrain BVH passed to GPU in " << msTakenBufferTerrainBVH / 1000.0f << "s\n";

			// ================================================================================================================
			// emission spectra LUT textures, packed together
			textureOptions_t opts;
			string LUTPath = "../src/data/spectraLUT/Preprocessed/";
			Image_1F inverseCDF( 1024, LUTFilenames.size() );

			for ( int i = 0; i < LUTFilenames.size(); i++ ) {
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
			opts.height = LUTFilenames.size();
			opts.dataType = GL_R32F;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.textureType = GL_TEXTURE_2D;
			opts.pixelDataType = GL_FLOAT;
			opts.initialData = inverseCDF.GetImageDataBasePtr();
			textureManager.Add( "iCDF", opts );

			// ================================================================================================================
			// film plane buffer

			// ================================================================================================================

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
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		// run some rays

		// run the autoexposure stuff

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
