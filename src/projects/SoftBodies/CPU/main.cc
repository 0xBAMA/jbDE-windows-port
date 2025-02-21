#include "../../../engine/engine.h"
#include "model.h"

class engineChild final : public engineBase {
public:
	engineChild () { Init(); OnInit(); PostInit(); }
	~engineChild () { Quit(); }

	// SoftBody Simulation Model
	model simulationModel;

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );
			// this will also contain application specific textures, shaders, and bindsets

			// compile background shader
			shaders[ "Background" ] = computeShader( "../src/projects/SoftBodies/CPU/shaders/background.cs.glsl" ).shaderHandle;

			// compile display shaders
			simulationModel.simGeometryShader =
				shaders[ "Softbody Display" ] = regularShader( "../src/projects/SoftBodies/CPU/shaders/main.vs.glsl", "../src/projects/SoftBodies/CPU/shaders/main.fs.glsl" ).shaderHandle;

			// load and initialize the model
			simulationModel.loadFramePoints();
			simulationModel.GPUSetup();
			simulationModel.passNewGPUData();

			// for orientaiton control
			simulationModel.trident = &trident;
		}
	}

	void HandleCustomEvents () {
		ZoneScoped;
		// application specific controls
	}

	void ImguiPass () {
		ZoneScoped;

		ImGui::Begin( "Model Config", NULL, 0 );
		if ( ImGui::BeginTabBar( "Config Sections", ImGuiTabBarFlags_None ) ) {
			ImGui::SameLine();
			if ( ImGui::BeginTabItem( "Simulation" ) ) {
				ImGui::SliderFloat( "Time Scale", &simulationModel.simParameters.timeScale, 0.0f, 0.01f );
				ImGui::SliderFloat( "Gravity", &simulationModel.simParameters.gravity, -10.0f, 10.0f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Noise Amplitude", &simulationModel.simParameters.noiseAmplitudeScale, 0.0f, 0.45f );
				ImGui::SliderFloat( "Noise Speed", &simulationModel.simParameters.noiseSpeed, 0.0f, 10.0f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Chassis Node Mass", &simulationModel.simParameters.chassisNodeMass, 0.1f, 10.0f );
				ImGui::SliderFloat( "Chassis K", &simulationModel.simParameters.chassisKConstant, 0.0f, 15000.0f );
				ImGui::SliderFloat( "Chassis Damping", &simulationModel.simParameters.chassisDamping, 0.0f, 100.0f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Suspension K", &simulationModel.simParameters.suspensionKConstant, 0.0f, 15000.0f );
				ImGui::SliderFloat( "Suspension Damping", &simulationModel.simParameters.suspensionDamping, 0.0f, 100.0f );
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Render" ) ) {
				ImGui::Text("Geometry Toggles");
				ImGui::Separator();
				ImGui::Checkbox( "Draw Body Panels", &simulationModel.displayParameters.showChassisFaces );
				ImGui::Checkbox( "Draw Chassis Edges", &simulationModel.displayParameters.showChassisEdges );
				ImGui::Checkbox( "Draw Chassis Nodes", &simulationModel.displayParameters.showChassisNodes );
				ImGui::Checkbox( "Draw Suspension Edges", &simulationModel.displayParameters.showSuspensionEdges );
				ImGui::Text(" ");
				ImGui::Text("Drawing Mode");
				ImGui::Separator();
				ImGui::Checkbox( "Tension Color Only", &simulationModel.displayParameters.tensionColorOnly );
				ImGui::Text(" ");
				ImGui::Text("Scaling");
				ImGui::Separator();
				ImGui::SliderFloat( "Global Scale", &simulationModel.displayParameters.scale, 0.25f, 0.75f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Chassis Rescale", &simulationModel.displayParameters.chassisRescaleAmnt, 0.8f, 1.1f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Line Scale", &simulationModel.drawParameters.lineScale, 1.0f, 20.0f );
				ImGui::SliderFloat( "Outline Ratio", &simulationModel.drawParameters.outlineRatio, 0.8f, 2.0f );
				ImGui::Text(" ");
				ImGui::SliderFloat( "Point Scale", &simulationModel.drawParameters.pointScale, 0.8f, 20.0f );
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		simulationModel.Update(); // update the CPU model
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		simulationModel.Display(); // draw the raster geometry
	}

	void ComputePasses () {
		ZoneScoped;

		// potentially doing the GPU model update here

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Draw Background" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Background" ] );
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
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// clear the buffer
		ComputePasses();			// draw the background + timing data
		BlitToScreen();				// fullscreen triangle copying to the screen
		DrawAPIGeometry();			// draw the model
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

		{ scopedTimer Start( "Handle Events" );
			HandleTridentEvents();
			HandleCustomEvents();
			HandleQuitEvents();
		}

		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	engineChild engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
