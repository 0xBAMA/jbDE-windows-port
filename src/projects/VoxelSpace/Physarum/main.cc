#include "../../../engine/engine.h"

struct VoxelSpaceConfig_t {

	// renderer config
	vec4 fogColor		= vec4( 0.16f, 0.16f, 0.16f, 1.0f );
	vec2 viewPosition	= vec2( 512, 512 );
	int viewerHeight	= 75;
	float viewAngle		= -0.425f;
	float maxDistance	= 800.0f;
	int horizonLine		= 700;
	float heightScalar	= 1200.0f;
	float offsetScalar	= 0.0f;
	float fogScalar		= 0.451f;
	float stepIncrement	= 0.0f;
	float FoVScalar		= 0.85f;
	float viewBump		= 275.0f;
	float minimapScalar	= 0.3f;
	bool showMinimap	= true;

	// map for the physarum
	ivec2 mapDims;

};

struct physarumConfig_t {

	// environment setup
	uint32_t numAgents;
	uint32_t dimensionX;
	uint32_t dimensionY;
	bool oddFrame = false;

	// agent sim
	float senseAngle;
	float senseDistance;
	float turnAngle;
	float stepSize;
	bool writeBack;
	rngi wangSeed = rngi( 0, 100000 );

	// diffuse + decay
	float decayFactor;
	uint32_t depositAmount;

};

class VoxelSpace final : public engineBase {
public:
	VoxelSpace () { Init(); OnInit(); PostInit(); }
	~VoxelSpace () { Quit(); }

	VoxelSpaceConfig_t voxelSpaceConfig;
	physarumConfig_t physarumConfig;

	GLuint renderFramebuffer;

	GLuint agentSSBO;
	GLuint continuumVAO;
	GLuint continuumVBO;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// load config
			json j; ifstream i ( "../src/engine/config.json" ); i >> j; i.close();
			physarumConfig.numAgents		= j[ "app" ][ "VoxelSpace_Physarum" ][ "numAgents" ];
			physarumConfig.dimensionX		= j[ "app" ][ "VoxelSpace_Physarum" ][ "dimensionX" ];
			physarumConfig.dimensionY		= j[ "app" ][ "VoxelSpace_Physarum" ][ "dimensionY" ];
			physarumConfig.senseAngle		= j[ "app" ][ "VoxelSpace_Physarum" ][ "senseAngle" ];
			physarumConfig.senseDistance	= j[ "app" ][ "VoxelSpace_Physarum" ][ "senseDistance" ];
			physarumConfig.turnAngle		= j[ "app" ][ "VoxelSpace_Physarum" ][ "turnAngle" ];
			physarumConfig.stepSize			= j[ "app" ][ "VoxelSpace_Physarum" ][ "stepSize" ];
			physarumConfig.writeBack		= j[ "app" ][ "VoxelSpace_Physarum" ][ "writeBack" ];
			physarumConfig.decayFactor		= j[ "app" ][ "VoxelSpace_Physarum" ][ "decayFactor" ];
			physarumConfig.depositAmount	= j[ "app" ][ "VoxelSpace_Physarum" ][ "depositAmount" ];

			// compile all the shaders
			const string basePath = "../src/projects/VoxelSpace/Physarum/shaders/";
			shaders[ "VoxelSpace" ]			= computeShader( basePath + "VoxelSpace.cs.glsl" ).shaderHandle;
			shaders[ "MiniMap" ]			= computeShader( basePath + "MiniMap.cs.glsl" ).shaderHandle;
			shaders[ "Update" ]				= computeShader( basePath + "Update.cs.glsl" ).shaderHandle;
			shaders[ "Diffuse and Decay" ]	= computeShader( basePath + "DiffuseAndDecay.cs.glsl" ).shaderHandle;
			shaders[ "Agents" ]				= computeShader( basePath + "Agent.cs.glsl" ).shaderHandle;

			// for rendering into the framebuffer
			shaders[ "Fullscreen Triangle" ] = regularShader(
				basePath + "FullscreenTriangle.vs.glsl",
				basePath + "FullscreenTriangle.fs.glsl"
			).shaderHandle;

			// create a framebuffer to accumulate the images
				// 16-bit color target gets fed into the tonemapping step
			textureOptions_t opts;
			opts.width = config.width;
			opts.height = config.height;
			opts.dataType = GL_RGBA16F;
			opts.textureType = GL_TEXTURE_2D;
			textureManager.Add( "Framebuffer Color", opts );
			opts.dataType = GL_DEPTH_COMPONENT16;
			textureManager.Add( "Framebuffer Depth", opts );

