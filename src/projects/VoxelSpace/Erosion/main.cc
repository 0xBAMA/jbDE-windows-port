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

	// thread sync, erosion control - initally true, this prevents the thread running before init completes
	bool erosionReady	= true;
	int erosionSteps	= 8000;

	// map for the eroder
	ivec2 mapDims;
	particleEroder p;

	// timing on the erosion thread
	float msLastUpdate	= 0.0f;

};

class VoxelSpace final : public engineBase {
public:
	VoxelSpace () { Init(); OnInit(); PostInit(); }
	~VoxelSpace () { Quit(); }

	VoxelSpaceConfig_t voxelSpaceConfig;
	GLuint renderFramebuffer;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// load config
			json j; ifstream i ( "../src/engine/config.json" ); i >> j; i.close();
			voxelSpaceConfig.mapDims = ivec2( j[ "app" ][ "VoxelSpace_Erode" ][ "eroderDim" ] );

			// compile all the shaders
			shaders[ "VoxelSpace" ] = computeShader( "../src/projects/VoxelSpace/Erosion/shaders/VoxelSpace.cs.glsl" ).shaderHandle;
			shaders[ "MiniMap" ] = computeShader( "../src/projects/VoxelSpace/Erosion/shaders/MiniMap.cs.glsl" ).shaderHandle;
			shaders[ "Update" ] = computeShader( "../src/projects/VoxelSpace/Erosion/shaders/Update.cs.glsl" ).shaderHandle;

			// for rendering into the framebuffer
			shaders[ "Fullscreen Triangle" ] = regularShader(
				"../src/projects/VoxelSpace/Erosion/shaders/FullscreenTriangle.vs.glsl",
				"../src/projects/VoxelSpace/Erosion/shaders/FullscreenTriangle.fs.glsl"
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

			// initialize the erosion map with a diamond square heightmap
			voxelSpaceConfig.p.InitWithDiamondSquare();

			Image_1F::rangeRemapInputs_t remap;
			remap.rangeType = Image_1F::HARDCLIP;
			remap.rangeStartLow = voxelSpaceConfig.p.model.GetPixelMin( red );
			remap.rangeStartHigh = voxelSpaceConfig.p.model.GetPixelMax( red );
			remap.rangeEndLow = 0.0001f;
			remap.rangeEndHigh = 1.0f;
			voxelSpaceConfig.p.model.RangeRemap( &remap );

			// so there is going to be two maps
			// 1-channel floating point height from the eroder
			opts.width = voxelSpaceConfig.p.model.Width();
			opts.height = voxelSpaceConfig.p.model.Height();
			opts.dataType = GL_R32F;
			opts.textureType = GL_TEXTURE_2D;
			opts.pixelDataType = GL_UNSIGNED_BYTE;
			opts.initialData = nullptr;
			textureManager.Add( "Erosion Result", opts );

			// 4-channel color which is used for the display
			opts.width = voxelSpaceConfig.p.model.Width();
			opts.height = voxelSpaceConfig.p.model.Height();
			opts.dataType = GL_RGBA8UI;
			opts.textureType = GL_TEXTURE_2D;
			opts.pixelDataType = GL_UNSIGNED_BYTE;
			opts.initialData = nullptr;
			textureManager.Add( "Map", opts );

			voxelSpaceConfig.erosionReady = false;		// unset erosionReady flag, since that data is now potentially in flux
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

		int heightRef = voxelSpaceConfig.p.model.GetAtXY( voxelSpaceConfig.viewPosition.x, voxelSpaceConfig.viewPosition.y )[ red ] * 255;
		if ( voxelSpaceConfig.viewerHeight < heightRef )
			voxelSpaceConfig.viewerHeight = heightRef + 5;
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
		ImGui::Text( "MiniMap" );
		ImGui::Indent();
		ImGui::Checkbox( "Show Minimap", &voxelSpaceConfig.showMinimap );
		ImGui::SliderFloat( "View Bump", &voxelSpaceConfig.viewBump, 0.0f, 500.0f, "%.3f" );
		ImGui::SliderFloat( "Minimap Scalar", &voxelSpaceConfig.minimapScalar, 0.1f, 5.0f, "%.3f" );
		ImGui::Text( " " );
		ImGui::Unindent();
		ImGui::Text( "Erosion" );
		ImGui::Indent();
		ImGui::SliderInt( "Steps per Update", &voxelSpaceConfig.erosionSteps, 0, 25000, "%d" );
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
			textureManager.BindImageForShader( "Map", "map", shaders[ "VoxelSpace" ], 1 );
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
				textureManager.BindImageForShader( "Map", "map", shaders[ "MiniMap" ], 1 );
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

	// update thread
	std::thread erosionThread { [=] () {
		while ( !pQuit ) { // while the program is running
			if ( voxelSpaceConfig.erosionReady ) { // waiting
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
			} else {
				auto tstart = std::chrono::high_resolution_clock::now();
				int count = voxelSpaceConfig.erosionSteps;
				// run the erosion for the specified number of steps
				while ( ( count -= 500 ) > 0 ) {
					voxelSpaceConfig.p.Erode( 500 );
				}

				// 1-channel image data will now be ready to pass during the next time OnUpdate() is called
					// no CPU-side prep work is needed - the 4 channel map is prepared on the GPU

				voxelSpaceConfig.msLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::high_resolution_clock::now() - tstart ).count();

				// the work for this timestep has completed
				voxelSpaceConfig.erosionReady = true;
			}
		}
	}};

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		const int w = voxelSpaceConfig.p.model.Width();
		const int h = voxelSpaceConfig.p.model.Height();

		// check to see if the erosion thread is ready
		if ( voxelSpaceConfig.erosionReady ) {
			// update the 1-channel image data on the GPU
			const GLuint handle = textureManager.Get( "Erosion Result" );
			glBindTexture( GL_TEXTURE_2D, handle );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT,
				( void * ) voxelSpaceConfig.p.model.GetImageDataBasePtr() );

			// GPU resource barrier - allow glTexImage2D to complete before continuing
			glMemoryBarrier( GL_TEXTURE_UPDATE_BARRIER_BIT );

			// run the "shading" shader - produces the new 4-channel color / height map for next renderer update
			glUseProgram( shaders[ "Update" ] );
			textureManager.BindImageForShader( "Erosion Result", "sourceData", shaders[ "Update" ], 0 );
			textureManager.BindImageForShader( "Map", "map", shaders[ "Update" ], 1 );
			glDispatchCompute( voxelSpaceConfig.mapDims.x / 16, voxelSpaceConfig.mapDims.y / 16, 1 );

			// make sure it's ready, after that shader runs
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			// tell the erosion thread to continue
			voxelSpaceConfig.erosionReady = false;
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

	// terminate erosion thread, since pQuit is now false
	VoxelSpaceInstance.erosionThread.join();

	return 0;
}
