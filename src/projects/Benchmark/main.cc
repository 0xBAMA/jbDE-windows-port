#include "../../engine/engine.h"

class engineDemo final : public engineBase { // sample derived from base engine class
public:
	engineDemo () { Init(); OnInit(); PostInit(); config.oneShot = true; };
	~engineDemo () { Quit(); }

// idea is that each test runs maybe 10 or 100 times, reports the average, variance, of the scores
	const uint32_t numRuns = 50;

// there are some hitches shortly after startup, we need to wait it out
	uint32_t numRunsWarmup = 100;

	// using seeded for repeatability
	rngi hashSeeder = rngi( 0, 1000000, 69420 );
	GLuint rngAtomicShader;
	float Test1 () {
		GLuint queryID[ 2 ];
		glGenQueries( 2, &queryID[ 0 ] );
		glQueryCounter( queryID[ 0 ], GL_TIMESTAMP );

		// establish the new RNG seed
		glUniform1i( glGetUniformLocation( rngAtomicShader, "wangSeed" ), hashSeeder() );

		// dispatch one invocation of the shader and sync
		glDispatchCompute( 256, 256, 1 );
		glMemoryBarrier( GL_ALL_BARRIER_BITS );

		// we have another timing sample
		// runTimes.push_back( Tock() );
		glQueryCounter( queryID[ 1 ], GL_TIMESTAMP );
		GLint timeAvailable = 0;
		while ( !timeAvailable ) { // wait on the most recent of the queries to become available
			glGetQueryObjectiv( queryID[ 1 ], GL_QUERY_RESULT_AVAILABLE, &timeAvailable );
		}

		GLuint64 startTime, stopTime; // get the query results, since they're both ready
		glGetQueryObjectui64v( queryID[ 0 ], GL_QUERY_RESULT, &startTime );
		glGetQueryObjectui64v( queryID[ 1 ], GL_QUERY_RESULT, &stopTime );
		glDeleteQueries( 2, &queryID[ 0 ] ); // and then delete them

		// get final operation time in ms, from difference of nanosecond timestamps
		return ( float( stopTime - startTime ) / 1000000.0f );
	}

	GLuint transferBuffer;
	const uint32_t numBytesTransfer = 1 << 30;
	uint32_t bufferData[ 1 << 28 ] = { 0 };

	float Test2 () {
		GLuint queryID[ 2 ];
		glGenQueries( 2, &queryID[ 0 ] );
		glQueryCounter( queryID[ 0 ], GL_TIMESTAMP );

		for ( int i = 0; i < 500; i++ ) {
			// we're doing a big 256mb transfer, somewhere inside of a 1gb buffer
			glBufferData( GL_SHADER_STORAGE_BUFFER, numBytesTransfer, ( GLvoid * ) bufferData[ 0 ], GL_DYNAMIC_COPY );
			glMemoryBarrier( GL_ALL_BARRIER_BITS );
		}

		// we have another timing sample
		// runTimes.push_back( Tock() );
		glQueryCounter( queryID[ 1 ], GL_TIMESTAMP );
		GLint timeAvailable = 0;
		while ( !timeAvailable ) { // wait on the most recent of the queries to become available
			glGetQueryObjectiv( queryID[ 1 ], GL_QUERY_RESULT_AVAILABLE, &timeAvailable );
		}

		GLuint64 startTime, stopTime; // get the query results, since they're both ready
		glGetQueryObjectui64v( queryID[ 0 ], GL_QUERY_RESULT, &startTime );
		glGetQueryObjectui64v( queryID[ 1 ], GL_QUERY_RESULT, &stopTime );
		glDeleteQueries( 2, &queryID[ 0 ] ); // and then delete them

		// get final operation time in ms, from difference of nanosecond timestamps
		return ( float( stopTime - startTime ) / 1000000.0f ) / 500.0f;
	}

