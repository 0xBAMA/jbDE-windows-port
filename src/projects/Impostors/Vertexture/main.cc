#include "../../../engine/engine.h"
#include "vertexture.h"

class Vertexture2 final : public engineBase {
public:
	Vertexture2 () { Init(); OnInit(); PostInit(); }
	~Vertexture2 () { Quit(); }

	// application data
	APIGeometryContainer data;

	// location 3 is the ssbo for the points // are these in a separate address space? I think the binding points are distinct
	// location 4 is the ssbo for the lights

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );

			// initial model orientation
			trident.basisX = vec3(  0.610246f,  0.454481f,  0.648863f );
			trident.basisY = vec3(  0.791732f, -0.321969f, -0.519100f );
			trident.basisZ = vec3( -0.027008f,  0.830518f, -0.556314f );

			// initialize the graphics api shit
			data.textureManager_local = &textureManager;
			data.Reset();

		}
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// application specific controls

		const bool *state = SDL_GetKeyboardState( NULL );
		if ( state[ SDL_SCANCODE_LEFTBRACKET ] )  { data.config.scale /= 0.99f; }
		if ( state[ SDL_SCANCODE_RIGHTBRACKET ] ) { data.config.scale *= 0.99f; }

		if ( state[ SDL_SCANCODE_R ] ) {
			data.Reset(); // SDL_Delay( 40 ); // debounce
		}

	}

	void ImguiPass () {
		ZoneScoped;

		data.ControlWindow();

		if ( tonemap.showTonemapWindow ) {
			TonemapControlsWindow();
		}

		// something to show log events? tbd, that part of vertexture didn't really carry over, we'll see

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );

		// is there a way to jitter the subpixel samples easily? blending/reprojection - resolve more static spheres over several frames
			// using the analytic model from the nvidia paper? would maybe be faster, too? tbd
			// https://developer.download.nvidia.com/presentations/2010/gdc/Direct3D_Effects.pdf

		// ImGuiIO &io = ImGui::GetIO();
		// const float width = io.DisplaySize.x;
		// const float height = io.DisplaySize.y;

		// TODO: consider setting near and far clip planes based on current scale
			// currently, zoomed in, we run into a lot of clipping issues

		// draw the output
		data.Render();
	}

	void ComputePasses () {
		ZoneScoped;

		// { // dummy draw - draw something into accumulatorTexture
		// 	scopedTimer Start( "Background" );
		// 	bindSets[ "Drawing" ].apply();
		// 	glUseProgram( data.resources.shaders[ "Background" ] );
		// 	glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
		// 	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		// }

		// todo: do the deferred update here
		{	// I think this should go pretty smoothly once I have the Gbuffer
			scopedTimer Start( "Deferred Pass" );
			bindSets[ "Drawing" ].apply();
			data.DeferredPass();
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
			if ( data.config.showTiming ) {
				textRenderer.Update( ImGui::GetIO().DeltaTime );
				textRenderer.Draw( textureManager.Get( "Display Texture" ) );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}
		}

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			if ( data.config.showTrident ) {
				trident.Update( textureManager.Get( "Display Texture" ) );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		// main view transform
		data.config.basisX = trident.basisX;
		data.config.basisY = trident.basisY;
		data.config.basisZ = trident.basisZ;

		// application-specific update code
		data.Update();
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();
		DrawAPIGeometry();			// draw the API geometry - this is drawn over the background
		ComputePasses();			// draw the 
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

		// event handling
		HandleTridentEvents();
		HandleCustomEvents();
		HandleQuitEvents(); // may have to skip this, tbd - I want to use keydown events to trigger behavior

		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	// vertextureConfig config;
	// if ( argc == 5 ) {
	// 	// cout << argv[0] << endl; // name of application
	// 	config.numGoodGuys	= atoi( argv[ 1 ] );
	// 	config.numBadGuys	= atoi( argv[ 2 ] );
	// 	config.numTrees		= atoi( argv[ 3 ] );
	// 	config.numBoxes		= atoi( argv[ 4 ] );
	// } else {
	// 	cout << "Incorrect # of command line args" << endl;
	// 	cout << "  Game parameters defaulting" << endl;
	// }

	Vertexture2 engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
