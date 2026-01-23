#include "engine.h"
#include "includes.h"
#include "vec2.hpp"
#include "vec3.hpp"
#include "../../../engine/engine.h"

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
	float mass			= 30.0f;
	float pad;
	float drag			= 1.0f;
	float senseDistance	= 5.0f;
	float senseAngle	= 0.3f;
	float turnAngle		= 0.9f;
	float forceAmount	= 0.3f;
	float depositAmount	= 10.0f;
	vec2 position		= vec2( 0.0f );
	vec2 velocity		= vec2( 0.0f );
};

struct physarumConfig_t {
// resolution of the substrate buffers
	// ivec2 dims = ivec2( 2880 / 2, 1800 / 2 );
	ivec2 dims = ivec2( 1920 * 2, 1080 * 2 );

// number of agents in play
	uint32_t numAgents = 50000000;
	GLuint agentSSBO = 0;
	agentRecord_t baseAgent;

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
	float fieldDiffuseRadius = 1.0f;

// program state
	bool runSim = true;
	int numIterationsPerFrame = 10;

// moving to impulse-based update will require some kind of clamping...
	int agentSpeedClampMethod = 0;
	float agentMaxSpeedConstant = 1.0f; // enforcing a maximum speed
	float agentDampingForce = 0.5f; // applying a friction/damping force, but not enforcing max speed

// Brush parameters will be handled here

// Drawing parameters
	int falloffMode = 0; // exp or linear falloff
	bool autoExposure = true; // enable autoexposure
	int autoExposureNumLevels = 0;
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
			shaders[ "CopyClear" ]	= computeShader( "../src/projects/Physarum/2D_inertia/shaders/copyClear.cs.glsl" ).shaderHandle;		// copy and clear shader
			shaders[ "Diffuse" ]		= computeShader( "../src/projects/Physarum/2D_inertia/shaders/diffuse.cs.glsl" ).shaderHandle;			// diffuse and decay shader
			shaders[ "Autoexposure" ]	= computeShader( "../src/projects/Physarum/2D_inertia/shaders/autoexposure.cs.glsl" ).shaderHandle;	// autoexposure compute shader
			shaders[ "Draw" ]			= computeShader( "../src/projects/Physarum/2D_inertia/shaders/draw.cs.glsl" ).shaderHandle;			// put data in the accumulator texture

			// shader labels
			glObjectLabel( GL_PROGRAM, shaders[ "Agent" ], -1, string( "Agent" ).c_str() );
			glObjectLabel( GL_PROGRAM, shaders[ "CopyClear" ], -1, string( "CopyClear" ).c_str() );
			glObjectLabel( GL_PROGRAM, shaders[ "Diffuse" ], -1, string( "Diffuse" ).c_str() );
			glObjectLabel( GL_PROGRAM, shaders[ "Autoexposure" ], -1, string( "Autoexposure" ).c_str() );
			glObjectLabel( GL_PROGRAM, shaders[ "Draw" ], -1, string( "Draw" ).c_str() );

			physarumConfig.dims = ivec2( config.width, config.height );