	void OnInit () {
		ZoneScoped;
		{
			// Block Start( "Additional User Init" );

			// compile shaders
			rngAtomicShader = computeShader( "../src/projects/Benchmark/shaders/rngAtomic.cs.glsl" ).shaderHandle;

			// begin
			cout << endl << endl << timeDateString() << " jbDE Benchmarking Results (" << numRuns << " runs/test):" << endl << endl;

		// Test 1: GPU Wang Hash RNG + Atomic Histogram Build
			{
				// compute shader
				glUseProgram( rngAtomicShader );

				// buffer for the atomic operations (1024 bins)
				GLuint buffer;
				glGenBuffers( 1, &buffer );
				glBindBuffer( GL_SHADER_STORAGE_BUFFER, buffer );
				glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( uint32_t ) * 1024, ( GLvoid * ) bufferData[ 0 ], GL_DYNAMIC_DRAW );
				glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, buffer );

				// run N test runs
				std::vector< float > runTimes;
				runTimes.reserve( numRuns );

				for ( uint32_t i = 0; i < numRuns; ++i ) {
					float tResult = 0.0f;
					tResult += Test2();
					if ( numRunsWarmup ) {
						numRunsWarmup--;
						i--;
					} else {
						// get final operation time in ms, from difference of nanosecond timestamps
						runTimes.push_back( tResult );
					}
				}

				// prepare a report for the test
				cout << "Test 1: GPU RNG + Atomic Histogram ( 1024 bins )" << endl;

				// going through the list of timing results to get the mean and variance
				float averageTime = 0.0f;
				float variance = 0.0f;
				// mean
				for ( uint32_t i = 0; i < numRuns; ++i ) {
					averageTime += runTimes[ i ] / float( numRuns );
				}
				// variance
				for ( uint32_t i = 0; i < numRuns; ++i ) {
					variance += pow( runTimes[ i ] - averageTime, 2 ) / float( numRuns );
				}

				cout << " Average Time To Compute 1B Random Numbers: " << averageTime << "ms with a variance of " << variance << " ms" << endl << endl;
			}

			// Test 2: Bus Transfer (Large Buffer Updates)x
			{	// result is megabytes/sec

				cout << "Test 2: Large Buffer Transfer" << endl;

				// creating a 1 gigabyte buffer
				GLuint buffer;
				glGenBuffers( 1, &transferBuffer );
				glBindBuffer( GL_SHADER_STORAGE_BUFFER, transferBuffer );
				glBufferData( GL_SHADER_STORAGE_BUFFER, 1 << 30, ( GLvoid * ) bufferData[ 0 ], GL_DYNAMIC_DRAW );
				glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, transferBuffer );

				// run N test runs
				std::vector< float > runTimes;
				runTimes.reserve( numRuns );

				for ( uint32_t i = 0; i < numRuns; ++i ) {

					// do a big transfer of 256mb
					float tResult = Test2();

					if ( numRunsWarmup ) {
						numRunsWarmup--;
						i--;
					} else {
						// get final operation time in ms, from difference of nanosecond timestamps
						runTimes.push_back( tResult );
					}
				}

				// going through the list of timing results to get the mean and variance
				float averageTransferTime = 0.0f;
				float variance = 0.0f;
				// mean
				for ( uint32_t i = 0; i < numRuns; ++i ) {
					averageTransferTime += ( runTimes[ i ] ) / float( numRuns );
				}
				// variance
				for ( uint32_t i = 0; i < numRuns; ++i ) {
					variance += pow( runTimes[ i ] - averageTransferTime, 2 ) / float( numRuns );
				}

				cout << " Large Buffer Transfers (256mb chunks): " << averageTransferTime << "ms with a variance of " << variance << " ms... approx " << ( float( 1 << 30 ) / averageTransferTime ) / ( 1000.0f * ( 1 << 20 ) ) << " megabytes/sec" << endl << endl;
			}

			// Test 3: Bus Transfer (Small Buffer Updates)
			{	// result in megabytes/sec

			}

			// Test 4: Deterministic Random or Reference Mesh BVH Build + Trace (CPU+GPU)
			{

			}

			// Test 5: Something to really hammer memory bandwidth
			{

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
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
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

engineDemo engineInstance;
int main ( int argc, char *argv[] ) {
	while( !engineInstance.MainLoop() );
	return 0;
}
