#include "../../engine/engine.h"

#include "LineSpam.h"

class LineSpam final : public engineBase { // sample derived from base engine class
public:
	LineSpam () { Init(); OnInit(); PostInit(); }
	~LineSpam () { Quit(); }

	LineSpamConfig_t LineSpamConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "../src/projects/LineSpam/shaders/draw.cs.glsl" ).shaderHandle;

			// compile the shaders
			LineSpamConfig.CompileShaders();

			// add the textures
			LineSpamConfig.textureManager = &textureManager;
			LineSpamConfig.CreateTextures();

			// // add some random lines
			rng brightness = rng( 0.2f, 0.9f );
			rngN posGen = rngN( 0.0f, 0.3f );
			rng widthGen = rng( 1.2f, 3.0f );
			// rng posGen = rng( -0.5f, 0.5f );
			for ( int i = 0; i < 200000; i++ ) {
				// rngi pick = rngi( 0, 1 );
				// switch( pick() ) {
				// 	case 0: {
						line l;
						l.p0 = vec4( posGen(), posGen(), posGen(), widthGen() );
						l.p1 = vec4( posGen(), posGen(), posGen(), 1.0f );
						// l.color0 = l.color1 = vec4( brightness(), 0.1f, 0.05f, 0.1f );
						// l.color0 = l.color1 = vec4( 0.4f * brightness(), 0.8f * brightness(), 0.9f * brightness(), 0.01f );
						l.color0 = l.color1 = vec4( 0.9f * brightness(), 0.2f * brightness(), 0.1f * brightness(), 0.1f );
						LineSpamConfig.AddLine( l );
				// 		break;
				// 	}

				// 	case 1: {
				// 		line l;
						l.p0 = vec4( posGen(), posGen(), posGen(), widthGen() );
						l.p1 = vec4( posGen(), posGen(), posGen(), 1.0f );
						l.color0 = l.color1 = vec4( vec3( 0.1f * brightness() ), 1.0f );
						LineSpamConfig.AddLine( l );
				// 		break;
				// 	}

				// 	default:
				// 	break;
				// }
			}

			// const float freq = 5.0f * pi;
			// const float radius = 0.4f;
			// for ( float t = -1.0f; t < 1.0f; t += 0.002f ) {
			// 	line l;
			// 	l.p0 = vec4( radius * cos( t * freq ), radius * sin( t * freq ), t, 1.0f );
			// 	l.p1 = vec4( -radius * cos( t * freq ), -radius * sin( t * freq ), t, 1.0f );
			// 	l.color0 = l.color1 = vec4( brightness(), 0.1f, 0.05f, 0.5f );
			// 	LineSpamConfig.AddLine( l );
			// 	l.p0 = vec4( radius * cos( t * freq ) + 0.03f, radius * sin( t * freq ) + 0.05f, t - 0.01f, 1.0f );
			// 	l.p1 = vec4( -radius * cos( t * freq ) + 0.03f, -radius * sin( t * freq ) + 0.05f, t - 0.01f, 1.0f );
			// 	l.color0 = l.color1 = vec4( vec3( 0.1f * brightness() ), 1.0f );
			// 	LineSpamConfig.AddLine( l );
			// }

			// prepare the line buffers
			LineSpamConfig.PrepLineBuffers();
		}
	}

	void HandleCustomEvents () { // application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
	}

	void ImguiPass () {
		ZoneScoped;

		ImGui::Begin( "LineSpam", NULL );
		ImGui::Text( "Loaded %d opaque lines", LineSpamConfig.opaqueLines.size() );
		ImGui::Text( "Loaded %d transparent lines", LineSpamConfig.transparentLines.size() );
		ImGui::SliderFloat( "Depth Range", &LineSpamConfig.depthRange, 0.001f, 10.0f );
		ImGui::End();

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

		{ // update the frame
			scopedTimer Start( "LineSpam Update" );
			LineSpamConfig.UpdateTransform( inputHandler );
			LineSpamConfig.ClearPass();
			LineSpamConfig.OpaquePass();
			LineSpamConfig.TransparentPass();
			LineSpamConfig.CompositePass();
		}

		{ // copy the composited image into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			textureManager.BindTexForShader( "Composite Target", "compositedResult", shaders[ "Draw" ], 2 );
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
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	LineSpam engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