			// create uint buffer for resolving atomics
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= physarumConfig.dims.x;
			opts.height			= physarumConfig.dims.y;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "Pheremone Uint Buffer", opts );

			// create 2 float buffers for blur/decay operation (read/write interface tex + scratch tex)
				// eventually the interface tex will have a mipchain, for the autoexposure usage
			opts.dataType		= GL_R32F;
			opts.magFilter		= GL_LINEAR;
			opts.minFilter		= GL_LINEAR;
			opts.wrap			= GL_REPEAT;
			textureManager.Add( "Pheremone Float Buffer 1", opts ); // interface
			textureManager.Add( "Pheremone Float Buffer 2", opts ); // scratch

			// create the mip levels explicitly... we want to be able to sample the texel (0,0) of the highest mip of the texture for the autoexposure term
			int w = physarumConfig.dims.x;
			int h = physarumConfig.dims.y;
			Image_4F zeroesF( w, h );

			int level = 0;
			while ( h >= 1 ) {
				// we half on both dimensions at once... don't have enough levels available for splitting resolution alternately on x and y at each step
				h /= 2; w /= 2; level++;
				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Pheremone Float Buffer 1" ) );
				glTexImage2D( GL_TEXTURE_2D, level, GL_R32F, w, h, 0, getFormat( GL_R32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );
			}
			physarumConfig.autoExposureNumLevels = level;

			// disable vignette
			tonemap.enableVignette = false;

			// initialize the memory for the agents
			PopulateSSBORandom();
		}
	}

	void PopulateSSBORandom () {
		// populating the SSBO of records

		/*
		rngN offset = rngN( 1.0f, 0.01f );
		rng xD = rng( 0.0f, float( physarumConfig.dims.x - 1 ) );
		rng yD = rng( 0.0f, float( physarumConfig.dims.y - 1 ) );
		rng rotDist = rng( 0.0f, jbDE::tau );

		rng mass = rng( 1.5f, 20.0f );
		rng drag = rng( 0.5f, 1.0f );
		rng senseDistance = rng( 5.0f, 20.0f );
		rng senseAngle = rng( 0.0f, jbDE::tau );
		rng turnAngle = rng( 0.0f, jbDE::tau );
		rng forceAmount	= rng( 0.1f, 2.0f );
		rng depositAmount	= rng( 10.0f, 1000.0f );

		physarumConfig.baseAgent.mass = mass();
		physarumConfig.baseAgent.drag = drag();
		physarumConfig.baseAgent.senseDistance = senseDistance();
		physarumConfig.baseAgent.senseAngle = senseAngle();
		physarumConfig.baseAgent.turnAngle = turnAngle();
		physarumConfig.baseAgent.forceAmount = forceAmount();
		physarumConfig.baseAgent.depositAmount = depositAmount();

		for ( int i = 0; i < physarumConfig.numAgents; i++ ) {
			agentRecord_t agent = physarumConfig.baseAgent;

			// apply small jitter to simulation parameters
			agent.mass *= offset();
			agent.drag *= offset();
			agent.senseDistance *= offset();
			agent.senseAngle *= offset();
			agent.turnAngle *= offset();
			agent.forceAmount *= offset();
			agent.depositAmount *= offset();

			// place the agent on the substrate
			agent.position = vec2( xD(), yD() );

			// agent needs an initial velocity (random unit vector)
			float rot = rotDist();
			agent.velocity = 0.1f * vec2( cos( rot ), sin( rot ) );

			// add it to the list
			agents[ i ] = agent;
		}
		*/

		// create the buffer and upload the contents
		static bool firstTime = true;
		if ( firstTime ) {
			firstTime = false;

			vector< agentRecord_t > agents;
			agents.resize( physarumConfig.numAgents );

			glGenBuffers( 1, &physarumConfig.agentSSBO );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, physarumConfig.agentSSBO );
			glBufferData( GL_SHADER_STORAGE_BUFFER, agents.size() * sizeof( agentRecord_t ), ( GLvoid * ) &agents[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, physarumConfig.agentSSBO );
		}

		// with the buffer created...
		glUseProgram( shaders[ "Agent" ] );

		static rngi wangSeeder = rngi( 0, 10000000 );
		glUniform1i( glGetUniformLocation( shaders[ "Agent" ], "resetSeed" ), wangSeeder() + 1 );
		glUniform1f( glGetUniformLocation( shaders[ "Agent" ], "spread" ), 0.0f );
		glUniform1i( glGetUniformLocation( shaders[ "Agent" ], "wangSeed" ), wangSeeder() );
		glUniform1i( glGetUniformLocation( shaders[ "Agent" ], "numAgents" ), physarumConfig.numAgents );
		textureManager.BindImageForShader( "Pheremone Uint Buffer", "atomicImage", shaders[ "Agent" ], 0 );

		const int workgroupsRoundedUp = ( physarumConfig.numAgents + 63 ) / 64;
		glDispatchCompute( 1, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
		glMemoryBarrier( GL_ALL_BARRIER_BITS );
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// FSM menu update?

		// other controls
		if ( inputHandler.getState4( KEY_R ) == KEYSTATE_FALLING ) {
			// populate buffer
			PopulateSSBORandom();

			// clear substrate
			textureManager.ZeroTexture2D( "Pheremone Float Buffer 1" );
			textureManager.ZeroTexture2D( "Pheremone Float Buffer 2" );
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

		// QuitConf( &quitConfirm ); // show quit confirm window, if triggered
		// if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // draw into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
			glUniform1i( glGetUniformLocation( shaders[ "Draw" ], "autoExposureLevel" ), physarumConfig.autoExposure ? physarumConfig.autoExposureNumLevels - 1 : -1 );
			textureManager.BindTexForShader( "Pheremone Float Buffer 1", "pheremoneBuffer", shaders[ "Draw" ], 2 );
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

		rngi wangSeeder = rngi( 0, 10000000 );

		if ( physarumConfig.runSim ) {
			// loop over number of iterations per frame
			for ( int i = 0; i < physarumConfig.numIterationsPerFrame; i++ ) {

		// Agent Update
			// Agents read from the first float texture (2), which retains state across updates
			// Atomic deposits are resolved to the uint texture (1)
				glUseProgram( shaders[ "Agent" ] );
				textureManager.BindImageForShader( "Pheremone Uint Buffer", "atomicImage", shaders[ "Agent" ], 0 );
				textureManager.BindTexForShader( "Pheremone Float Buffer 1", "floatTex", shaders[ "Agent" ], 1 );
				glUniform1i( glGetUniformLocation( shaders[ "Agent" ], "resetSeed" ), 0 );
				glUniform1i( glGetUniformLocation( shaders[ "Agent" ], "wangSeed" ), wangSeeder() );
				glUniform1i( glGetUniformLocation( shaders[ "Agent" ], "numAgents" ), physarumConfig.numAgents );
				const int workgroupsRoundedUp = ( physarumConfig.numAgents + 63 ) / 64;
				glDispatchCompute( 1, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
				glMemoryBarrier( GL_ALL_BARRIER_BITS );

		// Diffuse/Decay
			// First pass, uint texture (1) contents added to first float texture (2) contents
				// While you're here, clear the uint texture (1) to zero for next frame
				glUseProgram( shaders[ "CopyClear" ] );
				textureManager.BindImageForShader( "Pheremone Uint Buffer", "atomicImage", shaders[ "CopyClear" ], 0 );
				textureManager.BindImageForShader( "Pheremone Float Buffer 1", "floatImage", shaders[ "CopyClear" ], 1 );
				glDispatchCompute( ( physarumConfig.dims.x + 15 ) / 16, ( physarumConfig.dims.y + 15 ) / 16 , 1 );
				glMemoryBarrier( GL_ALL_BARRIER_BITS );
				// ready to do the blur step

			// Second pass, first separable blur pass, first float texture (2) horizontal blur into second float texture (3)
				glUseProgram( shaders[ "Diffuse" ] );

				// bindings for textures and images
				textureManager.BindTexForShader( "Pheremone Float Buffer 1", "sourceTex", shaders[ "Diffuse" ], 0 );
				textureManager.BindImageForShader( "Pheremone Float Buffer 2", "destTex", shaders[ "Diffuse" ], 1 );

				// setup for horizontal pass
				glUniform1i( glGetUniformLocation( shaders[ "Diffuse" ], "separableBlurMode" ), 0 );
				glUniform1f( glGetUniformLocation( shaders[ "Diffuse" ], "radius" ), physarumConfig.fieldDiffuseRadius );
				glDispatchCompute( ( physarumConfig.dims.x + 15 ) / 16, ( physarumConfig.dims.y + 15 ) / 16 , 1 );
				glMemoryBarrier( GL_ALL_BARRIER_BITS );

			// Third pass, second separable blur pass, second float texture (3) vertical blur into first float texture (2)
				// Decay applied when writing back to the first float texture (2)

				// bindings for textures and images
				textureManager.BindTexForShader( "Pheremone Float Buffer 2", "sourceTex", shaders[ "Diffuse" ], 0 );
				textureManager.BindImageForShader( "Pheremone Float Buffer 1", "destTex", shaders[ "Diffuse" ], 1 );

				// setup for vertical pass
				glUniform1i( glGetUniformLocation( shaders[ "Diffuse" ], "separableBlurMode" ), 1 );
				glUniform1f( glGetUniformLocation( shaders[ "Diffuse" ], "radius" ), physarumConfig.fieldDiffuseRadius );
				glDispatchCompute( ( physarumConfig.dims.x + 15 ) / 16, ( physarumConfig.dims.y + 15 ) / 16 , 1 );
				glMemoryBarrier( GL_ALL_BARRIER_BITS );
			}
		}

		static bool firstTime = true;
		static int frame = 0;
		if ( firstTime ) {
			firstTime = false;
		// if ( physarumConfig.autoExposure && frame++ % 60 == 0 ) { // run the autoexposure update ~1Hz
			int w = physarumConfig.dims.x / 2;
			int h = physarumConfig.dims.y / 2;

			const GLuint shader = shaders[ "Autoexposure" ];
			glUseProgram( shader );
			glUniform2i( glGetUniformLocation( shader, "dims" ), physarumConfig.dims.x, physarumConfig.dims.y );
			for ( int n = 0; n < physarumConfig.autoExposureNumLevels - 1; n++ ) { // for num mips minus 1

				// bind the appropriate levels for N and N+1 (starting with N=0... to N=...? )
				textureManager.BindImageForShader( "Pheremone Float Buffer 1", "layerN", shader, 0, n );
				textureManager.BindImageForShader( "Pheremone Float Buffer 1", "layerNPlus1", shader, 1, n + 1 );

				// dispatch the compute shader( 1x1x1 groupsize for simplicity )
				glDispatchCompute( w, h, 1 );
				glMemoryBarrier( GL_ALL_BARRIER_BITS );
				w /= 2; h /= 2;
			}
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
