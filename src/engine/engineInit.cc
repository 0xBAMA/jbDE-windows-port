#include "engine.h"
#include "../utils/debug/debug.h"

void engineBase::StartMessage () {
	ZoneScoped;
	cout << endl << T_YELLOW << BOLD << "  jbDE - the jb Demo Engine" << newline;
	cout << " By Jon Baker ( 2020 - 2025 ) " << RESET << newline;
	cout << "  https://jbaker.graphics/ " << newline << newline;
}

void engineBase::LoadConfig () {
	ZoneScoped;

	{
		Block Start( "Configuring Application" );

		// load the config json, populate config struct - this will probably have more data, eventually
		json j; ifstream i( engineDir + "config.json" ); i >> j; i.close();
		config.windowTitle				= j[ "system" ][ "windowTitle" ];
		config.width					= j[ "system" ][ "screenWidth" ];
		config.height					= j[ "system" ][ "screenHeight" ];
		config.linearFilter				= j[ "system" ][ "linearFilterDisplayTex" ];
		config.windowOffset.x			= j[ "system" ][ "windowOffset" ][ "x" ];
		config.windowOffset.y			= j[ "system" ][ "windowOffset" ][ "y" ];
		config.startOnScreen			= j[ "system" ][ "startOnScreen" ];
		config.numMsDelayAfterCallback	= j[ "system" ][ "numMsDelayAfterCallback" ];
		config.severeCallsToKill		= j[ "system" ][ "severeCallsToKill" ];
		config.windowFlags				|= ( j[ "system" ][ "SDL_WINDOW_FULLSCREEN" ] ? SDL_WINDOW_FULLSCREEN : 0 );
		config.windowFlags				|= ( j[ "system" ][ "SDL_WINDOW_BORDERLESS" ] ? SDL_WINDOW_BORDERLESS : 0 );
		config.windowFlags				|= ( j[ "system" ][ "SDL_WINDOW_RESIZABLE" ] ? SDL_WINDOW_RESIZABLE : 0 );
		config.windowFlags				|= ( j[ "system" ][ "SDL_WINDOW_INPUT_GRABBED" ] ? SDL_WINDOW_MOUSE_GRABBED : 0 );
		config.vSyncEnable				= j[ "system" ][ "vSyncEnable" ];
		config.MSAACount				= j[ "system" ][ "MSAACount" ];
		config.OpenGLVersionMajor		= j[ "system" ][ "OpenGLVersionMajor" ];
		config.OpenGLVersionMinor		= j[ "system" ][ "OpenGLVersionMinor" ];
		config.OpenGLVerboseInit		= j[ "system" ][ "OpenGLVerboseInit" ];
		config.SDLDisplayModeDump		= j[ "system" ][ "SDLDisplayModeDump" ];
		config.reportPlatformInfo		= j[ "system" ][ "reportPlatformInfo" ];
		config.enableDepthTesting		= j[ "system" ][ "depthTesting" ];
		config.SRGBFramebuffer			= j[ "system" ][ "SRGBFramebuffer" ];
		config.loadDataResources		= j[ "system" ][ "loadDataResources" ];
		config.clearColor.r				= j[ "system" ][ "clearColor" ][ "r" ];
		config.clearColor.g				= j[ "system" ][ "clearColor" ][ "g" ];
		config.clearColor.b				= j[ "system" ][ "clearColor" ][ "b" ];
		config.clearColor.a				= j[ "system" ][ "clearColor" ][ "a" ];
		//config.allowMultipleViewports	= j[ "system" ][ "allowMultipleViewports" ];

		config.oneShot					= j[ "system" ][ "oneShot" ]; // relatively special purpose - run intialization, and one pass through the main loop before quitting
		showDemoWindow					= j[ "system" ][ "showImGUIDemoWindow" ];
		showProfiler					= j[ "system" ][ "showProfiler" ];

		TonemapDefaults();
	}
}

void engineBase::TonemapDefaults () {
	// color grading stuff
	json j; ifstream i( engineDir + "config.json" ); i >> j; i.close();
	tonemap.showTonemapWindow		= j[ "system" ][ "colorGrade" ][ "showTonemapWindow" ];
	tonemap.tonemapMode				= j[ "system" ][ "colorGrade" ][ "tonemapMode" ];
	tonemap.gamma					= j[ "system" ][ "colorGrade" ][ "gamma" ];
	tonemap.postExposure			= j[ "system" ][ "colorGrade" ][ "postExposure" ];
	tonemap.saturation				= j[ "system" ][ "colorGrade" ][ "saturation" ];
	tonemap.saturationImprovedWeights = j[ "system" ][ "colorGrade" ][ "saturationImprovedWeights" ];
	tonemap.colorTemp				= j[ "system" ][ "colorGrade" ][ "colorTemp" ];
}

