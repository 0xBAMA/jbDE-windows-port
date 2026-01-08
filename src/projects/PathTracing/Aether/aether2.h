public:
	void RecompileShaders() {
		// called on init and at runtime if needed
	}

	// engine resource access
	textureManager_t * textureManager;
	std::unordered_map< string, GLuint >* shaders;
	orientTrident* trident;

	void Init ( textureManager_t* textureManager_in, std::unordered_map< string, GLuint >* shaders_in, orientTrident* trident_in ) {
		// cache resource handles
		textureManager = textureManager_in;
		shaders = shaders_in;
		trident = trident_in;

	// intialize...
		// load all data resources
		LoadGelFilterData();
		LoadPDFData();

		// compile shaders
		// create textures

		// ================================================================================================================
		{
			// will be more later... currently just visualizing the filtered light PDF
			textureOptions_t opts;
			opts.dataType = GL_RGBA8;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.width = 450;
			opts.height = 256;
			opts.textureType = GL_TEXTURE_2D;

			textureManager->Add( "Filtered PDF Preview", opts );
		}

		// simple testing
		AddLight();
		ComputeLightStack( 0 );

		// create SSBOs

	}

	void Update ( /* some kind of parameter to scale workload */ ) {
		// update the forward light transport simulation
	}

	void Draw () {
		// draw the image into the accumulator
	}

	void ImGuiMenu () {
		ImGui::Begin( "GelFilter" );

		// we need to eventually show the importance sampling structure up top

		// for now we're going to look at just the light config list (one light):
		// curve texture preview
		ImGui::Text( "" );
		const int w = ImGui::GetContentRegionAvail().x;
		ImGui::Image( ( ImTextureID ) ( void * ) intptr_t( textureManager->Get( "Filtered PDF Preview" ) ), ImVec2( w, w * ( 64.0f ) / ( 450.0f ) ) );
		ImGui::Text( "" );

		bool needsUpdate = false;
		// ImGui::PushID(  );

		// source distribution picker
		ImGui::Combo( ( string( "Light Type" ) ).c_str(), &lightList[ 0 ].sourcePDF, sourcePDFLabels, numSourcePDFs );
		needsUpdate |= ImGui::IsItemEdited();



		// for gels
		for ( int i = 0; i < lightList[ 0 ].gelStack.size(); i++ ) {
			string iString = to_string( i );

			ImGui::Separator();

			// for removing gels
			int flaggedForRemoval = -1;

			// show gel picker
			ImGui::Combo( ( "Gel##" + iString ).c_str(), &lightList[ 0 ].gelStack[ i ], gelFilterLabels, numGelFilters );
			needsUpdate |= ImGui::IsItemEdited();

			ImGui::SameLine();
			if ( ImGui::Button( ( "Randomize##" + iString  ).c_str() ) ) {
				rngi gelPick = rngi( 0, numGelFilters - 1 );
				lightList[ 0 ].gelStack[ i ] = gelPick();
				needsUpdate = true;
			}

			ImGui::SameLine();
			if ( ImGui::Button( ( "Remove##" + iString  ).c_str() ) ) {
				flaggedForRemoval = i;
				needsUpdate = true;
			}

			// show selected gel preview color
			vec3 col = gelPreviewColors[ lightList[ 0 ].gelStack[ i ] ];
			vec4 sRGB = vec4( col[ 0 ], col[ 1 ], col[ 2 ], 255 );
			bvec4 cutoff = lessThan( sRGB, vec4( 0.04045f ) );
			vec4 higher = pow( ( sRGB + vec4( 0.055f ) ) / vec4( 1.055f ), vec4( 2.4f ) );
			vec4 lower = sRGB / vec4( 12.92f );
			vec4 r =  mix( higher, lower, cutoff );
			if ( ImGui::ColorButton( ( "##ColorSquare" + iString ).c_str(), ImColor( r.r, r.g, r.b ), ImGuiColorEditFlags_NoAlpha, ImVec2(16, 16 ) ) ) {}
			ImGui::SameLine();

			// show selected gel description
			// ImGui::SameLine();
			ImGui::TextWrapped( "%s", gelFilterDescriptions[ lightList[ 0 ].gelStack[ i ] ] );
			ImGui::PopID();
		}

		// button to add a new gel to the stack
		if ( ImGui::Button( "Add Gel" ) ) {
			lightList[ 0 ].gelStack.emplace_back();
			needsUpdate = true;
		}

		if ( needsUpdate ) {
			ComputeLightStack( 0 );
		}

		ImGui::End();
	}
};