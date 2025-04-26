#include "daedalus.h"

void Daedalus::ShowDaedalusConfigWindow() {
	ZoneScoped;
	ImGui::Begin( "Daedalus", NULL );

	ImGui::Text( "Daedalus" );
	ImGui::Checkbox( "Run Renderer", &daedalusConfig.render.render );
	ImGui::SeparatorText( " Performance " );
	float ts = daedalusConfig.tiles.SecondsSinceLastReset();
	ImGui::Text( "Fullscreen Passes: %d in %.3f seconds ( %.3f samples/sec )", daedalusConfig.tiles.SampleCount(), ts, ( float ) daedalusConfig.tiles.SampleCount() / ts );
	ImGui::SeparatorText( " History " );
	// timing history
	const std::vector< float > timeVector = { daedalusConfig.timeHistory.begin(), daedalusConfig.timeHistory.end() };
	const string timeLabel = string( "Average: " ) + std::to_string( std::reduce( timeVector.begin(), timeVector.end() ) / timeVector.size() ).substr( 0, 5 ) + string( "ms/frame" );
	ImGui::PlotLines( " ", timeVector.data(), daedalusConfig.performanceHistorySamples, 0, timeLabel.c_str(), -5.0f, 180.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

	ImGui::SeparatorText( " Screenshots " );
	if ( ImGui::Button( "Capture Tonemapped" ) ) {
		Screenshot( "Tonemapped", true, false );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Capture Color Accumulator" ) ) {
		Screenshot( "Color Accumulator", false, true );
	}
	ImGui::SameLine();
	if ( ImGui::Button( "Capture Depth/Normals Accumulator" ) ) {
		Screenshot( "Depth/Normals Accumulator", false, true );
	}
	ImGui::Separator();

	// begin tabbed interface
	if ( ImGui::BeginTabBar( "Config Sections", ImGuiTabBarFlags_None ) ) {
		if ( ImGui::BeginTabItem( "Viewer Config" ) ) {

			ImGui::Text( "  Output: " );
			ImGui::Text( "  Zoom: %f, Offset: [%f, %f]", daedalusConfig.view.outputZoom, daedalusConfig.view.outputOffset.x, daedalusConfig.view.outputOffset.y );
			ImGui::Indent();
			if ( ImGui::SmallButton( "Reset View Offset" ) ) daedalusConfig.view.outputOffset = ivec2( 0 );
			if ( ImGui::SmallButton( "Reset Zoom" ) ) daedalusConfig.view.outputZoom = 1.0f;
			ImGui::Unindent();
			ImGui::Text( "  Displaying at: %dx%d", daedalusConfig.outputWidth, daedalusConfig.outputHeight );
			ImGui::Checkbox( "Edge Lines", &daedalusConfig.view.edgeLines );
			ImGui::SameLine(); ImGui::SetCursorPosX( 200 ); ImGui::ColorEdit3( "##edge", ( float * ) &daedalusConfig.view.edgeLinesColor, ImGuiColorEditFlags_PickerHueWheel );
			ImGui::Checkbox( "Center Lines", &daedalusConfig.view.centerLines );
			ImGui::SameLine(); ImGui::SetCursorPosX( 200 ); ImGui::ColorEdit3( "##center", ( float * ) &daedalusConfig.view.centerLinesColor, ImGuiColorEditFlags_PickerHueWheel );
			ImGui::Checkbox( "Rule of Thirds Lines", &daedalusConfig.view.ROTLines );
			ImGui::SameLine(); ImGui::SetCursorPosX( 200 ); ImGui::ColorEdit3( "##thirds", ( float * ) &daedalusConfig.view.ROTLinesColor, ImGuiColorEditFlags_PickerHueWheel );
			ImGui::Checkbox( "Golden Ratio Lines", &daedalusConfig.view.goldenLines );
			ImGui::SameLine(); ImGui::SetCursorPosX( 200 ); ImGui::ColorEdit3( "##golden", ( float * ) &daedalusConfig.view.goldenLinesColor, ImGuiColorEditFlags_PickerHueWheel );
			ImGui::Checkbox( "Vignette", &daedalusConfig.view.vignette );
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Scene Settings" ) ) {

			ImGui::SeparatorText( "SDF Raymarch" );
			ImGui::Checkbox( "Enable##raymarch", &daedalusConfig.render.scene.raymarchEnable );
			ImGui::SliderFloat( "Understep", &daedalusConfig.render.scene.raymarchUnderstep, 0.5f, 1.0f );
			ImGui::SliderFloat( "Max Distance", &daedalusConfig.render.scene.raymarchMaxDistance, 0.0f, 300.0f );
			ImGui::SliderInt( "Max Steps", &daedalusConfig.render.scene.raymarchMaxSteps, 0, 300 );

			ImGui::SeparatorText( "DDA Spheres" );
			ImGui::Checkbox( "Enable##ddaspheres", &daedalusConfig.render.scene.ddaSpheresEnable );
			ImGui::SliderFloat( "Bounds Size X", &daedalusConfig.render.scene.ddaSpheresBoundSize.x, 0.1f, 20.0f );
			ImGui::SliderFloat( "Bounds Size Y", &daedalusConfig.render.scene.ddaSpheresBoundSize.y, 0.1f, 20.0f );
			ImGui::SliderFloat( "Bounds Size Z", &daedalusConfig.render.scene.ddaSpheresBoundSize.z, 0.1f, 20.0f );
			ImGui::SliderInt( "Resolution", &daedalusConfig.render.scene.ddaSpheresResolution, 5, 1000 );
			if ( ImGui::Button( "Regen" ) ) {
				// GoLTex();
				DDAVATTex();
			}
			// bounding box center offset

			ImGui::SeparatorText( "Masked Plane" );
			ImGui::Checkbox( "Enable##maskedPlane", &daedalusConfig.render.scene.maskedPlaneEnable );
			if ( ImGui::Button( "Generate" ) ) {
				PrepGlyphBuffer();
			}

			ImGui::SeparatorText( "Explicit Primitives" );
			ImGui::Checkbox( "Enable##explicit", &daedalusConfig.render.scene.explicitListEnable );
			ImGui::SliderInt( "Count##explicit", &daedalusConfig.render.scene.numExplicitPrimitives, 0, 1000 );
			ImGui::SliderFloat( "Marble Radius", &daedalusConfig.render.scene.marbleRadius, 0.0f, 20.0f );
			// more involved parameterization
			if ( ImGui::Button( "Regen List" ) ) {
				PrepSphereBufferRandom();
				RelaxSphereList();
				SendExplicitPrimitiveSSBO();
			}

			// constant color, ollj model, ...
			ImGui::SeparatorText( "Sky" );
			const char * skyModes[] = { "Constant Color", "Two Color Gradient", "ollj Sky Model", "Hard Cut North Pole Sun", "Two Suns", "Nameless", "EXR" };
			ImGui::Combo( "Sky Mode", &daedalusConfig.render.scene.skyMode, skyModes, IM_ARRAYSIZE( skyModes ) );
			daedalusConfig.render.scene.skyNeedsUpdate |= ImGui::IsItemEdited();
			if ( daedalusConfig.render.scene.skyMode == 0 ) { // constant color
				ImGui::ColorEdit3( "Sky Color", ( float * ) &daedalusConfig.render.scene.skyConstantColor1, ImGuiColorEditFlags_PickerHueWheel );
				daedalusConfig.render.scene.skyNeedsUpdate |= ImGui::IsItemEdited();
			} else if ( daedalusConfig.render.scene.skyMode == 1 || daedalusConfig.render.scene.skyMode == 3 || daedalusConfig.render.scene.skyMode == 4 ) {
				ImGui::ColorEdit3( "Azimuth Color", ( float * ) &daedalusConfig.render.scene.skyConstantColor1, ImGuiColorEditFlags_PickerHueWheel );
				daedalusConfig.render.scene.skyNeedsUpdate |= ImGui::IsItemEdited();
				ImGui::ColorEdit3( "Zenith Color", ( float * ) &daedalusConfig.render.scene.skyConstantColor2, ImGuiColorEditFlags_PickerHueWheel );
				daedalusConfig.render.scene.skyNeedsUpdate |= ImGui::IsItemEdited();
				ImGui::SliderInt( "Sun Threshold", &daedalusConfig.render.scene.sunThresh, 0, 500 );
				daedalusConfig.render.scene.skyNeedsUpdate |= ImGui::IsItemEdited();
			} else if ( daedalusConfig.render.scene.skyMode == 2 || daedalusConfig.render.scene.skyMode == 5 ) {
				// ollj model needs float slider for time of day
				ImGui::SliderFloat( "Time Of Day", &daedalusConfig.render.scene.skyTime, -2.0f, 7.0f );
				daedalusConfig.render.scene.skyNeedsUpdate |= ImGui::IsItemEdited();
			} else if ( daedalusConfig.render.scene.skyMode == 6 ) {
				static std::vector< string > savesList;
				if ( savesList.size() == 0 ) { // get the list
					struct pathLeafString {
						std::string operator()( const std::filesystem::directory_entry &entry ) const {
							return entry.path().string();
						}
					};
				// list of exrs, kept in Documents/EXRs/ (which is ../EXRs/, from the working directory that I use)
					std::filesystem::path p( "../../EXRs" );
					std::filesystem::directory_iterator start( p );
					std::filesystem::directory_iterator end;
					std::transform( start, end, std::back_inserter( savesList ), pathLeafString() );
					std::sort( savesList.begin(), savesList.end() ); // sort these alphabetically
				}

				#define LISTBOX_SIZE_MAX 256
				const char *listboxItems[ LISTBOX_SIZE_MAX ];
				uint32_t i;
				for ( i = 0; i < LISTBOX_SIZE_MAX && i < savesList.size(); ++i ) {
					listboxItems[ i ] = savesList[ i ].c_str();
				}

				ImGui::Text( "Files In ../EXRs/" );
				static int listboxSelected = 0;
				ImGui::ListBox( " ", &listboxSelected, listboxItems, i, 24 );

				if ( ImGui::Button( " Load " ) ) {
					LoadSkyBoxEXRFromString( savesList[ listboxSelected ] );
				}
				ImGui::SameLine();
				if ( ImGui::Button( "Refresh List" ) ) {
					savesList.clear(); // will hook the filesystem iterator thing, next frame
				}
				ImGui::SameLine();
				if ( ImGui::Button( " Pick Random " ) ) {
					rngi pick = rngi( 0, savesList.size() - 1 );
					listboxSelected = pick();
					LoadSkyBoxEXRFromString( savesList[ listboxSelected ] );
				}
			}
			ImGui::SliderFloat( "Sky Brightness Scalar", &daedalusConfig.render.scene.skyBrightnessScalar, 0.0f, 5.0f );

			static bool applyClamp = true;
			ImGui::Checkbox( "Clamp Sky", &applyClamp );
			if ( applyClamp ) {
				ImGui::SliderFloat( "Sky Clamp Value", &daedalusConfig.render.scene.skyClamp, 0.0f, 10.0f );
			} else {
				daedalusConfig.render.scene.skyClamp = -1.0f;
			}

			ImGui::Checkbox( "Invert Sampling Direction", &daedalusConfig.render.scene.skyInvert );

			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Rendering Settings" ) ) {

			ImGui::SliderFloat( "Viewer X", &daedalusConfig.render.viewerPosition.x, -40.0f, 40.0f );
			ImGui::SliderFloat( "Viewer Y", &daedalusConfig.render.viewerPosition.y, -40.0f, 40.0f );
			ImGui::SliderFloat( "Viewer Z", &daedalusConfig.render.viewerPosition.z, -40.0f, 40.0f );
			ImGui::Text( "Camera Basis Vectors:" );
			ImGui::Text( " X: %.3f %.3f %.3f", daedalusConfig.render.basisX.x, daedalusConfig.render.basisX.y, daedalusConfig.render.basisX.z );
			ImGui::Text( " Y: %.3f %.3f %.3f", daedalusConfig.render.basisY.x, daedalusConfig.render.basisY.y, daedalusConfig.render.basisY.z );
			ImGui::Text( " Z: %.3f %.3f %.3f", daedalusConfig.render.basisZ.x, daedalusConfig.render.basisZ.y, daedalusConfig.render.basisZ.z );

			ImGui::Separator();
			ImGui::SliderInt( "Max Bounces", &daedalusConfig.render.maxBounces, 0, 256 );
			ImGui::SliderFloat( "Exposure", &daedalusConfig.render.exposure, 0.0f, 10.0f );
			ImGui::SliderFloat( "Epsilon", &daedalusConfig.render.epsilon, 0.0f, 0.01f, "%.6f", ImGuiSliderFlags_Logarithmic );
			ImGui::Separator();

			ImGui::Text( "Camera Mode: " );
			ImGui::RadioButton( "Normal", &daedalusConfig.render.cameraType, 0 );
			ImGui::SameLine();
			ImGui::RadioButton( "Spherical", &daedalusConfig.render.cameraType, 1 );
			ImGui::SameLine();
			ImGui::RadioButton( "Spherical2", &daedalusConfig.render.cameraType, 2 );
			ImGui::SameLine();
			ImGui::RadioButton( "SphereBug", &daedalusConfig.render.cameraType, 3 );
			// ImGui::SameLine();
			ImGui::RadioButton( "FakeOrtho", &daedalusConfig.render.cameraType, 4 );
			ImGui::SameLine();
			ImGui::RadioButton( "Ortho", &daedalusConfig.render.cameraType, 5 );
			ImGui::SameLine();
			ImGui::RadioButton( "Compound", &daedalusConfig.render.cameraType, 6 );
			ImGui::SameLine();
			ImGui::RadioButton( "Voraldo", &daedalusConfig.render.cameraType, 7 );
			ImGui::Separator();
			if ( daedalusConfig.render.cameraType == 7 ) {
				ImGui::SliderFloat( "Perspective Factor", &daedalusConfig.render.voraldoCameraScalar, -2.0f, 2.0f );
				ImGui::Separator();
			}

			ImGui::SliderFloat( "Camera FoV", &daedalusConfig.render.FoV, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
			ImGui::Checkbox( "Jitter Origin", &daedalusConfig.render.cameraOriginJitter );

			ImGui::SeparatorText( "Thin Lens" );
			ImGui::Checkbox( "Enable##thinlens", &daedalusConfig.render.thinLensEnable );
			ImGui::SliderFloat( "Focus Distance", &daedalusConfig.render.thinLensFocusDistance, 0.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Jitter Radius ( Inner )", &daedalusConfig.render.thinLensJitterRadiusInner, 0.0f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Jitter Radius ( Outer )", &daedalusConfig.render.thinLensJitterRadiusOuter, 0.0f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			const char * bokehModeNames[] = { "NONE", "EDGE BIASED DISK", "UNIFORM DISK", "REJECTION SAMPLED HEXAGON", "UNIFORM SAMPLED HEXAGON", "UNIFORM HEART", "THREE BLADE ROSETTE", "FIVE BLADE ROSETTE", "RING", "PENTAGON", "SEPTAGON", "OCTAGON", "NONAGON", "DECAGON", "11-GON", "5 SIDED STAR", "6 SIDED STAR", "7 SIDED STAR" };
			ImGui::Combo( "Bokeh Mode", &daedalusConfig.render.bokehMode, bokehModeNames, IM_ARRAYSIZE( bokehModeNames ) );

			ImGui::Separator();

			ImGui::Text( "Subpixel Jitter: " );
			ImGui::RadioButton( "None", &daedalusConfig.render.subpixelJitterMethod, 0 );
			ImGui::SameLine();
			ImGui::RadioButton( "Blue Noise", &daedalusConfig.render.subpixelJitterMethod, 1 );
			ImGui::SameLine();
			ImGui::RadioButton( "Uniform RNG", &daedalusConfig.render.subpixelJitterMethod, 2 );
			ImGui::SameLine();
			ImGui::RadioButton( "Weyl", &daedalusConfig.render.subpixelJitterMethod, 3 );
			ImGui::SameLine();
			ImGui::RadioButton( "Weyl (Integer)", &daedalusConfig.render.subpixelJitterMethod, 4 );
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Tiles" ) ) {

			ImGui::Text( "Tile Config:" );
			ImGui::Text( " Split the %dx%d image into %d tiles", daedalusConfig.targetWidth, daedalusConfig.targetHeight, daedalusConfig.tiles.Count() );
			ImGui::Text( " Tile Size:" );
			ImGui::SameLine();
			if ( ImGui::SmallButton( " - " ) ) {
				daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize / 2, daedalusConfig.targetWidth, daedalusConfig.targetHeight );
			}
			ImGui::SameLine();
			ImGui::Text( "%d", daedalusConfig.tiles.tileSize );
			ImGui::SameLine();
			if ( ImGui::SmallButton( " + " ) ) {
				daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize * 2, daedalusConfig.targetWidth, daedalusConfig.targetHeight );
			}
			ImGui::SliderInt( "Tiles Between Queries", ( int * )&daedalusConfig.tiles.tilesBetweenQueries, 1, 15 );
			ImGui::SliderFloat( "Frame Time Limit (ms)", &daedalusConfig.tiles.tileTimeLimitMS, 16.0f, 100.0f );

			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Images" ) ) {

			ImGui::Text( "Resolution" );
			static int x = daedalusConfig.targetWidth;
			static int y = daedalusConfig.targetHeight;
			ImGui::SameLine();
			if ( ImGui::SmallButton( " Resize " ) ) {
				ResizeAccumulators( x, y );
			}
			ImGui::SliderInt( "Accumulator X", &x, 0, 5000 );
			ImGui::SliderInt( "Accumulator Y", &y, 0, 5000 );
			ImGui::Text( "Presets:" );
			ImGui::Indent();
			ImGui::Text( "Standard" );
			if ( ImGui::SmallButton( " Preview " ) )	ResizeAccumulators( x = 160, y = 90 );
			ImGui::SameLine(); if ( ImGui::SmallButton( " 360p " ) )		ResizeAccumulators( x = 640, y = 360 );
			ImGui::SameLine(); if ( ImGui::SmallButton( " 720p " ) )		ResizeAccumulators( x = 1280, y = 720 );
			ImGui::SameLine(); if ( ImGui::SmallButton( " 1080p " ) )	ResizeAccumulators( x = 1920, y = 1080 );
			ImGui::SameLine(); if ( ImGui::SmallButton( " 4K " ) )		ResizeAccumulators( x = 3840, y = 2160 );
			ImGui::Unindent();
			ImGui::Indent();
			ImGui::Text( "Ultrawide" );
			if ( ImGui::SmallButton( " Preview ##ultrawide" ) )	ResizeAccumulators( x = 768, y = 144 );
			ImGui::SameLine(); if ( ImGui::SmallButton( " Wallpaper ##ultrawide" ) )	ResizeAccumulators( x = 7680, y = 1440 );
			ImGui::Unindent();
			ImGui::Indent();
			ImGui::Text( "HDRI" ); // ouroboros
			if ( ImGui::SmallButton( " Preview ##hdri" ) )	ResizeAccumulators( x = 256, y = 128 );
			ImGui::SameLine(); if ( ImGui::SmallButton( " 4k ##hdri" ) )	ResizeAccumulators( x = 4096, y = 2048 );
			ImGui::Unindent();

			float availableWidth = ImGui::GetContentRegionAvail().x - 20;
			float proportionalHeight = availableWidth * ( float ) config.height / ( float ) config.width;

			ImGui::SeparatorText( "Display Texture" );
			ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Display Texture" ), ImVec2( availableWidth, proportionalHeight ) );
			if ( ImGui::Button( "Capture##DisplayTexture" ) ) {
				Screenshot( "Display Texture" );
			}

			proportionalHeight = availableWidth * ( float ) daedalusConfig.targetHeight / ( float ) daedalusConfig.targetWidth;

			ImGui::SeparatorText( "Color Accumulator" );
			ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Color Accumulator" ), ImVec2( availableWidth, proportionalHeight ) );
			if ( ImGui::Button( "Capture##ColorAccumulator" ) ) {
				Screenshot( "Color Accumulator", true, true );
			}

			ImGui::SeparatorText( "Color Accumulator Scratch" );
			ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Color Accumulator Scratch" ), ImVec2( availableWidth, proportionalHeight ) );

			ImGui::SeparatorText( "Depth/Normals Accumulator" );
			ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Depth/Normals Accumulator" ), ImVec2( availableWidth, proportionalHeight ) );
			if ( ImGui::Button( "Capture##DepthNormalAccumulator" ) ) {
				Screenshot( "Depth/Normals Accumulator", true, true );
			}

			ImGui::SeparatorText( "Tonemapped" );
			ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Tonemapped" ), ImVec2( availableWidth, proportionalHeight ) );
			if ( ImGui::Button( "Capture##Tonemapped" ) ) {
				Screenshot( "Tonemapped", true, false );
			}

			ImGui::SeparatorText( "Sky Cache" );
			ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Sky Cache" ), ImVec2( availableWidth, availableWidth / 2.0f ) );
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Filters" ) ) {

			ImGui::Text( "Filter Mode: " );
			ImGui::SameLine();
			ImGui::RadioButton( " Gaussian ", &daedalusConfig.filterSelector, 0 );
			ImGui::SameLine();
			ImGui::RadioButton( " Median ", &daedalusConfig.filterSelector, 1 );
			ImGui::SameLine();
			ImGui::RadioButton( " Combo ", &daedalusConfig.filterSelector, 2 );
			ImGui::SameLine();
			ImGui::RadioButton( " Kuwahara ", &daedalusConfig.filterSelector, 3 );
			ImGui::SameLine();
			ImGui::RadioButton( " Random ", &daedalusConfig.filterSelector, 4 );

			ImGui::Text( "Apply: " );
			ImGui::SameLine();
			if ( ImGui::Button( " 1x " ) ) {
				ApplyFilter( daedalusConfig.filterSelector, 1 );
			} ImGui::SameLine();
			if ( ImGui::Button( " 10x " ) ) {
				ApplyFilter( daedalusConfig.filterSelector, 10 );
			} ImGui::SameLine();
			if ( ImGui::Button( " 100x " ) ) {
				ApplyFilter( daedalusConfig.filterSelector, 100 );
			}
			ImGui::SliderFloat( "Blend Amount", &daedalusConfig.filterBlendAmount, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
			ImGui::Checkbox( "Apply Every Frame", &daedalusConfig.filterEveryFrame );

			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Postprocess" ) ) {

			// histogram display
			ImGui::SeparatorText( "Exposure Histogram" );
			ImGui::SameLine();
			ImGui::Checkbox( "Update##exposure", &daedalusConfig.render.grading.updateHistogram );
			ImGui::Indent();
			if ( daedalusConfig.render.grading.updateHistogram == true ) {
				const float availableWidth = ImGui::GetContentRegionAvail().x - 20;
				const float proportionalHeight = availableWidth * 0.25f;
				ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Histogram Composite" ), ImVec2( availableWidth, proportionalHeight ) );
			}

			// parade graph display
			ImGui::Unindent();
			ImGui::SeparatorText( "Parade Graph Display" );
			ImGui::SameLine();
			ImGui::Checkbox( "Update##parade", &daedalusConfig.render.grading.updateParade );
			ImGui::Indent();
			if ( daedalusConfig.render.grading.updateParade == true ) {
				const float availableWidth = ImGui::GetContentRegionAvail().x - 20;
				const float proportionalHeight = ( 1024.0f / daedalusConfig.targetWidth ) * availableWidth;
				ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Parade Composite" ), ImVec2( availableWidth, proportionalHeight ) );
			}

			// waveform display
			ImGui::Unindent();
			ImGui::SeparatorText( "Waveform Display" );
			ImGui::SameLine();
			ImGui::Checkbox( "Update##waveform", &daedalusConfig.render.grading.updateWaveform );
			ImGui::Indent();
			if ( daedalusConfig.render.grading.updateWaveform == true ) {
				const float availableWidth = ImGui::GetContentRegionAvail().x - 20;
				// proportions will maybe need to be tweaked
				const float proportionalHeight = ( 512.0f / daedalusConfig.targetWidth ) * availableWidth;
				ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Waveform Composite" ), ImVec2( availableWidth, proportionalHeight ) );
			}

			ImGui::Unindent();
			ImGui::SeparatorText( "Vectorscope Display" );
			ImGui::SameLine();
			ImGui::Checkbox( "Update##vectorscope", &daedalusConfig.render.grading.updateVectorscope );
			ImGui::Indent();
			if ( daedalusConfig.render.grading.updateVectorscope == true ) {
				const float availableWidth = ImGui::GetContentRegionAvail().x - 20;
				ImGui::Image( ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Vectorscope Composite" ), ImVec2( availableWidth, availableWidth ) );
			}

			// this needs significant work
			if ( ImGui::CollapsingHeader( "Color Management" ) ) {
				ImGui::SeparatorText( "Basic" );
				ImGui::SliderFloat( "Exposure##post", &tonemap.postExposure, 0.0f, 5.0f );
				ImGui::SliderFloat( "Gamma", &tonemap.gamma, 0.0f, 3.0f );

				ImGui::SeparatorText( "Adjustments" );
				ImGui::SliderFloat( "Hue Shift", &tonemap.hueShift, -5.0f, 5.0f );
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
					"AgX",
					"LMS"
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

			// if ( ImGui::CollapsingHeader( "Bloom" ) ) {
			// 	ImGui::Text( "todo" );
			// 	ImGui::Text( " " );
			// }

			if ( ImGui::CollapsingHeader( "Lens Distort" ) ) {
				// // lens distortion
				ImGui::Checkbox( "Enable##lensDistort", &daedalusConfig.post.enableLensDistort );
				ImGui::Checkbox( "Normalize##lensDistort", &daedalusConfig.post.lensDistortNormalize );
				ImGui::Checkbox( "Chromatic Aberration##lensDistort", &daedalusConfig.post.lensDistortChromab );
				ImGui::SliderInt( "Samples##lensDistort", &daedalusConfig.post.lensDistortNumSamples, 3, 100 );
				ImGui::SliderFloat( "K1 Start", &daedalusConfig.post.lensDistortParametersStart.x, -0.1f, 0.1f );
				ImGui::SliderFloat( "K1 End", &daedalusConfig.post.lensDistortParametersEnd.x, -0.1f, 0.1f );
				ImGui::SliderFloat( "K2 Start", &daedalusConfig.post.lensDistortParametersStart.y, -0.1f, 0.1f );
				ImGui::SliderFloat( "K2 End", &daedalusConfig.post.lensDistortParametersEnd.y, -0.1f, 0.1f );
				ImGui::SliderFloat( "T1 Start", &daedalusConfig.post.lensDistortParametersStart.z, -0.1f, 0.1f );
				ImGui::SliderFloat( "T1 End", &daedalusConfig.post.lensDistortParametersEnd.z, -0.1f, 0.1f );
			}

			// if ( ImGui::CollapsingHeader( "Dithering" ) ) {
			// 	ImGui::Text( "todo" );
			// 	ImGui::Text( " " );
			// }

			if ( ImGui::Button( "Reset to Defaults" ) ) {
				TonemapDefaults(); // revisit how this works
			}

			ImGui::Unindent();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}