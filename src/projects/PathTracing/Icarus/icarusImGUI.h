void IcarusImguiWindow ( icarusState_t &state ) {
	ImGui::Begin( "Icarus", NULL );

	ImGui::SeparatorText( "Render" );
	ImGui::SliderInt( "Max Bounces", ( int * ) &state.maxBounces, 0, 256 );
	ImGui::Text( "Intersectors:" );
	ImGui::Checkbox( "SDF", &state.runSDF );
	ImGui::Checkbox( "Triangle", &state.runTriangle );
	ImGui::Checkbox( "BVH", &state.runBVH );
	ImGui::Checkbox( "Volume", &state.runVolume );

	const char * resolveModes[] = { "UNIFORM", "GAUSSIAN", "FOCUSED", "SHUFFLED", "SHUFFOCUS", "FLEXTILE" };
	ImGui::Combo( "Offset Feed Mode", &state.offsetFeedMode, resolveModes, IM_ARRAYSIZE( resolveModes ) );

	if ( state.offsetFeedMode == FLEXTILE ) {
		ImGui::SliderInt( "Tile Size", &state.flexTileConfig.tileSize, 1, 512 );
		state.flexTileConfig.dirty |= ImGui::IsItemEdited();
		ImGui::SliderInt( "Samples", &state.flexTileConfig.samplesPerPixel, 1, 1024 );
		state.flexTileConfig.dirty |= ImGui::IsItemEdited();

		const char * tileOrderModes[] = { "In Order", "Shuffled" };
		ImGui::Combo( "Tile Feed Mode", ( int * ) &state.flexTileConfig.tileOrder, tileOrderModes, IM_ARRAYSIZE( tileOrderModes ) );
		state.flexTileConfig.dirty |= ImGui::IsItemEdited();

		const char * fillModes[] = { "In Order", "Shuffled", "Bayer" };
		ImGui::Combo( "Tile Fill Mode", ( int * ) &state.flexTileConfig.fillMode, fillModes, IM_ARRAYSIZE( fillModes ) );
		state.flexTileConfig.dirty |= ImGui::IsItemEdited();
	}

	ImGui::SeparatorText( "Camera" );
	ImGui::SliderFloat( "FoV", &state.FoV, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
	ImGui::SliderFloat( "UV Scalar", &state.uvScaleFactor, 0.0f, 5.0f, "%.3f", ImGuiSliderFlags_Logarithmic );

	const char * cameraModeNames[] = { "DEFAULT", "ORTHO", "VORALDO", "SPHERICAL", "HDRI", "COMPOUND" };
	ImGui::Combo( "Camera Mode", &state.cameraMode, cameraModeNames, IM_ARRAYSIZE( cameraModeNames ) );

	const char * bokehModeNames[] = { "NONE", "EDGE BIASED DISK", "UNIFORM DISK", "REJECTION SAMPLED HEXAGON", "UNIFORM SAMPLED HEXAGON", "UNIFORM HEART", "THREE BLADE ROSETTE", "FIVE BLADE ROSETTE", "RING", "PENTAGON", "SEPTAGON", "OCTAGON", "NONAGON", "DECAGON", "11-GON", "5 SIDED STAR", "6 SIDED STAR", "7 SIDED STAR" };
	ImGui::Combo( "Bokeh Mode", &state.DoFBokehMode, bokehModeNames, IM_ARRAYSIZE( bokehModeNames ) );

	ImGui::SliderFloat( "DoF Radius", &state.DoFRadius, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
	ImGui::SliderFloat( "DoF Distance", &state.DoFFocusDistance, 0.0f, 30.0f, "%.3f", ImGuiSliderFlags_Logarithmic );

	ImGui::SliderFloat( "Chromab Scale", &state.chromabScaleFactor, 0.0f, 0.1f, "%.6f", ImGuiSliderFlags_Logarithmic );

	ImGui::SeparatorText( "Resolution" );
	static int x = state.dimensions.x, y = state.dimensions.y;
	ImGui::SliderInt( "X", &x, 0, 8000 );
	ImGui::SliderInt( "Y", &y, 0, 8000 );
	if ( ImGui::Button( "Set" ) ) {
		state.forceUpdate = true;
		state.dimensions.x = x;
		state.dimensions.y = y;
		AllocateTextures( state );
	}

	ImGui::Text( "Presets:" );
	ImGui::Indent();
	ImGui::Text( "Standard" );
	if ( ImGui::SmallButton( " Preview " ) ){
		state.forceUpdate = true;
		state.dimensions.x = 160, state.dimensions.y = 90;
		AllocateTextures( state );
	}
	ImGui::SameLine();
	if ( ImGui::SmallButton( " 360p " ) ) {
		state.forceUpdate = true;
		state.dimensions.x = 640, state.dimensions.y = 360;
		AllocateTextures( state );
	}
	ImGui::SameLine();
	if ( ImGui::SmallButton( " 720p " ) ) {
		state.forceUpdate = true;
		state.dimensions.x = 1280, state.dimensions.y = 720;
		AllocateTextures( state );
	}
	ImGui::SameLine();
	if ( ImGui::SmallButton( " 1080p " ) ){
		state.forceUpdate = true;
		state.dimensions.x = 1920, state.dimensions.y = 1080;
		AllocateTextures( state );
	}
	ImGui::SameLine();
	if ( ImGui::SmallButton( " 4K " ) )	{
		state.forceUpdate = true;
		state.dimensions.x = 3840, state.dimensions.y = 2160;
		AllocateTextures( state );
	}
	ImGui::Unindent();
	ImGui::Indent();
	ImGui::Text( "Ultrawide" );
	if ( ImGui::SmallButton( " Preview ##ultrawide" ) ){
		state.forceUpdate = true;
		state.dimensions.x = 768, state.dimensions.y = 144;
		AllocateTextures( state );
	}
	ImGui::SameLine();
	if ( ImGui::SmallButton( " Wallpaper ##ultrawide" ) ){
		state.forceUpdate = true;
		state.dimensions.x = 7680, state.dimensions.y = 1440;
		AllocateTextures( state );
	}
	ImGui::Unindent();
	ImGui::Indent();
	ImGui::Text( "HDRI" ); // ouroboros
	if ( ImGui::SmallButton( " Preview ##hdri" ) ){
		state.forceUpdate = true;
		state.dimensions.x = 256, state.dimensions.y = 128;
		AllocateTextures( state );
	}
	ImGui::SameLine();
	if ( ImGui::SmallButton( " 4k ##hdri" ) ){
		state.forceUpdate = true;
		state.dimensions.x = 4096, state.dimensions.y = 2048;
		AllocateTextures( state );
	}
	ImGui::Unindent();
	ImGui::End();
}