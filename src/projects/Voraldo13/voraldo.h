#include "../../engine/engine.h"

#include "dataStructs.h"
#include "menuEntry.h"

class Voraldo13 final : public engineBase {
//==============================================================================
public:
	Voraldo13 () { Init(); OnInit(); PostInit(); }
	~Voraldo13 () { Quit(); }
	
//==============================================================================

	menuContainer menu;	// contains the menu entries ( labels + interface layout blocks )
	renderState render;	// render settings

//==============================================================================

	void SwapBlocks();
	void setColorMipmapFlag();
	void setLightMipmapFlag();
	bool mipmapFlagColor = true;
	bool mipmapFlagLight = true;
	void genColorMipmap();
	void genLightMipmap();
	void SendRaymarchParameters();
	void SendTonemappingParameters();

	// dithering helpers
	void SendDitherParametersQ();
	void SendSelectedPalette();
	void SendDitherParametersP();

	// bindset replacements
	void RenderBindings();
	void BasicOperationBindings();
	void HeightmapOperationBindings();
	void LoadBufferOperationBindings();
	void BasicOperationWithLightingBindings();
	void LightingOperationBindings();

//==============================================================================
// these were previously done via #defines... no reason to hardcode them,
	// they should be dynamic at runtime

	int tileSize = 64;
	float SSFactor = 0.618f;		// supersampling factor for the accumulator
	uvec3 blockDim = uvec3( 256u );	// size of the block

//==============================================================================
// ImGui menu functions + associated operation functions
	// need to figure out argument list for each one, to call in several places:
		// inside the imgui functions
		// inside the terminal commands
		// details tbd, but something for the scripting interface

	void MenuAABB();				float OperationAABB();
	void MenuCylinderTube();		float OperationCylinderTube();
	void MenuEllipsoid();			float OperationEllipsoid();
	void MenuGrid();				float OperationGrid();
	void MenuHeightmap();			float OperationHeightmap();
	void MenuNoise();				float OperationNoise();
	void MenuSphere();				float OperationSphere();
	void MenuTriangle();			float OperationTriangle();
	void MenuUserShader();			float OperationUserShader();
	void MenuVAT();					float OperationVAT();
	void MenuSpaceship();			float OperationSpaceship();
	void MenuLetters();				float OperationLetters();
	void MenuXOR();					float OperationXOR();
	void MenuClearBlock();			float OperationClearBlock();
	void MenuMasking();				float OperationMasking();
	void MenuBlur();				float OperationBlur();
	void MenuShiftTrim();			float OperationShiftTrim();
	void MenuLoadSave();			float OperationLoadSave();
	void MenuLimiterCompressor();	float OperationLimiterCompressor();
	void MenuCopyPaste();			float OperationCopyPaste();
	void MenuLogging();				float OperationLogging();
	void MenuScreenshot();			float OperationScreenshot();
	void MenuClearLightLevels();	float OperationClearLightLevels();
	void MenuPointLight();			float OperationPointLight();
	void MenuConeLight();			float OperationConeLight();
	void MenuDirectionalLight();	float OperationDirectionalLight();
	void MenuFakeGI();				float OperationFakeGI();
	void MenuAmbientOcclusion();	float OperationAmbientOcclusion();
	void MenuLightMash();			float OperationLightMash();

	void MenuApplicationSettings();
	void MenuRenderingSettings();
	void MenuPostProcessingSettings();

	// and some helpers
	void MenuInit();
	void MenuPopulate();
	void MenuLayout( bool* open );
	int currentlySelectedMenuItem = -1;
	void MenuSplash();
	void DrawTextEditorV();
	bool wantCapturePostprocessScreenshot = false;
	float postprocessScreenshotScaleFactor = 1.0f;
	void OrangeText( const char* string );
	void ColorPickerHelper( bool& draw, int& mask, glm::vec4& color );
	void CollapsingSection( string labelString, category_t x, unsigned int& current );
	ImFont* defaultFont;
	ImFont* titleFont;

//==============================================================================
	void newHeightmapPerlin();
	void newHeightmapDiamondSquare();
	void newHeightmapXOR();
	void newHeightmapAND();

	void CapturePostprocessScreenshot();
	void SendUniforms( json j );
	void AddToLog( json j );
	void DumpLog();
	void BlockDispatch();

	void updateSavesList();
	std::vector<string> savesList;
	bool hasEnding( std::string fullString, std::string ending );
	bool hasPNG( std::string filename );

	// json adder helper functions
	void AddBool( json& j, string label, bool value );
	void AddInt( json& j, string label, int value );
	void AddFloat( json& j, string label, float value );
	void AddIvec3( json& j, string label, glm::ivec3 value );
	void AddVec3( json& j, string label, glm::vec3 value );
	void AddVec4( json& j, string label, glm::vec4 value );

	string processAddEscapeSequences( string input );

	bool paletteResendFlag = true;

	// operation logging
	std::vector< json > log;

//==============================================================================

	void CompileShaders();
	void CreateTextures();

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// compile the shaders
			CompileShaders();

			// create the textures
			CreateTextures();

			// setup for menus
			MenuInit();

