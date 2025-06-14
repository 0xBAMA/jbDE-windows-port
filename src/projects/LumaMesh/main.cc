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
			shaders[ "Draw" ] = computeShader( "../src/projects/LumaMesh/shaders/draw.cs.glsl" ).shaderHandle;

			// compile the shaders
			LineSpamConfig.CompileShaders();

			// add the textures
			LineSpamConfig.textureManager = &textureManager;
			LineSpamConfig.CreateTextures();

			// Image_4F testImage( "../src/projects/LumaMesh/testImages/circuitBoard.png" );
			// Image_4F testImage( "../src/projects/LumaMesh/testImages/waves.png" );
			// Image_4F testImage( "../src/projects/LumaMesh/testImages/crinkle.png" );
			Image_4F testImage( "../src/projects/LumaMesh/testImages/flower.png" );

			PerlinNoise p;

			const float zDispMin = -0.03f;
			const float zDispMax = 0.03f;
			const float noiseScale = 5.0f;
			const float ampScale = 0.0f;

			for ( int y = 1; y < testImage.Height(); y++ ) {
				for ( int x = 1; x < testImage.Width(); x++ ) {
					line l;

				// four corners of a square - 5 lines to draw
					/*	C	@=======@ D
							|      /|
							|     / |
							|    /  |
							|   /   |
							|  /    |
							| /     |
							|/      |
						A	@=======@ B --> X */
					color_4F colA = testImage.GetAtXY( x - 1, y - 1 );
					vec4 cA = vec4( colA[ 0 ], colA[ 1 ], colA[ 2 ], 1.0f );
					vec4 pA = vec4( RemapRange( x - 1, 0, testImage.Width(), -1.0f, 1.0f ), RemapRange( y - 1, 0, testImage.Height(), -1.0f, 1.0f ), RemapRange( colA.GetLuma(), 0.0f, 1.0f, zDispMin, zDispMax ), 1.0f );
					pA.z += ampScale * p.noise( pA.x * noiseScale, pA.y * noiseScale, pA.z * noiseScale );

					color_4F colB = testImage.GetAtXY( x - 1, y );
					vec4 cB = vec4( colB[ 0 ], colB[ 1 ], colB[ 2 ], 1.0f );
					vec4 pB = vec4( RemapRange( x - 1, 0, testImage.Width(), -1.0f, 1.0f ), RemapRange( y, 0, testImage.Height(), -1.0f, 1.0f ), RemapRange( colB.GetLuma(), 0.0f, 1.0f, zDispMin, zDispMax ), 1.0f );
					pB.z += ampScale * p.noise( pB.x * noiseScale, pB.y * noiseScale, pB.z * noiseScale );

					color_4F colC = testImage.GetAtXY( x, y - 1 );
					vec4 cC = vec4( colC[ 0 ], colC[ 1 ], colC[ 2 ], 1.0f );
					vec4 pC = vec4( RemapRange( x, 0, testImage.Width(), -1.0f, 1.0f ), RemapRange( y - 1, 0, testImage.Height(), -1.0f, 1.0f ), RemapRange( colC.GetLuma(), 0.0f, 1.0f, zDispMin, zDispMax ), 1.0f );
					pC.z += ampScale * p.noise( pC.x * noiseScale, pC.y * noiseScale, pC.z * noiseScale );

					color_4F colD = testImage.GetAtXY( x, y );
					vec4 cD = vec4( colD[ 0 ], colD[ 1 ], colD[ 2 ], 1.0f );
					vec4 pD = vec4( RemapRange( x, 0, testImage.Width(), -1.0f, 1.0f ), RemapRange( y, 0, testImage.Height(), -1.0f, 1.0f ), RemapRange( colD.GetLuma(), 0.0f, 1.0f, zDispMin, zDispMax ), 1.0f );
					pD.z += ampScale * p.noise( pD.x * noiseScale, pD.y * noiseScale, pD.z * noiseScale );

					LineSpamConfig.AddLine( { pA, pB, cA, cB } ); // AB
					LineSpamConfig.AddLine( { pA, pC, cA, cC } ); // AC
					LineSpamConfig.AddLine( { pC, pD, cC, cD } ); // CD
					LineSpamConfig.AddLine( { pB, pD, cB, cD } ); // BD
					// LineSpamConfig.AddLine( { pA, pD, cA, cD } ); // AD
				}
			}

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