			// // creating the actual framebuffers with their attachments
			const GLenum bufs[] = { GL_COLOR_ATTACHMENT0 }; // framebuffer just has the one color attachment + depth
			glGenFramebuffers( 1, &renderFramebuffer );
			glBindFramebuffer( GL_FRAMEBUFFER, renderFramebuffer );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager.Get( "Framebuffer Depth" ), 0 );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager.Get( "Framebuffer Color" ), 0 );
			glDrawBuffers( 1, bufs ); // how many active attachments, and their attachment locations
			if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
				// cout << newline << "framebuffer creation successful" << endl; // not important to report right now
			}

			// tbd - for right now, I think these are going to be screen resolution, but maybe
				// consider supersampling + generating mips, etc, to supersample / antialias a bit
			opts.dataType = GL_RGBA16F;
			textureManager.Add( "Main Rendered View", opts );		// create the image to draw the regular map into
			textureManager.Add( "Minimap Rendered View", opts );	// create the image to draw the minimap into

		// physarum sim
			// setup the ssbo for the agent data
			struct agent_t {
				vec2 position;
				vec2 direction;
			};

			std::vector< agent_t > agentsInitialData;
			size_t bufferSize = 4 * sizeof( GLfloat ) * physarumConfig.numAgents;
			rng dist( -1.0f, 1.0f );

			for ( uint32_t i = 0; i < physarumConfig.numAgents; i++ ) {
				agentsInitialData.push_back( { { dist(), dist() }, glm::normalize( vec2( dist(), dist() ) ) } );
			}

			glGenBuffers( 1, &agentSSBO );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, agentSSBO );
			glBufferData( GL_SHADER_STORAGE_BUFFER, bufferSize, ( GLvoid * ) &agentsInitialData[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, agentSSBO );

			// setup the image buffers for the atomic writes ( 2x for ping-ponging )
			opts.dataType		= GL_R32UI;
			opts.width			= physarumConfig.dimensionX;
			opts.height			= physarumConfig.dimensionY;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "Pheremone Continuum Buffer 0", opts );
			textureManager.Add( "Pheremone Continuum Buffer 1", opts );
		}
	}

	void positionAdjust ( float amt ) {
		glm::mat2 rotate = glm::mat2(
			 cos( voxelSpaceConfig.viewAngle ),
			 sin( voxelSpaceConfig.viewAngle ),
			-sin( voxelSpaceConfig.viewAngle ),
			 cos( voxelSpaceConfig.viewAngle )
		);
		vec2 direction = rotate * vec2( 1.0f, 0.0f );
		voxelSpaceConfig.viewPosition += amt * direction;
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		const bool * state = SDL_GetKeyboardState( NULL );

		// handle specific keys
		if ( state[ SDL_SCANCODE_D ] || state[ SDL_SCANCODE_RIGHT ] )	voxelSpaceConfig.viewAngle += SDL_GetModState() & SDL_KMOD_SHIFT ? 0.05f : 0.005f;
		if ( state[ SDL_SCANCODE_A ] || state[ SDL_SCANCODE_LEFT ] )	voxelSpaceConfig.viewAngle -= SDL_GetModState() & SDL_KMOD_SHIFT ? 0.05f : 0.005f;
		if ( state[ SDL_SCANCODE_W ] || state[ SDL_SCANCODE_UP ] )		positionAdjust( SDL_GetModState() & SDL_KMOD_SHIFT ? 5.0f : 1.0f );
		if ( state[ SDL_SCANCODE_S ] || state[ SDL_SCANCODE_DOWN ] )	positionAdjust( SDL_GetModState() & SDL_KMOD_SHIFT ? -5.0f : -1.0f );
		if ( state[ SDL_SCANCODE_PAGEUP ] )		voxelSpaceConfig.viewerHeight += SDL_GetModState() & SDL_KMOD_SHIFT ? 10 : 1;
		if ( state[ SDL_SCANCODE_PAGEDOWN ] )	voxelSpaceConfig.viewerHeight -= SDL_GetModState() & SDL_KMOD_SHIFT ? 10 : 1;
		if ( state[ SDL_SCANCODE_HOME ] )		voxelSpaceConfig.horizonLine += SDL_GetModState() & SDL_KMOD_SHIFT ? 10 : 1;
		if ( state[ SDL_SCANCODE_DELETE ] )		voxelSpaceConfig.horizonLine -= SDL_GetModState() & SDL_KMOD_SHIFT ? 10 : 1;
	}

	void ImguiPass () {
		ZoneScoped;

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		// controls window for the renderer parameters
		ImGui::Begin( "Renderer State", NULL, 0 );
		ImGui::Text( " Adjustment of Render parameters" );
		ImGui::Indent();
		ImGui::SliderInt( "Height", &voxelSpaceConfig.viewerHeight, 0, 1800, "%d" );
		ImGui::SliderFloat2( "Position", (float*)&voxelSpaceConfig.viewPosition, 0.0f, 1024.0f, "%.3f" );
		ImGui::SliderFloat( "Angle", &voxelSpaceConfig.viewAngle, -3.14159265f, 3.14159265f, "%.3f" );
		ImGui::SliderFloat( "Max Distance", &voxelSpaceConfig.maxDistance, 10.0f, 5000.0f, "%.3f" );
		ImGui::SliderInt( "Horizon", &voxelSpaceConfig.horizonLine, 0, 3000, "%d" );
		ImGui::SliderFloat( "Height Scale", &voxelSpaceConfig.heightScalar, 0.0f, 1500.0f, "%.3f" );
		ImGui::SliderFloat( "Side-to-Side Offset", &voxelSpaceConfig.offsetScalar, 0.0f, 300.0f, "%.3f" );
		ImGui::SliderFloat( "Step Increment", &voxelSpaceConfig.stepIncrement, 0.0f, 0.5f, "%.3f" );
		ImGui::SliderFloat( "FoV", &voxelSpaceConfig.FoVScalar, 0.001f, 15.0f, "%.3f" );
		ImGui::Text( " " );
		ImGui::SliderFloat( "Fog Scale", &voxelSpaceConfig.fogScalar, 0.0f, 1.5f, "%.3f" );
		ImGui::ColorEdit3( "Fog Color", ( float * ) &voxelSpaceConfig.fogColor, 0 );
		ImGui::Text( " " );
		ImGui::Unindent();
		ImGui::Text( "Physarum" );
		ImGui::Indent();
		// expose controls for the physarum
		if ( ImGui::SmallButton( "Randomize Parameters" ) ) {
			rng senseAngle( 0.0f, float( pi ) );
			rng senseDistance( 0.0f, 0.005f );
			rng turnAngle( 0.0f, float( pi ) );
			rng stepSize( 0.0f, 0.005f );
			rngi depositAmount( 4000, 75000 );
			rng decayFactor( 0.75f, 1.0f );

			// generate new values based on above distributions
			physarumConfig.senseAngle		= senseAngle();
			physarumConfig.senseDistance	= senseDistance();
			physarumConfig.turnAngle		= turnAngle();
			physarumConfig.stepSize			= stepSize();
			physarumConfig.depositAmount	= depositAmount();
			physarumConfig.decayFactor		= decayFactor();
		}
		// widgets
		HelpMarker( "The angle between the sensors." );
		ImGui::SameLine();
		ImGui::Text( "Sensor Angle:" );
		ImGui::SliderFloat( "radians", &physarumConfig.senseAngle, 0.0f, 3.14f, "%.4f" );
		ImGui::Separator();
		HelpMarker( "The distance from the agent position to the sensors." );
		ImGui::SameLine();
		ImGui::Text( "Sensor Distance:" );
		ImGui::SliderFloat( "        ", &physarumConfig.senseDistance, 0.0f, 0.005f, "%.4f" );
		ImGui::Separator();
		HelpMarker( "Amount that each simulation agent can turn in the movement shader." );
		ImGui::SameLine();
		ImGui::Text( "Turn Angle:" );
		ImGui::SliderFloat( "radians ", &physarumConfig.turnAngle, 0.0f, 3.14f, "%.4f" );
		ImGui::Separator();
		HelpMarker( "Distance that each sim agent will go in their current direction each step." );
		ImGui::SameLine();
		ImGui::Text( "Step Size:" );
		ImGui::SliderFloat( "    ", &physarumConfig.stepSize, 0.0f, 0.005f, "%.4f" );
		ImGui::Separator();
		HelpMarker( "Amout of pheremone that is deposited by each simulation agent." );
		ImGui::SameLine();
		ImGui::Text( "Deposit Amount:" );
		ImGui::DragScalar( "  ", ImGuiDataType_U32, &physarumConfig.depositAmount, 50, NULL, NULL, "%u units" );
		ImGui::Separator();
		HelpMarker( "Scale factor applied when storing the result of the gaussian blur." );
		ImGui::SameLine();
		ImGui::Text( "Decay Factor:" );
		ImGui::SliderFloat( "              ", &physarumConfig.decayFactor, 0.75f, 1.0f, "%.4f" );
		ImGui::Checkbox( "Agent Direction Writeback", &physarumConfig.writeBack );
		ImGui::Text( " " );
		ImGui::Unindent();
		ImGui::Text( "MiniMap" );
		ImGui::Indent();
		ImGui::Checkbox( "Show Minimap", &voxelSpaceConfig.showMinimap );
		ImGui::SliderFloat( "View Bump", &voxelSpaceConfig.viewBump, 0.0f, 500.0f, "%.3f" );
		ImGui::SliderFloat( "Minimap Scalar", &voxelSpaceConfig.minimapScalar, 0.1f, 5.0f, "%.3f" );
		ImGui::Text( " " );
		ImGui::Unindent();
		ImGui::Text( " Postprocess Controls" );
		ImGui::Indent();
		const char * tonemapModesList[] = {
			"None (Linear)", "ACES (Narkowicz 2015)", "Unreal Engine 3",
			"Unreal Engine 4", "Uncharted 2", "Gran Turismo",
			"Modified Gran Turismo", "Rienhard", "Modified Rienhard",
			"jt", "robobo1221s", "robo", "reinhardRobo", "jodieRobo",
			"jodieRobo2", "jodieReinhard", "jodieReinhard2"
		};
		ImGui::Combo("Tonemapping Mode", &tonemap.tonemapMode, tonemapModesList, IM_ARRAYSIZE( tonemapModesList ) );
		ImGui::SliderFloat( "Gamma", &tonemap.gamma, 0.0f, 3.0f );
		ImGui::SliderFloat( "Color Temperature", &tonemap.colorTemp, 1000.0f, 40000.0f );

		ImGui::End();
	}

	void ComputePasses () {
		ZoneScoped;

		{
			scopedTimer Start( "VoxelSpace Render" );

			// update the main rendered view - draw the map
			glUseProgram( shaders[ "VoxelSpace" ] );
			glUniform2i( glGetUniformLocation( shaders[ "VoxelSpace" ], "resolution" ), config.width, config.height );
			glUniform2f( glGetUniformLocation( shaders[ "VoxelSpace" ], "viewPosition" ), voxelSpaceConfig.viewPosition.x, voxelSpaceConfig.viewPosition.y );
			glUniform1i( glGetUniformLocation( shaders[ "VoxelSpace" ], "viewerHeight" ), voxelSpaceConfig.viewerHeight );
			glUniform1f( glGetUniformLocation( shaders[ "VoxelSpace" ], "viewAngle" ), voxelSpaceConfig.viewAngle );
			glUniform1f( glGetUniformLocation( shaders[ "VoxelSpace" ], "maxDistance" ), voxelSpaceConfig.maxDistance );
			glUniform1i( glGetUniformLocation( shaders[ "VoxelSpace" ], "horizonLine" ), voxelSpaceConfig.horizonLine );
			glUniform1f( glGetUniformLocation( shaders[ "VoxelSpace" ], "heightScalar" ), voxelSpaceConfig.heightScalar );
			glUniform1f( glGetUniformLocation( shaders[ "VoxelSpace" ], "offsetScalar" ), voxelSpaceConfig.offsetScalar );
			glUniform1f( glGetUniformLocation( shaders[ "VoxelSpace" ], "fogScalar" ), voxelSpaceConfig.fogScalar );
			glUniform1f( glGetUniformLocation( shaders[ "VoxelSpace" ], "stepIncrement" ), voxelSpaceConfig.stepIncrement );
			glUniform1f( glGetUniformLocation( shaders[ "VoxelSpace" ], "FoVScalar" ), voxelSpaceConfig.FoVScalar );
			textureManager.BindImageForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "1" : "0" ), "map", shaders[ "VoxelSpace" ], 1 );
			textureManager.BindImageForShader( "Main Rendered View", "target", shaders[ "VoxelSpace" ], 2 );
			glDispatchCompute( ( config.width + 63 ) / 64, 1, 1 );

			if ( voxelSpaceConfig.showMinimap ) {
				// update the minimap rendered view - draw the area of the map near the user
				glUseProgram( shaders[ "MiniMap" ] );
				glUniform2i( glGetUniformLocation( shaders[ "MiniMap" ], "resolution" ), config.width / 4, config.height / 3 );
				glUniform2f( glGetUniformLocation( shaders[ "MiniMap" ], "viewPosition" ), voxelSpaceConfig.viewPosition.x, voxelSpaceConfig.viewPosition.y );
				glUniform1f( glGetUniformLocation( shaders[ "MiniMap" ], "viewAngle" ), voxelSpaceConfig.viewAngle );
				glUniform1f( glGetUniformLocation( shaders[ "MiniMap" ], "viewBump" ), voxelSpaceConfig.viewBump );
				glUniform1f( glGetUniformLocation( shaders[ "MiniMap" ], "minimapScalar" ), voxelSpaceConfig.minimapScalar );
				textureManager.BindImageForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "1" : "0" ), "map", shaders[ "MiniMap" ], 1 );
				textureManager.BindImageForShader( "Minimap Rendered View", "target", shaders[ "MiniMap" ], 2 );
				glDispatchCompute( ( ( config.width / 4 ) + 63 ) / 64, 1, 1 );
			}

			// we do need a barrier before drawing the fullscreen triangles, images need to complete
				// this kind of prevents, ah, well, profiling the two passes separately, but whatever
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT );
		}

		{
			scopedTimer Start( "Fullscreen Triangle Passes" );

			// bind the framebuffer for drawing the layers
			glBindFramebuffer( GL_FRAMEBUFFER, renderFramebuffer );

			// clear to the background color to the fog color - hijacking alpha blending while drawing color targets to do fog
			glClearColor(
				voxelSpaceConfig.fogColor.x,
				voxelSpaceConfig.fogColor.y,
				voxelSpaceConfig.fogColor.z,
				voxelSpaceConfig.fogColor.w
			);
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			// fullscreen triangle passes:
			glUseProgram( shaders[ "Fullscreen Triangle" ] );
			glUniform2f( glGetUniformLocation( shaders[ "Fullscreen Triangle" ], "resolution" ), config.width, config.height );

			// just want simple overwriting behavior for these passes
			glDisable( GL_DEPTH_TEST );

			// draw the main rendered view, blending with the background color for the fog
			textureManager.BindTexForShader( "Main Rendered View", "current", shaders[ "Fullscreen Triangle" ], 0 );
			glDrawArrays( GL_TRIANGLES, 0, 3 );

			if ( voxelSpaceConfig.showMinimap ) {
				// draw the minimap view, blending with the existing contents, same blending logic
				textureManager.BindTexForShader( "Minimap Rendered View", "current", shaders[ "Fullscreen Triangle" ], 0 );
				glDrawArrays( GL_TRIANGLES, 0, 3 );
			}

			// return to default
			glEnable( GL_DEPTH_TEST );
		}

		// this framebuffer's color attachment becomes the input for the postprocessing step

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			glUseProgram( shaders[ "Tonemap" ] );
			glBindImageTexture( 0, textureManager.Get( "Framebuffer Color" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F );
			glBindImageTexture( 1, textureManager.Get( "Display Texture" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8UI );
			glBindImageTexture( 2, textureManager.Get( "Blue Noise" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8UI );
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

		// reset to default framebuffer
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		// run the shader to do the gaussian blur
		glUseProgram( shaders[ "Diffuse and Decay" ] );
		glBindImageTexture( 1, textureManager.Get( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "0" : "1" ) ), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI ); // previous
		glBindImageTexture( 2, textureManager.Get( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "1" : "0" ) ), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI ); // current
		glUniform1f( glGetUniformLocation( shaders[ "Diffuse and Decay" ], "decayFactor" ), physarumConfig.decayFactor );
		physarumConfig.oddFrame = !physarumConfig.oddFrame;

		glDispatchCompute( ( physarumConfig.dimensionX + 7 ) / 8, ( physarumConfig.dimensionY + 7 ) / 8, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		glUseProgram( shaders[ "Agents" ] );
		glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "stepSize" ), physarumConfig.stepSize );
		glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "senseAngle" ), physarumConfig.senseAngle );
		glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "senseDistance" ), physarumConfig.senseDistance );
		glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "turnAngle" ), physarumConfig.turnAngle );
		glUniform1i( glGetUniformLocation( shaders[ "Agents" ], "writeBack" ), physarumConfig.writeBack );
		glUniform1i( glGetUniformLocation( shaders[ "Agents" ], "wangSeed" ), physarumConfig.wangSeed() );
		glUniform1ui( glGetUniformLocation( shaders[ "Agents" ], "depositAmount" ), physarumConfig.depositAmount );
		glUniform1ui( glGetUniformLocation( shaders[ "Agents" ], "numAgents" ), physarumConfig.numAgents );

		textureManager.BindTexForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "0" : "1" ), "current", shaders[ "Agents" ], 2 );

		// number of 1024-size jobs, rounded up
		const int numSlices = ( physarumConfig.numAgents + 1023 ) / 1024;

		glDispatchCompute( 1, numSlices, 1 );
		glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );

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

		// event handling
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
	VoxelSpace VoxelSpaceInstance;
	while( !VoxelSpaceInstance.MainLoop() );
	return 0;
}