void engineBase::CreateWindowAndContext () {
	ZoneScoped;

	window.config = &config;

	{
		Block Start( "Initializing SDL3" );
		window.PreInit();
	}

	{
		Block Start( "Creating Window" );
		window.Init();
	}

	{
		string StringWithVersionNumber = string( "Setting Up OpenGL " ) + string( to_string( window.config->OpenGLVersionMajor ) ) + string( "." ) + string( to_string( window.config->OpenGLVersionMinor ) ) + string( " Context" );
		Block Start( StringWithVersionNumber );
		window.OpenGLSetup();
	}

	// setup OpenGL debug callback with configured delay
	numMsDelayAfterCallback = config.numMsDelayAfterCallback;
	severeCallsToKill = config.severeCallsToKill;
	GLDebugEnable();
	cout << endl << T_YELLOW << "  OpenGL debug callback enabled - " << numMsDelayAfterCallback << "ms delay.";
	cout << endl << T_YELLOW << "    " << severeCallsToKill << " GL_DEBUG_SEVERITY_HIGH calls to kill." << RESET << endl << endl;
}

// split up into vertex, texture funcs + report platform info ( maybe do this later? )
void engineBase::DisplaySetup () {
	ZoneScoped;

	// I want to save some of this information off to a struct on the terminal, maybe add this same report to the terminal startup message

	// some info on your current platform
	if ( config.reportPlatformInfo ) {
		// report current platform
		cout << T_BLUE << "    Platform : " << T_CYAN << SDL_GetPlatform() << RESET << newline << newline;

		// CPU information
		cout << T_BLUE << "    CPU Info :" << RESET << newline;
		cout << T_RED << "      Logical Cores : " << T_CYAN << SDL_GetNumLogicalCPUCores() << newline;
		cout << T_RED << "      System RAM : " << T_CYAN << SDL_GetSystemRAM() << " MB" << newline;
		cout << newline;

		// GPU information from OpenGL
		const GLubyte *vendor = glGetString( GL_VENDOR );
		const GLubyte *renderer = glGetString( GL_RENDERER );
		const GLubyte *version = glGetString( GL_VERSION );
		const GLubyte *glslVersion = glGetString( GL_SHADING_LANGUAGE_VERSION );
		cout << T_BLUE << "    GPU Info :" << RESET << newline;
		cout << T_RED << "      Vendor : " << T_CYAN << vendor << RESET << newline;
		cout << T_RED << "      Renderer : " << T_CYAN << renderer << RESET << newline;
		cout << T_RED << "      OpenGL Version Supported : " << T_CYAN << version << RESET << newline;
		cout << T_RED << "      GLSL Version Supported : " << T_CYAN << glslVersion << RESET << newline << newline;

		// https://wiki.libsdl.org/SDL2/SDL_GetNumVideoDrivers
		// https://wiki.libsdl.org/SDL2/SDL_GetCurrentVideoDriver
	}

	if ( config.OpenGLVerboseInit ) {
		// report all gl extensions - useful on different platforms
		GLint n;
		glGetIntegerv( GL_NUM_EXTENSIONS, &n );
		cout << "starting dump of " << n << " extensions" << endl;
		for ( int i = 0; i < n; i++ )
		cout << "\t" << i << " : " << glGetStringi( GL_EXTENSIONS, i ) << endl;
		cout << endl;
	}

	if ( config.SDLDisplayModeDump ) {
		// will take some work to port this specific piece to SDL3

		//// https://wiki.libsdl.org/SDL2/SDL_GetDesktopDisplayMode
		//static int display_in_use = 0;
		//int i, display_mode_count;
		//SDL_DisplayMode mode;
		//Uint32 f;

		//SDL_Log( "SDL_GetNumVideoDisplays(): %i", SDL_GetNumVideoDisplays() );

		//display_mode_count = SDL_GetNumDisplayModes( display_in_use );
		//if ( display_mode_count < 1 ) {
		//	SDL_Log( "SDL_GetNumDisplayModes failed: %s", SDL_GetError() );
		//}
		//SDL_Log( "SDL_GetNumDisplayModes: %i", display_mode_count );

		//for ( i = 0; i < display_mode_count; ++i ) {
		//	if ( SDL_GetDisplayMode( display_in_use, i, &mode ) != 0 ) {
		//		SDL_Log( "SDL_GetDisplayMode failed: %s", SDL_GetError() );
		//	}
		//	f = mode.format;
		//	SDL_Log( "Mode %i\tbpp %i\t%s\t%i x %i",
		//		i, SDL_BITSPERPIXEL( f ),
		//		SDL_GetPixelFormatName( f ),
		//		mode.w, mode.h );
		//}
	}
}

