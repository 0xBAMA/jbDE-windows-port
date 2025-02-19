#ifndef ENGINE
#define ENGINE
#include "includes.h"

class engineBase {
protected:
//====== Application Handles and Basic Data ===================================
	windowHandler window;				// OpenGL context and SDL2 window
	configData config;					// loaded from config.json
	layerManager textRenderer;			// text renderer framework
	orientTrident trident;				// orientation gizmo from Voraldo13
	textureManager_t textureManager;	// simplified texture interface
	inputHandler_t inputHandler;		// improved input handling (rising, falling edge)
	terminal_t terminal;				// CLI, interface is... wip

	// shaderManager_t shaderManager;	// shader compilation + management of uniforms + texture/image bindings would be nice

//====== OpenGL ===============================================================
	GLuint displayVAO;
	GLuint displayVBO;

	// resource management
	unordered_map< string, GLuint > shaders;	// replace with shaderManager, I think
	unordered_map< string, bindSet > bindSets;	// needs refactor to use the new texture manager

//====== Tonemapping Parameters + Adjustment ==================================
	colorGradeParameters tonemap;
	void TonemapControlsWindow();
	void SendTonemappingParameters( const bool passthrough = false );
	void PostProcessImguiMenu();

//====== Initialization =======================================================
	void StartBlock( string sectionName );
	void EndBlock();
	void Init();
	void PostInit();
	void StartMessage();
	void LoadConfig();
	void TonemapDefaults();
	void CreateWindowAndContext();
	void DisplaySetup();
	void SetupVertexData();
	void SetupTextureData();
	void ShaderCompile();
	void TerminalSetup();
	void LoadData();
	void ImguiSetup();
	void InitialClear();
	void ReportStartupStats();

//====== Main Loop Functions ==================================================
	void BlitToScreen();
	void HandleQuitEvents();
	void HandleTridentEvents();
	void ClearColorAndDepth();
	void ImguiFrameStart();
	void ImguiFrameEnd();
	void DrawTextEditor();
	void MenuLayout( bool * open );
	void QuitConf( bool * open );

//====== ImGui Windows ========================================================
	bool showDemoWindow;
	bool showProfiler;
	void HelpMarker( const char * desc );

//====== LegitProfiler Data ===================================================
	void PrepareProfilingData();
	std::vector< legit::ProfilerTask > tasks_CPU;
	std::vector< legit::ProfilerTask > tasks_GPU;

public:
	timerManager timerQueries_engine;

	// startup timer state
	std::chrono::time_point<std::chrono::steady_clock> tStart = std::chrono::steady_clock::now();
	std::chrono::time_point<std::chrono::steady_clock> tCurr = std::chrono::steady_clock::now();
	void Tick();
	float Tock();
	float TotalTime();

	// data resources
	std::vector< glyph > glyphList;
	std::vector< paletteEntry > paletteList;
	std::vector< string > colorWords;
	std::vector< string > badWords;

//====== Shutdown Procedures ==================================================
protected:
	void ExitMessage();
	void ImguiQuit();
	void Quit();

//====== Program Flags ========================================================
	bool quitConfirm = false;
	bool pQuit = false;

};

#endif