			/*
			// thinking about adding command line interface for all the operations...
			terminal.addCommand( std::vector< string > commandAndOptionalAliases_in,
				std::vector< var_t > argumentList_in,
				std::function< void( args_t args ) > func_in,
				string description_in );
			*/

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		//==============================================================================
		// Need to keep this for pQuit handling ( force quit ), and it makes scolling easier, too
		//==============================================================================
		SDL_Event event;
		while ( SDL_PollEvent( &event ) ) {
			SDL_PumpEvents();
			// imgui event handling
			ImGui_ImplSDL3_ProcessEvent( &event ); // imgui event handling
			pQuit = config.oneShot || // swap out the multiple if statements for a big chained boolean setting the value of pQuit
				( event.type == SDL_EVENT_QUIT ) ||
				( event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID( window.window ) ) ||
				( event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE && SDL_GetModState() & SDL_KMOD_SHIFT );
			if ( ( event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE ) || ( event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_X1 ) ) {
				quitConfirm = !quitConfirm; // this has to stay because it doesn't seem like ImGui::IsKeyReleased is stable enough to use
			}
			if ( !ImGui::GetIO().WantCaptureMouse ) {
				constexpr float scaleFactor = 0.965f;
				if ( event.type == SDL_EVENT_MOUSE_WHEEL ) {
					if ( event.wheel.y > 0 ) {
						render.scaleFactor *= scaleFactor;
					}
					else if ( event.wheel.y < 0 ) {
						render.scaleFactor /= scaleFactor;
					}
					render.framesSinceLastInput = 0;
				}
				ImVec2 valueRaw = ImGui::GetMouseDragDelta( 0, 0.0f );
				if ( ( valueRaw.x != 0 || valueRaw.y != 0 ) ) {
					render.renderOffset.x -= valueRaw.x;
					render.renderOffset.y += valueRaw.y;
					render.framesSinceLastInput = 0;
					ImGui::ResetMouseDragDelta( 0 );
				}
				valueRaw = ImGui::GetMouseDragDelta( 1, 0.0f );
				if ( ( valueRaw.x != 0 || valueRaw.y != 0 ) ) {
					trident.RotateY( -valueRaw.x * 0.03f );
					trident.RotateX( -valueRaw.y * 0.03f );
					ImGui::ResetMouseDragDelta( 1 );
				}
			}
		}

		//==============================================================================
		// the rest of the event checking just looks at the current input state
		//==============================================================================
		if ( !ImGui::GetIO().WantCaptureKeyboard ) {
			constexpr float bigStep = 0.120f;
			constexpr float lilStep = 0.008f;
			const bool* state = SDL_GetKeyboardState( NULL );

			// panning around, vim style
			if ( state[ SDL_SCANCODE_H ] ) {
				render.renderOffset.x += ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 10.0f : 1.0f;
				render.framesSinceLastInput = 0;
			}
			if ( state[ SDL_SCANCODE_L ] ) {
				render.renderOffset.x -= ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 10.0f : 1.0f;
				render.framesSinceLastInput = 0;
			}
			if ( state[ SDL_SCANCODE_J ] ) {
				render.renderOffset.y += ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 10.0f : 1.0f;
				render.framesSinceLastInput = 0;
			}
			if ( state[ SDL_SCANCODE_K ] ) {
				render.renderOffset.y -= ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 10.0f : 1.0f;
				render.framesSinceLastInput = 0;
			}
			if ( inputHandler.getState4( KEY_Y ) == KEYSTATE_RISING ) {
				CompileShaders();
			}
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

		static bool showMenu = true;
		MenuLayout( &showMenu );

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // update the block
			scopedTimer Start( "Drawing" );
			// bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			// glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			// don't render redundantly - only run for numFramesHistory frames after any state changes
			// if ( render.framesSinceLastInput <= render.numFramesHistory ) {
			if ( true ) {

				genColorMipmap();
				genLightMipmap();
				const GLuint shader = shaders[ "Renderer" ];
				glUseProgram( shader );
				SendRaymarchParameters();
				RenderBindings();

				static rngi noiseOffset = rngi( 0, 512 );

				// tiled update of the accumulator texture
				const int w = SSFactor * config.width;
				const int h = SSFactor * config.height;
				const int t = tileSize;
				for ( int x = 0; x < w; x += t ) {
					for ( int y = 0; y < h; y += t ) {
						glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );
						glUniform2i( glGetUniformLocation( shader, "tileOffset" ), x, y );
						glDispatchCompute( t / 16, t / 16, 1 );
					}
				}
			}
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			bindSets[ "Postprocessing" ].apply();
			glUseProgram( shaders[ "Tonemap" ] );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // dithering
			scopedTimer Start( "Dithering" );

			switch ( render.ditherMode ) {
			case 0: // no dithering
				break;

			case 1: // quantize dither
				glUseProgram( shaders[ "Dither Quantize" ] );
				SendDitherParametersQ();
				glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
				break;

			case 2: // palette dither
				glUseProgram( shaders[ "Dither Palette" ] );
				SendSelectedPalette();
				SendDitherParametersP();
				glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
				break;

			default:
				break;
			}
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		if ( wantCapturePostprocessScreenshot ) { // postprocessed screenshot requested
			CapturePostprocessScreenshot();
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Clear();
			if ( render.showTiming ) {
				textRenderer.Update( ImGui::GetIO().DeltaTime );
			}

			// show terminal, if active - check happens inside
			textRenderer.drawTerminal( terminal );

			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		if ( render.showTrident ) { // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
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

		// increment frame timers
		render.framesSinceLastInput++;
		render.framesSinceStartup++;

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal (active check happens inside)
		terminal.update( inputHandler );

		// event handling
		HandleTridentEvents();
		if ( trident.Dirty() ) // rotation has happened, we need to start drawing again
			render.framesSinceLastInput = 0;

		// quitting, scrolling, etc
		HandleCustomEvents();

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};
