
	// memory associated with the xRite color chip reflectances
	const float** xRiteReflectances = nullptr;
	void PrecomputesRGBReflectances () {
		// populating the xRite color checker card
		const vec3 sRGBConstants[] = {
			vec3( 115,  82,  68 ), // dark skin
			vec3( 194, 150, 120 ), // light skin
			vec3(  98, 122, 157 ), // blue sky
			vec3(  87, 108,  67 ), // foliage
			vec3( 133, 128, 177 ), // blue flower
			vec3( 103, 189, 170 ), // bluish green
			vec3( 214, 126,  44 ), // orange
			vec3(  80,  91, 166 ), // purplish blue
			vec3( 193,  90,  99 ), // moderate red
			vec3(  94,  60, 108 ), // purple
			vec3( 157, 188,  64 ), // yellow green
			vec3( 244, 163,  46 ), // orange yellow
			vec3(  56,  61, 150 ), // blue
			vec3(  70, 148,  73 ), // green
			vec3( 175,  54,  60 ), // red
			vec3( 231, 199,  31 ), // yellow
			vec3( 187,  86, 149 ), // magenta
			vec3(   8, 133, 161 ), // cyan
			vec3( 243, 243, 242 ), // white
			vec3( 200, 200, 200 ), // neutral 8
			vec3( 160, 160, 160 ), // neutral 6.5
			vec3( 122, 122, 121 ), // neutral 5
			vec3(  85,  85,  85 ), // neutral 3.5
			vec3(  52,  52,  52 ) // black
		};

	// there is some resources associated with this sampling process...
		// we first need to load the LUT from Jakob's paper
		// https://rgl.epfl.ch/publications/Jakob2019Spectral
		RGB2Spec *model = rgb2spec_load( "../src/data/Jakob2019Spectral/supplement/tables/srgb.coeff" );

		// once we have that, we can use an sRGB constant to derive a reflectance curve
			// we are going to do that by 1nm bands to match the other data
		xRiteReflectances = ( const float ** ) malloc( 24 * sizeof( float * ) );

		// for each of the reflectances
		for ( int i = 0; i < 24; i++ ) {
			// first step get the reflectance coefficients
			float rgb[ 3 ] = { sRGBConstants[ i ].r / 255.0f, sRGBConstants[ i ].g / 255.0f, sRGBConstants[ i ].b / 255.0f }, coeff[ 3 ];

			rgb2spec_fetch( model, rgb, coeff );
			// printf( "fetch(): %f %f %f\n", coeff[ 0 ], coeff[ 1 ], coeff[ 2 ] );

			// allocate and populate the reflectance corresponding to this color chip
			xRiteReflectances[ i ] = ( const float * ) malloc( 450 * sizeof( float ) );
			for ( int l = 0; l < 450; l++ ) {
				( float& ) xRiteReflectances[ i ][ l ] = rgb2spec_eval_precise( coeff, float( l + 380 ) );
			}
		}
	}
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

		// precompute reflectance curves
		PrecomputesRGBReflectances();

		// compile shaders
		// create textures

		// ================================================================================================================
		{
			// will be more later... currently just visualizing the filtered light PDF
			textureOptions_t opts;
			opts.dataType = GL_RGBA8;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.width = 450 + 104;
			opts.height = 64;
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
		ImGui::Image( ( ImTextureID ) ( void * ) intptr_t( textureManager->Get( "Filtered PDF Preview" ) ), ImVec2( w, w * ( 64.0f ) / ( 450.0f + 104 ) ) );
		ImGui::Text( "" );

		bool needsUpdate = false;
		// ImGui::PushID(  );

		// source distribution picker
		ImGui::Combo( ( string( "Light Type" ) ).c_str(), &lightList[ 0 ].sourcePDF, sourcePDFLabels, numSourcePDFs );
		needsUpdate |= ImGui::IsItemEdited();

		ImGui::SameLine();
		if ( ImGui::Button( "Randomize" ) ) {
			RandomizeLight( 0 );
		}

		// for gels
		for ( int i = 0; i < lightList[ 0 ].gelStack.size(); i++ ) {
			string iString = to_string( i );

			ImGui::Separator();

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
				lightList[ 0 ].gelStack.erase( lightList[ 0 ].gelStack.begin() + i );
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
			// ImGui::PopID();
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