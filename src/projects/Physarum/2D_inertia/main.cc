#include "engine.h"
#include "includes.h"
#include "vec2.hpp"
#include "vec3.hpp"
#include "../../engine/engine.h"

struct physarumConfig_t {
// resolution of the substrate buffers
	ivec2 dims = ivec2( 2880 / 2, 1800 / 2 );

// number of agents in play
	uint32_t numAgents = 100000u;

// Parameters
// Decay has to be handled at global scope, can't be part of the agent's parameterization
	int fieldDecayMode = 0; // 0 is constant, 1 is noise based... more later
	// constant mode requires only a single value
	float fieldDecayConstantMagnitude = 0.9f;
	// noise mode requires frequency, [-1,1] remap bounds, scale factors on the position and an offset for time varying behavior
	float fieldDecayNoiseFrequency = 1.0f;
	float fieldDecayNoiseLowBound = 0.0f;
	float fieldDecayNoiseHighBound = 1.0f;
	vec3 fieldDecayNoiseSampleScalar = vec3( 1.0f );
	vec3 fieldDecayNoiseSampleOffset = vec3( 0.0f );

// should also have some parameterization around wrapping/edge behavior on the substrate texture

// Diffuse only has one parameter, now
	int fieldDiffuseRadius = 1;

// program state
	bool runSim = true;
	int numIterationsPerFrame = 3;

// moving to impulse-based update will require some kind of clamping...
	int agentSpeedClampMethod = 0;
	float agentMaxSpeedConstant = 1.0f; // enforcing a maximum speed
	float agentDampingForce = 0.5f; // applying a friction/damping force, but not enforcing max speed

// Brush parameters will be handled here

// Drawing parameters
	int falloffMode = 0; // exp or linear falloff
	bool autoExposure = false; // enable autoexposure
};

// 10 floats
// each agent now carries the parameters with it, plus the state:
	// Parameters:
		// Mass
		// Sense Distance
		// Sense Angle
		// Turn Angle
		// Step Size/Force Amount
		// Deposit Amount
	// State:
		// Position (vec2)
		// Velocity (vec2)

struct agentRecord_t {
	float mass			= 1.0f;
	float senseDistance	= 1.0f;
	float senseAngle	= 1.0f;
	float turnAngle		= 1.0f;
	float forceAmount	= 1.0f;
	float depositAmount	= 1.0f;
	vec2 position		= vec2( 0.0f );
	vec2 velocity		= vec2( 0.0f );
};

class PhysarumInertia final : public engineBase {
public:
	PhysarumInertia () { Init(); OnInit(); PostInit(); }
	~PhysarumInertia () { Quit(); }

	physarumConfig_t physarumConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// shader compilation
			shaders[ "Agent" ]		= computeShader( "../src/projects/Physarum/2D_inertia/shaders/agent.cs.glsl" ).shaderHandle;			// agent update shader
			shaders[ "Diffuse" ]		= computeShader( "../src/projects/Physarum/2D_inertia/shaders/diffuse.cs.glsl" ).shaderHandle;		// diffuse/decay shader
			shaders[ "Autoexposure" ]	= computeShader( "../src/projects/Physarum/2D_inertia/shaders/autoexposure.cs.glsl" ).shaderHandle;	// autoexposure compute shader
			shaders[ "Draw" ]			= computeShader( "../src/projects/Physarum/2D_inertia/shaders/draw.cs.glsl" ).shaderHandle;			// put data in the accumulator texture

			// populating the SSBO of records
			vector< agentRecord_t > agents;

			rngN offset = rngN( 0.0f, 0.01f );
			rng xD = rng( 0.0f, float( physarumConfig.dims.x - 1 ) );
			rng yD = rng( 0.0f, float( physarumConfig.dims.y - 1 ) );
			for ( int i = 0; i < physarumConfig.numAgents; i++ ) {
				agentRecord_t agent;

				// apply small jitter to simulation parameters
				agent.mass += offset();
				agent.senseDistance += offset();
				agent.senseAngle += offset();
				agent.turnAngle += offset();
				agent.forceAmount += offset();
				agent.depositAmount += offset();

				// place the agent on the substrate
				agent.position = vec2( xD(), yD() );
			}

