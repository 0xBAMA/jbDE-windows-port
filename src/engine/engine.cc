#include "engine.h"

// initialization of OpenGL, etc
void engineBase::Init () {
	ZoneScoped;
	StartMessage();
	LoadConfig();
	CreateWindowAndContext();
	DisplaySetup();
	TerminalSetup();
	ShaderCompile();
	SetupVertexData();
	SetupTextureData();
	ImguiSetup();
	LoadData();
}

// after the derived class custom init
void engineBase::PostInit () {
	InitialClear();
	timerQueries = &timerQueries_engine;
	window.ShowIfNotHeadless();
	ReportStartupStats();
}

// terminate ImGUI
void engineBase::ImguiQuit () {
	ZoneScoped;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

// function to terminate, called from destructor
void engineBase::Quit () {
	ZoneScoped;
	ImguiQuit();
	window.Kill();
	ExitMessage();
}