void engineBase::SetupVertexData () {
	ZoneScoped;

	{
		Block Start( "Setting Up Vertex Data" );

		// OpenGL core spec requires a VAO bound when calling glDrawArrays
		glGenVertexArrays( 1, &displayVAO );
		glBindVertexArray( displayVAO );

		// corresponding VBO, unused by the default jbDE template
		glGenBuffers( 1, &displayVBO );
		glBindBuffer( GL_ARRAY_BUFFER, displayVBO );

		// no op, by default...
			// load models, setup vertex attributes, etc, here
	}
}

void engineBase::SetupTextureData () {
	ZoneScoped;

	{
		Block Start( "Setting Up Textures" );

		textureManager.Init();
		textureOptions_t opts;

	// =======================================================================
		// LDR tonemapped texture, ready for display
		opts.width = config.width;
		opts.height = config.height;
		opts.dataType = GL_RGBA8;
		opts.minFilter = ( config.linearFilter ? GL_LINEAR : GL_NEAREST );
		opts.magFilter = ( config.linearFilter ? GL_LINEAR : GL_NEAREST );
		opts.textureType = GL_TEXTURE_2D;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = nullptr;

		textureManager.Add( "Display Texture", opts );
	// =======================================================================

	// =======================================================================
		// holds renderer state at high bit depth
		opts.width = config.width;
		opts.height = config.height;
		opts.dataType = GL_RGBA16F;
		opts.minFilter = GL_NEAREST;
		opts.magFilter = GL_NEAREST;
		opts.textureType = GL_TEXTURE_2D;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = nullptr;

		textureManager.Add( "Accumulator", opts );
	// =======================================================================

	// =======================================================================
		// blue noise texture, used for various purposes
		Image_4U blueNoiseImage{ "../src/utils/noise/blueNoise.png" };
		opts.width = blueNoiseImage.Width();
		opts.height = blueNoiseImage.Height();
		opts.dataType = GL_RGBA8UI;
		opts.minFilter = GL_NEAREST;
		opts.magFilter = GL_NEAREST;
		opts.textureType = GL_TEXTURE_2D;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = ( void * ) blueNoiseImage.GetImageDataBasePtr();

		textureManager.Add( "Blue Noise", opts );
	// =======================================================================

	// =======================================================================
		// add textures for the text renderer + access to the manager
		textRenderer.textureManager_local = &textureManager;
		textRenderer.Init( config.width, config.height, shaders[ "Font Renderer" ] );

		// font atlas texture
		Image_4U fontAtlas( "../src/utils/fonts/fontRenderer/whiteOnClear.png" );
		fontAtlas.FlipVertical();
		opts.width = fontAtlas.Width();
		opts.height = fontAtlas.Height();
		opts.dataType = GL_RGBA8UI;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = ( void * ) fontAtlas.GetImageDataBasePtr();

		// font renderer data textures created internal to each layer via textureManager_local
		textureManager.Add( "Font Atlas", opts );
	// =======================================================================

	// =======================================================================
		// image buffer for the trident orientation indicator
		opts.width = trident.blockDimensions.x * 8;
		opts.height = trident.blockDimensions.y * 16;
		opts.minFilter = GL_NEAREST;
		opts.magFilter = GL_NEAREST;

		textureManager.Add( "Trident Storage", opts );
		trident.PassInImage( textureManager.Get( "Trident Storage" ) );
		trident.basePt = textRenderer.basePt; // location to draw at
	// =======================================================================

	// =======================================================================
		// bayer patterns todo - how important is it that they're loaded at startup?
	// =======================================================================

	}

	{
		Block Start( "Setting Up Bindsets" );

		bindSets[ "Drawing" ] = bindSet( {
			binding( 0, textureManager.Get( "Blue Noise" ), GL_RGBA8UI ),
			binding( 1, textureManager.Get( "Accumulator" ), GL_RGBA16F )
		} );

		bindSets[ "Postprocessing" ] = bindSet( {
			binding( 0, textureManager.Get( "Accumulator" ), GL_RGBA16F ),
			binding( 1, textureManager.Get( "Display Texture" ), GL_RGBA8UI ),
			binding( 2, textureManager.Get( "Blue Noise" ), GL_RGBA8UI )
		} );

		bindSets[ "Display" ] = bindSet( {
			binding( 0, textureManager.Get( "Display Texture" ), GL_RGBA8UI )
		} );
	}
}