			// create uint buffer for resolving atomics
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= physarumConfig.dims.x;
			opts.height			= physarumConfig.dims.y;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.add( "Pheremone Uint Buffer", opts );

			// create 2 float buffers for blur/decay operation (read/write interface tex + scratch tex)
				// eventually the interface tex will have a mipchain, for the autoexposure usage
			opts.dataType		= GL_R32F;
			opts.magFilter		= GL_LINEAR;
			opts.minFilter		= GL_LINEAR;
			textureManager.add( "Pheremone Float Buffer 1", opts ); // interface
			textureManager.add( "Pheremone Float Buffer 2", opts ); // scratch
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// FSM menu update

		// other controls

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

		// QuitConf( &quitConfirm ); // show quit confirm window, if triggered
		// if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // draw into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
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

		// There's been some changes to how the update takes place... it now uses 3 textures, two of which are now floating point.
		// I think this will give more precision for the gradients that the simulation agents use to inform their movement. This
		// also allows for filtered texture reads to interpolate between nearby texels, so we have smoother gradients on several
		// vectors. I started seeing some of the "flow" behavior that looks like fluid sim with larger sense distances, so I think
		// we may be able to do more with this.

		// Textures:
			// 1. Uint32 1-channel for resolving agent atomic writes
			// 2. Float32 1-channel for retaining simulation state between updates
			// 3. Float32 1-channel for scratch memory during blur

		// Another couple of large changes related to the parameterization of the agents themselves... First thing, we had some
		// issues with anisotropy and resolution dependent behavior on the previous 2d implementation because of how the step
		// was calculated. This is easy to correct if we just operate in pixel space, and do the offsets in terms of pixels of
		// distance. Second thing, we are moving from a global set of simulation parameters to now parameters living on each
		// simulation agent. This will be slower, but I don't expect more than a factor of 2 or 3 and I've been able to handle
		// in the ballpark of 50m simulation agents, so I'm not really concerned with performance implications.

		// One of the cool things that this will enable is that you can have different "cultures" interact on the same substrate.
		// You can also have the agents parameters jittered with a small gaussian distribution about the "culture" value. Moving
		// from calling them "presets" to "cultures". I like how this kind of treats it a bit more like a biological system.
		// Notably we have to keep decay handling at global scope, since it is a property of the substrate, not the agent. But
		// even with this limitation, you can create some additional interest by adding a texture-space noise term. Moving to
		// floating point buffers to hold this information and using larger radius blurs, I think we'll see subtler behaviors.

		// I want to do a little bit of a viewer for the cultures you've collected... something with the text renderer and
		// a small resolution preview of the culture's behavior. You can select one from this menu, and inject agents of that
		// culture into the simulation with a brush at your mouse location. This will be further down the line. Maybe even
		// zoom way in and show the individual agents and how they move around on the preview.

		if ( physarumConfig.runSim ) {
			// loop over number of iterations per frame
			for ( int i = 0; i < physarumConfig.numIterationsPerFrame; i++ ) {

		// Agent Update
			// Agents read from the first float texture (2), which retains state across updates
			// Atomic deposits are resolved to the uint texture (1)

				glUseProgram( shaders[ "Agent" ] );

				// run the agent shader

				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// Diffuse/Decay
			// First pass, uint texture (1) contents added to first float texture (2) contents
				// While you're here, clear the uint texture (1) to zero for next frame
			// Second pass, first separable blur pass, first float texture (2) horizontal blur into second float texture (3)
			// Third pass, second separable blur pass, second float texture (3) vertical blur into first float texture (2)
				// Decay applied when writing back to the first float texture (2)

				glUseProgram( shaders[ "Diffuse" ] );

				// horizontal pass
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

				// vertical pass
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}
		}

		if ( physarumConfig.autoExposure ) { // run the autoexposure update
			// todo
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
	PhysarumInertia engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
