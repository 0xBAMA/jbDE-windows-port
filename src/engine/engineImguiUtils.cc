#include "engine.h"

void engineBase::QuitConf ( bool *open ) {
	if ( *open ) {
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos( center, 0, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 230, 55 ) );
		ImGui::OpenPopup( "Quit Confirm" );
		if ( ImGui::BeginPopupModal( "Quit Confirm", NULL, ImGuiWindowFlags_NoDecoration ) ) {
			ImGui::Text( "Are you sure you want to quit?" );
			ImGui::Text( "  " );
			ImGui::SameLine();
			// button to cancel -> set this window's bool to false
			if ( ImGui::Button( " Cancel " ) ) {
				*open = false;
			}
			ImGui::SameLine();
			ImGui::Text( "      " );
			ImGui::SameLine();
			if ( ImGui::Button( " Quit " ) ) {
				pQuit = true; // button to quit -> set pquit to true so program exits
			}
		}
		ImGui::End();
	}
}

void engineBase::HelpMarker ( const char *desc ) {
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
		ImGui::TextUnformatted( desc );
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void engineBase::DrawTextEditor () {
	ZoneScoped;

	ImGui::Begin( "Editor", NULL, 0 );
	static TextEditor editor;

	static auto language = TextEditor::LanguageDefinition::GLSL();
	// static auto language = TextEditor::LanguageDefinition::CPlusPlus();
	editor.SetLanguageDefinition( language );

	auto cursorPosition = editor.GetCursorPosition();
	editor.SetPalette( TextEditor::GetMonoPalette() );

	static const char *fileToEdit = "src/engine/shaders/blit.vs.glsl";
	// static const char *fileToEdit = "src/engine/engineImguiUtils.cc";
	static bool loaded = false;
	if ( !loaded ) {
		std::ifstream t ( fileToEdit );
		editor.SetLanguageDefinition( language );
		if ( t.good() ) {
			editor.SetText( std::string( ( std::istreambuf_iterator< char >( t ) ), std::istreambuf_iterator< char >() ) );
			loaded = true;
		}
	}

	// add dropdown for different shaders? this can be whatever
	ImGui::Text( "%6d/%-6d %6d lines  | %s | %s | %s | %s", cursorPosition.mLine + 1,
		cursorPosition.mColumn + 1, editor.GetTotalLines(),
		editor.IsOverwrite() ? "Ovr" : "Ins",
		editor.CanUndo() ? "*" : " ",
		editor.GetLanguageDefinitionName(), fileToEdit );

	editor.Render( "Editor" );
	HelpMarker( "dummy helpmarker to get rid of unused warning" );
	ImGui::End();
}

bool engineBase::ColorPickerElement ( float &min, float &max, int &selectedPalette, int &colorLimit, string sublabel ) {
	bool edited = false;
	ImGui::SliderFloat( ( string( "Min##" ) + sublabel ).c_str(), &min, 0.0f, 1.0f );
	edited |= ImGui::IsItemEdited();
	ImGui::SliderFloat( ( string( "Max##" ) + sublabel ).c_str(), &max, 0.0f, 1.0f );
	edited |= ImGui::IsItemEdited();

	static std::vector< const char* > paletteLabels;
	if ( paletteLabels.size() == 0 ) {
		for ( auto& entry : palette::paletteListLocal ) {
			// copy to a cstr for use by imgui
			char* d = new char[ entry.label.length() + 1 ];
			std::copy( entry.label.begin(), entry.label.end(), d );
			d[ entry.label.length() ] = '\0';
			paletteLabels.push_back( d );
		}
	}

	ImGui::SliderInt( ( string( "Palette Color Count Limit##" ) + sublabel ).c_str(), &colorLimit, 0, 256 );
	ImGui::Combo( ( string( "Palette##" ) + sublabel ).c_str(), &selectedPalette, paletteLabels.data(), paletteLabels.size() );
	edited |= ImGui::IsItemEdited();

	ImGui::SameLine();
	if ( ImGui::Button( ( string( "Pick Random##" ) + sublabel ).c_str() ) ) {
		do {
			palette::PickRandomPalette( true );
			selectedPalette = palette::PaletteIndex;
		} while ( palette::paletteListLocal[ selectedPalette ].colors.size() > colorLimit );
	}
	edited |= ImGui::IsItemEdited();

	const size_t paletteSize = palette::paletteListLocal[ selectedPalette ].colors.size();
	ImGui::Text( "  Contains %.3lu colors:", palette::paletteListLocal[ palette::PaletteIndex ].colors.size() );
	// handle max < min
	float minVal = min;
	float maxVal = max;
	float realSelectedMin = std::min( minVal, maxVal );
	float realSelectedMax = std::max( minVal, maxVal );
	size_t minShownIdx = std::floor( realSelectedMin * ( paletteSize - 1 ) );
	size_t maxShownIdx = std::ceil( realSelectedMax * ( paletteSize - 1 ) );

	bool finished = false;
	for ( int y = 0; y < 8; y++ ) {
		if ( !finished ) {
			ImGui::Text( " " );
		}
		for ( int x = 0; x < 32; x++ ) {
			// terminate when you run out of colors
			const uint32_t index = x + 32 * y;
			if ( index >= paletteSize ) {
				finished = true;
				// goto terminate;
			}
			// show color, or black if past the end of the list
			ivec4 color = ivec4( 0 );
			if ( !finished ) {
				color = ivec4( palette::paletteListLocal[ selectedPalette ].colors[ index ], 255 );
				// determine if it is in the active range
				if ( index < minShownIdx || index > maxShownIdx ) {
					color.a = 64; // dim inactive entries
				}
			}
			if ( color.a != 0 ) {
				ImGui::SameLine();
				ImGui::TextColored( ImVec4( color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f ), "@" );
			}
		}
	}
	return edited;
}

// this will be removed, once everything is moved over
void engineBase::TonemapControlsWindow () {
	ZoneScoped;

	ImGui::SetNextWindowSize( { 425, 300 } );
	ImGui::Begin( "Tonemapping Controls", NULL, 0 );
	const char* tonemapModesList[] = {
		"None (Linear)",
		"ACES (Narkowicz 2015)",
		"Unreal Engine 3",
		"Unreal Engine 4",
		"Uncharted 2",
		"Gran Turismo",
		"Modified Gran Turismo",
		"Rienhard",
		"Modified Rienhard",
		"jt",
		"robobo1221s",
		"robo",
		"reinhardRobo",
		"jodieRobo",
		"jodieRobo2",
		"jodieReinhard",
		"jodieReinhard2"
	};
	ImGui::Combo("Tonemapping Mode", &tonemap.tonemapMode, tonemapModesList, IM_ARRAYSIZE( tonemapModesList ) );
	ImGui::SliderFloat( "Gamma", &tonemap.gamma, 0.0f, 3.0f );
	ImGui::SliderFloat( "PostExposure", &tonemap.postExposure, 0.0f, 5.0f );
	ImGui::SliderFloat( "Saturation", &tonemap.saturation, 0.0f, 4.0f );
	ImGui::Checkbox( "Saturation Uses Improved Weight Vector", &tonemap.saturationImprovedWeights );
	ImGui::SliderFloat( "Color Temperature", &tonemap.colorTemp, 1000.0f, 40000.0f );
	ImGui::Checkbox( "Enable Vignette", &tonemap.enableVignette );
	if ( tonemap.enableVignette ) {
		ImGui::SliderFloat( "Vignette Power", &tonemap.vignettePower, 0.0f, 2.0f );
	}
	if ( ImGui::Button( "Reset to Defaults" ) ) {
		TonemapDefaults();
	}

	ImGui::End();
}

void engineBase::PostProcessImguiMenu() {
	if ( ImGui::CollapsingHeader( "Color Management" ) ) {
		ImGui::SeparatorText( "Basic" );
		ImGui::SliderFloat( "Exposure##post", &tonemap.postExposure, 0.0f, 5.0f );
		ImGui::SliderFloat( "Gamma", &tonemap.gamma, 0.0f, 3.0f );

		ImGui::SeparatorText( "Adjustments" );
		ImGui::SliderFloat( "Saturation", &tonemap.saturation, 0.0f, 4.0f );
		ImGui::Checkbox( "Saturation Uses Improved Weight Vector", &tonemap.saturationImprovedWeights );
		ImGui::SliderFloat( "Color Temperature", &tonemap.colorTemp, 1000.0f, 40000.0f );

		ImGui::SeparatorText( "Tonemap" );
		const char* tonemapModesList[] = {
			"None (Linear)",
			"ACES (Narkowicz 2015)",
			"Unreal Engine 3",
			"Unreal Engine 4",
			"Uncharted 2",
			"Gran Turismo",
			"Modified Gran Turismo",
			"Rienhard",
			"Modified Rienhard",
			"jt",
			"robobo1221s",
			"robo",
			"reinhardRobo",
			"jodieRobo",
			"jodieRobo2",
			"jodieReinhard",
			"jodieReinhard2",
			"AgX"
		};
		ImGui::Combo("Tonemapping Mode", &tonemap.tonemapMode, tonemapModesList, IM_ARRAYSIZE( tonemapModesList ) );
		ImGui::Text( " " );
	}
	if ( ImGui::CollapsingHeader( "Vignette" ) ) {
		ImGui::Checkbox( "Enable Vignette", &tonemap.enableVignette );
		if ( tonemap.enableVignette ) {
			ImGui::SliderFloat( "Vignette Power", &tonemap.vignettePower, 0.0f, 2.0f );
		}
		ImGui::Text( " " );
	}
	if ( ImGui::CollapsingHeader( "Bloom" ) ) {
		ImGui::Text( "todo" );
		ImGui::Text( " " );
	}
	if ( ImGui::CollapsingHeader( "Lens Distort" ) ) {
		ImGui::Text( "todo" );
		ImGui::Text( " " );
	}
	if ( ImGui::CollapsingHeader( "Dithering" ) ) {
		static bool enable = false;
		static int modeSelect = 0;
		const char* ditherModesList[] = { "Bitcrush", "Distance-Based Palette", "Optimized Distance-based", "Bluescreen's \"Palette Dither\"" };
		ImGui::Checkbox( "Enable", &enable );
		if ( enable == true ) {
			ImGui::Combo( "Mode", &modeSelect, ditherModesList, IM_ARRAYSIZE( ditherModesList ) );
			switch ( modeSelect ) {
				case 0: // bitcrush mode
					// parameterization from before:
						// number of bits
						// space to quantize in
						// method for doing the quantization
				break;

				case 1: // distance mode
					// This takes a palette, and maybe a colorspace to do the distance measurement in
				break;

				case 2: // distance mode, but cache results to 3D texture
					// same as above, but maybe also specify the dimensions of the texture
				break;

				case 3: // bluescreen's "palette dither"
					// I think this just takes a palette
						// optimizing this may involve an SSBO with prefix summed list of cantidates
						// because it ends up being like, sort by luma, pick out of the list by the dither pattern threshold value
				break;

				default:
				break;
			}
		}

		ImGui::Text( " " );
	}

	if ( ImGui::Button( "Reset to Defaults" ) ) {
		TonemapDefaults(); // revisit how this works
	}

}

void engineBase::ImguiFrameStart () {
	ZoneScoped;

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void engineBase::ImguiFrameEnd () {
	ZoneScoped;

	// get it ready to put on the screen
	ImGui::Render();

	// put imgui data into the framebuffer
	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

	// docking/platform windows pending, will need to look at required changes for SDL3

	//// platform windows ( pop out windows )
	//ImGuiIO &io = ImGui::GetIO();
	//if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable && config.allowMultipleViewports ) {
	//	SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
	//	SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
	//	ImGui::UpdatePlatformWindows();
	//	ImGui::RenderPlatformWindowsDefault();
	//	SDL_GL_MakeCurrent( backup_current_window, backup_current_context );
	//}
}