void engineBase::LoadData () {
	ZoneScoped;

	if ( config.loadDataResources ) { // toggle loading of palettes, font glyphs, and bad/color wordlists
		{
			Block Start( "Loading Palettes" );

			LoadPalettes( paletteList );
			palette::PopulateLocalList( paletteList );
			// cout << "loaded " << paletteList.size() << " palettes" << newline;
		}

		{
			Block Start( "Loading Font Glyphs" );

			LoadGlyphs( glyphList );
			// cout << "loaded " << glyphList.size() << " glyphs" << newline;
		}

		{
			Block Start( "Load Wordlists" );

			LoadBadWords( badWords );
			// cout << "loaded " << badWords.size() << " bad words" << newline;

			LoadColorWords( colorWords );
			// cout << "loaded " << colorWords.size() << " color words" << newline;

			/* plantWords, animalWords, toolWords, etc? tbd */
		}
	} else {
		cout << endl << T_RED << " User Has Elected to Skip Loading of Data Resources ( Palettes, WordLists ) " << RESET << endl;
		cout << T_RED << "  Check Value of " << T_YELLOW << "config.loadDataResources" << T_RED << " to Change This Behavior" << RESET << endl << endl;
	}
}

void engineBase::TerminalSetup () {
	ZoneScoped;
	{
		Block Start( "Terminal Setup" );

	// this is where system commands live... quit is the big one, but maybe also eventually screenshots by string label

		terminal.addCommand( { "quit" }, {},
			[=] ( args_t args ) {
				pQuit = true;
			}, "Quit the engine." );


		// texture usage report, with some extras that I wanted anyways
		terminal.addCommand( { "textureManagerReport" }, {},
		[=] ( args_t args ) {
			

		// reporting the number of active textures, as well as the total number of bytes used

		// then we want to give a list like this one...

			// I want more info here:
				// dimensions
				// data type + number of bits

			// precompute layout constants
			int maxWidth = 0;
			int dataWidth = 0;
			for ( auto& tex : textureManager.textures ) {
				maxWidth = std::max( maxWidth, int( GetWithThousandsSeparator( tex.textureSize ).length() ) );
				dataWidth = std::max( dataWidth, int( getTypeString( tex.creationOptions.dataType ).length() ) );
			}
			// also consider the total
			maxWidth = std::max( maxWidth, int( GetWithThousandsSeparator( textureManager.TotalSize() ).length() ) );

			// give overal usage stats
			const size_t bytes = textureManager.TotalSize();
			stringstream byteString;
			if ( bytes < ( 1u << 30 ) ) { // less than 1 billion bytes
				byteString << std::setprecision( 4 ) << float( bytes ) / float( 1u << 20 ) << "MB";
			} else { // otherwise we are into gigabytes
				byteString << std::setprecision( 4 ) << float( bytes ) / float( 1u << 30 ) << "GB";
			}

			terminal.addLineBreak();
			terminal.addHistoryLine( terminal.csb.append( "Texture Manager Report ", 3 ).flush() );
			terminal.addHistoryLine( terminal.csb.append( "[ Summary: ", GREY_DD ).append( to_string( textureManager.Count() ) ).append( " textures in ", GREY_DD ).append( byteString.str() ).append( " total ]", GREY_DD ).flush() );

			// divider...
			terminal.csb.append( cChar( GREY_DD, ( char ) 218 ) );
			terminal.csb.append( string( 3, ( char ) 196 ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 194 ) );
			terminal.csb.append( string( 4, ( char ) 196 ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 194 ) );
			terminal.csb.append( string( 9, ( char ) 196 ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 194 ) );
			terminal.csb.append( string( 12, ( char ) 196 ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 194 ) );
			terminal.csb.append( string( 65, ( char ) 196 ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 191 ) );

			terminal.addHistoryLine( terminal.csb.flush() );

			// table headings
			terminal.csb.append( cChar( GREY_DD, VERTICAL_SINGLE ) );
			terminal.csb.append( "ID " );
			terminal.csb.append( cChar( GREY_DD, VERTICAL_SINGLE ) );
			terminal.csb.append( "TYPE" );
			terminal.csb.append( cChar( GREY_DD, VERTICAL_SINGLE ) );
			terminal.csb.append( "CHANNELS " );
			terminal.csb.append( cChar( GREY_DD, VERTICAL_SINGLE ) );
			terminal.csb.append( "FORMAT      " );
			terminal.csb.append( cChar( GREY_DD, VERTICAL_SINGLE ) );
			terminal.csb.append( "LABEL" + string( 60, ' ' ) );
			terminal.csb.append( cChar( GREY_DD, VERTICAL_SINGLE ) );

			terminal.addHistoryLine( terminal.csb.flush() );

			// divider
			terminal.csb.append( cChar( GREY_DD, TEE_UDR ) );
			terminal.csb.append( string( 3, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 216 ) );
			terminal.csb.append( string( 4, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 216 ) );
			terminal.csb.append( string( 9, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 216 ) );
			terminal.csb.append( string( 12, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 216 ) );
			terminal.csb.append( string( 65, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 190 ) );

			terminal.addHistoryLine( terminal.csb.flush() );

			for ( auto& tex : textureManager.textures ) { // the report for each one should only take up one line....

				// adding the texture handle
				// terminal.csb.append( " [", GREY_DD ).append( fixedWidthNumberString( tex.textureHandle, 3, '0' ), 1 ).append( "] ", GREY_DD );

				string idString = to_string( tex.textureHandle );
				terminal.csb.append( cChar( GREY_DD, VERTICAL_SINGLE ) );
				// terminal.csb.append( string( 3 - idString.length(), '0' ), 1 );
				// terminal.csb.append( idString, 2 );

				terminal.csb.append( string( 3 - idString.length(), '0' ), TERMBG );
				terminal.csb.append( idString, TERMFG );

				// two characters indicating the type of texture
				string typeString;

				if ( tex.creationOptions.textureType == GL_TEXTURE_1D ) {
					typeString = "1D  ";
				}
				if ( tex.creationOptions.textureType == GL_TEXTURE_2D_ARRAY ) {
					typeString = "2DAr";
				}
				if ( tex.creationOptions.textureType == GL_TEXTURE_2D ) {
					typeString = "2D  ";
				}
				if ( tex.creationOptions.textureType == GL_TEXTURE_3D ) {
					typeString = "3D  ";
				}

				GLenum thisTextureFormat = getFormat( tex.creationOptions.dataType );
				bvec4 flags = bvec4( false );

				// if the texture is a depth texture, just none of these will be turned on... I don't like that a lot, but it's alright

				if ( thisTextureFormat == GL_RED || thisTextureFormat == GL_RED_INTEGER ) {
					flags.r = true;
				}
				if ( thisTextureFormat == GL_RG || thisTextureFormat == GL_RG_INTEGER ) {
					flags.r = true;
					flags.g = true;
				}
				if ( thisTextureFormat == GL_RGB || thisTextureFormat == GL_RGB_INTEGER ) {
					flags.r = true;
					flags.g = true;
					flags.b = true;
				}
				if ( thisTextureFormat == GL_RGBA || thisTextureFormat == GL_RGBA_INTEGER ) {
					flags.r = true;
					flags.g = true;
					flags.b = true;
					flags.a = true;
				}

				string dataTypeString = getTypeString( tex.creationOptions.dataType );
				string bitString = to_string( getBitCount( tex.creationOptions.dataType ) );
				bitString = string( 2 - bitString.length(), ' ' ) + bitString + "-bit"; // make constant width

				terminal.csb.append( cChar( GREY_DD, VERTICAL_SINGLE ) )
					.append( typeString )
					.append( cChar( GREY_DD, VERTICAL_SINGLE ) )
					.append( " R ", flags.r ? RED : GREY_DD )
					.append( "G ", flags.g ? GREEN : GREY_DD )
					.append( "B ", flags.b ? BLUE : GREY_DD )
					.append( "A ", flags.a ? WHITE : GREY_DD )
					.append( cChar( GREY_DD, VERTICAL_SINGLE ) )
					.append( bitString )
					.append( " " )
					.append( dataTypeString + string( dataWidth - dataTypeString.length(), ' ' ) )
					.append( cChar( GREY_DD, VERTICAL_SINGLE ) );

				// appending the texture label, and the trailing
				stringstream ss;
				ss << tex.label;
				string fill = string( 50 - ss.str().length(), '.' );
				terminal.csb.append( ss.str(), 2 ).append( fill, 3 );

				// now the number of bytes taken up by this texture
				fill = string( maxWidth - GetWithThousandsSeparator( tex.textureSize ).length(), '.' );

				terminal.csb.append( fill, 3 ).append( GetWithThousandsSeparator( tex.textureSize ) ).append( " bytes", 2 );
				terminal.addHistoryLine( terminal.csb.flush() );
			}

			// output the separator, up to the total
			terminal.csb.append( cChar( GREY_DD, ( char ) 212 ) );
			terminal.csb.append( string( 3, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 207 ) );
			terminal.csb.append( string( 4, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 207 ) );
			terminal.csb.append( string( 9, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 207 ) );
			terminal.csb.append( string( 12, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.csb.append( cChar( GREY_DD, ( char ) 207 ) );
			terminal.csb.append( string( 41, HORIZONTAL_DOUBLE ), GREY_DD );
			terminal.addHistoryLine( terminal.csb.append( " Total:  " ).append( GetWithThousandsSeparator( bytes ) ).append( " bytes", 2 ).flush() );

			terminal.addLineBreak();
		}, "Give the texture manager usage report." );

		// screenshots? not sure what exactly that's going to look like yet

		// more...?

	}
}

void engineBase::ShaderCompile () {
	ZoneScoped;

	{
		Block Start( "Compiling Shaders" );

		const string basePath( engineDir + "shaders/" );

		// tonemapping shader - this will need some refinement as time goes on
		std::system( "dir" );
		shaders[ "Tonemap" ] = computeShader( basePath + "tonemap.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Tonemap" ], -1, string( "Tonemap" ).c_str() );

		// create the shader for the triangles to cover the screen
		shaders[ "Display" ] = regularShader( basePath + "blit.vs.glsl", basePath + "blit.fs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Display" ], -1, string( "Display" ).c_str() );

		// initialize the text renderer
		shaders[ "Font Renderer" ] = computeShader( "../src/utils/fonts/fontRenderer/font.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Font Renderer" ], -1, string( "Font Renderer" ).c_str() );

		// trident shaders
		shaders[ "Trident Raymarch" ] = computeShader( "../src/utils/trident/tridentGenerate.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Trident Raymarch" ], -1, string( "Trident Raymarch" ).c_str() );
		shaders[ "Trident Blit" ] = computeShader( "../src/utils/trident/tridentCopy.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Trident Blit" ], -1, string( "Trident Blit" ).c_str() );
		trident.PassInShaders( shaders[ "Trident Raymarch" ], shaders[ "Trident Blit" ] );
	}
}

