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

//==============================================================================
// these were previously done via #defines... no reason to hardcode them,
	// they should be dynamic at runtime

	float SSFactor = 1.0f;			// supersampling factor for the accumulator
	uvec3 blockDim = uvec3( 256u );	// size of the block

//==============================================================================
// ImGui menu functions + associated operation functions
	void MenuAABB();				float OperationAABB();
	void MenuCylinderTube();		float OperationCylinderTube();
	void MenuEllipsoid();			float OperationEllipsoid();
	void MenuGrid();				float OperationGrid();
	void MenuHeightmap();			float OperationHeightmap();
	void MenuIcosahedron();			float OperationIcosahedron();
	void MenuNoise();				float OperationNoise();
	void MenuSphere();				float OperationSphere();
	void MenuTriangle();			float OperationTriangle();
	void MenuUserShader();			float OperationUserShader();
	void MenuVAT();					float OperationVAT();
	void MenuSpaceship();			float OperationSpaceship();
	void MenuLetters();				float OperationLetters();
	void MenuOBJ();					float OperationOBJ();
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
	glm::vec3 GetColorForTemperature( float temperature ); // 6500.0 is white
	std::vector<uint8_t> BayerData( int dimension );
	std::vector<uint8_t> Make4Channel( std::vector<uint8_t> input );

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

	int selectedPalette = 0;
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

		static bool showMenu = true;
		MenuLayout( &showMenu );

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

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

		{ // show trident with current orientation
			// scopedTimer Start( "Trident" );
			// trident.Update( textureManager.Get( "Display Texture" ) );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
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