void engineBase::ImguiSetup () {
	ZoneScoped;

	{
		Block Start( "Configuring dearImGUI" );

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();

		// docking/multi viewport stuff pending, not important right now

		// enable docking
		//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		//if ( config.allowMultipleViewports )
		//	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		// Setup Platform/Renderer bindings
		ImGui_ImplSDL3_InitForOpenGL( window.window, window.GLcontext );
		const char *glsl_version = "#version 450";
		ImGui_ImplOpenGL3_Init( glsl_version );

		// setting custom font, if desired
		// io.Fonts->AddFontFromFileTTF( "src/fonts/star_trek/titles/TNG_Title.ttf", 16 );

		ImVec4 *colors = ImGui::GetStyle().Colors;
		colors[ ImGuiCol_Text ]					= ImVec4( 0.51f, 0.48f, 0.48f, 1.00f );
		colors[ ImGuiCol_TextDisabled ]			= ImVec4( 0.28f, 0.28f, 0.28f, 1.00f );
		colors[ ImGuiCol_WindowBg ]				= ImVec4( 0.00f, 0.00f, 0.00f, 0.78f );
		colors[ ImGuiCol_ChildBg ]				= ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
		colors[ ImGuiCol_PopupBg ]				= ImVec4( 0.13f, 0.13f, 0.13f, 0.94f );
		colors[ ImGuiCol_Border ]				= ImVec4( 0.66f, 0.40f, 0.00f, 0.50f );
		colors[ ImGuiCol_BorderShadow ]			= ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
		colors[ ImGuiCol_FrameBg ]				= ImVec4( 0.08f, 0.08f, 0.08f, 1.00f );
		colors[ ImGuiCol_FrameBgHovered ]		= ImVec4( 0.19f, 0.19f, 0.19f, 1.00f );
		colors[ ImGuiCol_FrameBgActive ]		= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
		colors[ ImGuiCol_TitleBg ]				= ImVec4( 0.14f, 0.07f, 0.00f, 1.00f );
		colors[ ImGuiCol_TitleBgActive ]		= ImVec4( 0.24f, 0.12f, 0.00f, 1.00f );
		colors[ ImGuiCol_TitleBgCollapsed ]		= ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
		colors[ ImGuiCol_MenuBarBg ]			= ImVec4( 0.06f, 0.02f, 0.00f, 1.00f );
		colors[ ImGuiCol_ScrollbarBg ]			= ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
		colors[ ImGuiCol_ScrollbarGrab ]		= ImVec4( 0.19f, 0.19f, 0.19f, 1.00f );
		colors[ ImGuiCol_ScrollbarGrabHovered ]	= ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
		colors[ ImGuiCol_ScrollbarGrabActive ]	= ImVec4( 0.66f, 0.40f, 0.00f, 0.50f );
		colors[ ImGuiCol_CheckMark ]			= ImVec4( 0.24f, 0.12f, 0.00f, 1.00f );
		colors[ ImGuiCol_SliderGrab ]			= ImVec4( 0.24f, 0.12f, 0.00f, 1.00f );
		colors[ ImGuiCol_SliderGrabActive ]		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_Button ]				= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_ButtonHovered ]		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_ButtonActive ]			= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_Header ]				= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_HeaderHovered ]		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_HeaderActive ]			= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_Separator ]			= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_SeparatorHovered ]		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_SeparatorActive ]		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_ResizeGrip ]			= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_ResizeGripHovered ]	= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_ResizeGripActive ]		= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_Tab ]					= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_TabHovered ]			= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_TabActive ]			= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_TabUnfocused ]			= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_TabUnfocusedActive ]	= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		//colors[ ImGuiCol_DockingPreview ]		= ImVec4( 0.26f, 0.59f, 0.98f, 0.70f );
		//colors[ ImGuiCol_DockingEmptyBg ]		= ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
		colors[ ImGuiCol_PlotLines ]			= ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
		colors[ ImGuiCol_PlotLinesHovered ]		= ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
		colors[ ImGuiCol_PlotHistogram ]		= ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
		colors[ ImGuiCol_PlotHistogramHovered ]	= ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
		colors[ ImGuiCol_TableHeaderBg ]		= ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
		colors[ ImGuiCol_TableBorderStrong ]	= ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );
		colors[ ImGuiCol_TableBorderLight ]		= ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );
		colors[ ImGuiCol_TableRowBg ]			= ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
		colors[ ImGuiCol_TableRowBgAlt ]		= ImVec4( 1.00f, 1.00f, 1.00f, 0.06f );
		colors[ ImGuiCol_TextSelectedBg ]		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_DragDropTarget ]		= ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
		colors[ ImGuiCol_NavHighlight ]			= ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
		colors[ ImGuiCol_NavWindowingHighlight ]= ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
		colors[ ImGuiCol_NavWindowingDimBg ]	= ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
		colors[ ImGuiCol_ModalWindowDimBg ]		= ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
	}
}

void engineBase::InitialClear () {
	ZoneScoped;

	{
		Block Start( "Clear Buffer" );
		glClearColor( config.clearColor.x, config.clearColor.y, config.clearColor.z, config.clearColor.w );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		window.Swap();
	}
}

void engineBase::ReportStartupStats () {
	ZoneScoped;

	cout << endl << T_CYAN << "  " <<
		shaders.size() << " shader programs, " <<
		bindSets.size() << " bindsets" << endl;

	const size_t bytes = textureManager.TotalSize();
	cout << "  " << textureManager.Count() << " textures " << float( bytes ) / float( 1u << 20 ) << "MB ( " << GetWithThousandsSeparator( bytes ) << " bytes )" << endl;
	cout << T_YELLOW << "  Startup is complete ( total " << TotalTime() << " ms )" << RESET << endl << endl;

	// texture setup report
	textureManager.EnumerateTextures();
}
