#include "../../engine/engine.h"
#include "operations.h"

class ifs final : public engineBase { // sample derived from base engine class
public:
	ifs () { Init(); OnInit(); PostInit(); }
	~ifs () { Quit(); }

	GLuint maxBuffer;
	GLuint dataBuffer;

	// IFS view parameters
	float scale = 1.0f;
	vec2 offset = vec2( 0.0f );

	// output prep
	float brightness = 100.0f;
	float brightnessPower = 1.0f;
	int screenshotRequested = 0;

	// flag for field wipe (on zoom, drag, etc)
	bool RendererNeedsReset = false;

	// flag for SSBO update
	bool BufferNeedsReset = false;

	// operations list
	std::vector< operation_t > currentOperations;
	int initMode = 0;

	void LoadShaders() {
		shaders[ "Draw" ] = computeShader( "../src/projects/IFS/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "Update" ] = computeShader( "../src/projects/IFS/shaders/update.cs.glsl" ).shaderHandle;
		shaders[ "Clear" ] = computeShader( "../src/projects/IFS/shaders/clear.cs.glsl" ).shaderHandle;

		glObjectLabel( GL_PROGRAM, shaders[ "Draw" ], -1, string( "Draw" ).c_str() );
		glObjectLabel( GL_PROGRAM, shaders[ "Update" ], -1, string( "Update" ).c_str() );
		glObjectLabel( GL_PROGRAM, shaders[ "Clear" ], -1, string( "Clear" ).c_str() );
	}

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			LoadShaders();

			textureOptions_t opts;
			opts.width = config.width;
			opts.height = config.height;
			opts.dataType = GL_R32UI;
			opts.textureType = GL_TEXTURE_2D;
			textureManager.Add( "IFS Accumulator R", opts );
			textureManager.Add( "IFS Accumulator G", opts );
			textureManager.Add( "IFS Accumulator B", opts );

			// buffer holding the normalize factor
			glGenBuffers( 1, &maxBuffer );
			constexpr uint32_t countValue = 0;
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, maxBuffer );

			// setup labels for imgui
			PopulateLabels();

			// populate the current list of operations
			palette::PickRandomPalette( true );
			for ( int i = 0; i < 6; i++ ) {
				currentOperations.push_back( GetRandomOperation() );
			}

			glGenBuffers( 1, &dataBuffer );
			PassOperationData();
		}
	}

	void PassOperationData () {
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, dataBuffer );
		std::vector< vec4 > data;

		for ( uint32_t i = 0; i < currentOperations.size(); i++ ) {
			// operation information ( operation, input swizzle, output swizzle, unused )
			data.push_back( vec4( currentOperations[ i ].index, currentOperations[ i ].inputSwizzle, currentOperations[ i ].outputSwizzle, 0.0f ) );
			
			// arguments ( x,y,z are used, selectively, by some operations, fourth is unused )
			data.push_back( currentOperations[ i ].args );
			
			// color (r,g,b,unused alpha channel)
			data.push_back( currentOperations[ i ].color );
		}

		size_t numBytes = sizeof( float ) * 4 * 3 * currentOperations.size();
		glBufferData( GL_SHADER_STORAGE_BUFFER, numBytes, ( GLvoid * ) &data[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, dataBuffer );
	}

	void HandleQuitEvents () {
		ZoneScoped;
		//==============================================================================
		// Need to keep this for pQuit handling ( force quit )
		// In particular - checking for window close and the SDL_QUIT event can't really be determined
		//  via the keyboard state, and then imgui needs it too, so can't completely kill the event
		//  polling loop - maybe eventually I'll find a more complete solution for this
		SDL_Event event;
		SDL_PumpEvents();
		while ( SDL_PollEvent( &event ) ) {
			ImGui_ImplSDL3_ProcessEvent( &event ); // imgui event handling
			pQuit = config.oneShot || // swap out the multiple if statements for a big chained boolean setting the value of pQuit
				( event.type == SDL_EVENT_QUIT ) ||
				( event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID( window.window ) ) ||
				( event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE && SDL_GetModState() & SDL_KMOD_SHIFT );
			if ( ( event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE ) || ( event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_X1 ) ) {
				quitConfirm = !quitConfirm; // this has to stay because it doesn't seem like ImGui::IsKeyReleased is stable enough to use
			}

			if ( event.type == SDL_EVENT_MOUSE_WHEEL && !ImGui::GetIO().WantCaptureMouse ) {
				// float wheel_x = -event.wheel.x;
				const float wheel_y = event.wheel.y;
				scale *= ( wheel_y > 0.0f ) ? wheel_y * ( ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 1.01f : 1.1f ) : abs( wheel_y ) * ( ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 0.99f : 0.9f );
				brightness *= ( wheel_y > 0.0f ) ? wheel_y * ( ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 1.01f : 1.1f ) : abs( wheel_y ) * ( ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 0.99f : 0.9f );
				RendererNeedsReset = true;
			}
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// current state of the whole keyboard
		const bool * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		const SDL_Keymod k	= SDL_GetModState();
		const bool shift		= ( k & SDL_KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		if ( state[ SDL_SCANCODE_R ] && shift ) {
			// reset
			RendererNeedsReset = true;
			scale = 1.0f;
			offset = vec2( 0.0f );
			trident.InitBasis();
		}

		if ( state[ SDL_SCANCODE_Y ] && shift ) {
			// reload shaders
			LoadShaders();
		}

		vec2 mouse;
		uint32_t mouseState = SDL_GetMouseState( &mouse.x, &mouse.y );
		if ( mouseState != 0 && !ImGui::GetIO().WantCaptureMouse ) {
			ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
			ImGui::ResetMouseDragDelta();
			const float aspectRatio = ( float ) config.height / ( float ) config.width;
			offset.x += currentMouseDrag.x * aspectRatio * 0.002f / scale;
			offset.y += currentMouseDrag.y * 0.002f / scale;
			RendererNeedsReset = true;
		}
	}

	bool ImGuiDrawPalette ( int &palette, string sublabel, float min = 0.0f, float max = 1.0f ) {
		static std::vector< const char * > paletteLabels;
		if ( paletteLabels.size() == 0 ) {
			for ( auto& entry : palette::paletteListLocal ) {
				// copy to a cstr for use by imgui
				char * d = new char[ entry.label.length() + 1 ];
				std::copy( entry.label.begin(), entry.label.end(), d );
				d[ entry.label.length() ] = '\0';
				paletteLabels.push_back( d );
			}
		}

		ImGui::Combo( ( string( "Palette##" ) + sublabel ).c_str(), &palette, paletteLabels.data(), paletteLabels.size() );
		bool isUpdated = ImGui::IsItemEdited();
		const size_t paletteSize = palette::paletteListLocal[ palette ].colors.size();
		ImGui::Text( "  Contains %.3lu colors:", palette::paletteListLocal[ palette::PaletteIndex ].colors.size() );
		// handle max < min
		float minVal = min;
		float maxVal = max;
		float realSelectedMin = std::min( minVal, maxVal );
		float realSelectedMax = std::max( minVal, maxVal );
		size_t minShownIdx = std::floor( realSelectedMin * ( paletteSize - 1 ) );
		size_t maxShownIdx = std::ceil( realSelectedMax * ( paletteSize - 1 ) );

	// I want to convert from using @ symbols, to these rounded edge boxes
		// const ImVec2 p = ImGui::GetCursorScreenPos();
		// ImDrawList* draw_list = ImGui::GetWindowDrawList();
		// // Square with all rounded corners
		// ImVec4 colf = ImVec4(
		// 	palette::paletteListLocal[ palette ].colors[ 0 ].x,
		// 	palette::paletteListLocal[ palette ].colors[ 0 ].y,
		// 	palette::paletteListLocal[ palette ].colors[ 0 ].z,
		// 	1.0f
		// );
		// const ImU32 col = ImColor( colf );
		// draw_list->AddRectFilled( ImVec2( p.x, p.y ), ImVec2( p.x + 16, p.y + 16 ), col, 3.0f );

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
					color = ivec4( palette::paletteListLocal[ palette ].colors[ index ], 255 );
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
		return isUpdated;
	}

	void ImguiPass () {
		ZoneScoped;
	
		// =================================
		ImGui::Begin( "Operations" );

		ImGui::Text( "Rendering" );
		ImGui::Indent();
		// =================================
		if ( ImGui::Button( " Capture " ) ) {
			screenshotRequested = 1;
		}
		ImGui::SameLine();
		if ( ImGui::Button( " Capture EXR " ) ) {
			// I want EXRs for HDRI usage
			screenshotRequested = 2;
		}
		ImGui::SliderFloat( "Scale", &scale, 0.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		RendererNeedsReset = RendererNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "X Offset", &offset.x, -100.0f, 100.0f );
		RendererNeedsReset = RendererNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "Y Offset", &offset.y, -100.0f, 100.0f );
		RendererNeedsReset = RendererNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "Brightness", &brightness, 0.0f, 100000.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderFloat( "Brightness Power", &brightnessPower, 0.0f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::Unindent();
		ImGui::Text( " " );
		// =================================
		// selecting endpoints of the palette
		static float paletteEndpoints[ 2 ] = { 0.0f, 1.0f };
		ImGui::Text( "Palette" );
		ImGui::Indent();
		if ( ImGui::Button( " Pick Random Palette " ) ) {
			paletteEndpoints[ 0 ] = 0.0f;
			paletteEndpoints[ 1 ] = 1.0f;
			rng colorInit = rng( 0.0f, 1.0f );
			palette::PickRandomPalette( true );
			for ( uint32_t i = 0; i < currentOperations.size(); i++ ) {
				currentOperations[ i ].color = vec4( palette::paletteRef( colorInit() ), 1.0f );
			}
			BufferNeedsReset = true;
			RendererNeedsReset = true;
		}
		// showing palette contents
		ImGui::SliderFloat2( "Palette Endpoints", paletteEndpoints, 0.0f, 1.0f );
		if( ImGui::IsItemEdited() || ImGuiDrawPalette( palette::PaletteIndex, " ", paletteEndpoints[ 0 ], paletteEndpoints[ 1 ] ) ) {
			rng colorInit = rng( paletteEndpoints[ 0 ], paletteEndpoints[ 1 ] );
			for ( uint32_t i = 0; i < currentOperations.size(); i++ ) {
				currentOperations[ i ].color = vec4( palette::paletteRef( colorInit() ), 1.0f );
			}
			BufferNeedsReset = true;
			RendererNeedsReset = true;
		}
		ImGui::Unindent();
		// =================================
		ImGui::Text( " " );
		ImGui::Text( "Seeding" );
		ImGui::Indent();
		ImGui::Combo( "Initial Point Distribution", &initMode, initializationLabels, INIT_MODES_COUNT );
		ImGui::Unindent();
		// =================================
		ImGui::Text( " " );
		ImGui::Text( "Operations" );
		ImGui::SameLine();
		if ( ImGui::Button( " Randomize All " ) ) {
			for ( uint32_t i = 0; i < currentOperations.size(); i++ ) {
				currentOperations[ i ] = GetRandomOperation();
			}
			BufferNeedsReset = true;
			RendererNeedsReset = true;
		}
		ImGui::Indent();
		for ( uint32_t i = 0; i < currentOperations.size(); i++ ) { // list out the current operations
			string label = string( "Operation " ) + std::to_string( i );
			ImGui::Text( "%s", label.c_str() );
			ImGui::SameLine();
			if ( ImGui::Button( ( string( " Remove ##" + std::to_string( i ) ) ).c_str() ) ) {
				currentOperations.erase( currentOperations.begin() + i );
				BufferNeedsReset = true;
				RendererNeedsReset = true;
			}
			ImGui::SameLine();
			if ( ImGui::Button( ( string( " Randomize ##" + std::to_string( i ) ) ).c_str() ) ) {
				currentOperations.erase( currentOperations.begin() + i );
				currentOperations.insert( currentOperations.begin() + i, GetRandomOperation() );
				BufferNeedsReset = true;
				RendererNeedsReset = true;
			}

			ImGui::Separator();
			ImGui::Indent();

			// type of operation specified
			string typeLabel = string( "Type##" ) + std::to_string( i );
			ImGui::Combo( typeLabel.c_str(), ( int * ) &currentOperations[ i ].index, operationLabels, NUM_OPERATIONS );
			BufferNeedsReset |= ImGui::IsItemEdited();

			// input swizzle
			int dimensions = 0; int offset = 0; int count = 0;
			switch ( operationList[ currentOperations[ i ].index ].inputSize ) {
				case 1: dimensions = 1; offset =  1; count = 3; break;
				case 2: dimensions = 2; offset =  4; count = 6; break;
				case 3: dimensions = 3; offset = 10; count = 6; break;
				default: break;
			}
			string inputSwizzleLabel = string( "Input Swizzle (" ) + std::to_string( dimensions ) + string( "D)##" ) + std::to_string( i );
			currentOperations[ i ].inputSwizzle = swizzle( int( currentOperations[ i ].inputSwizzle ) - offset );
			ImGui::Combo( inputSwizzleLabel.c_str(), ( int * ) &currentOperations[ i ].inputSwizzle, swizzleLabels + offset, count );
			currentOperations[ i ].inputSwizzle = swizzle( int( currentOperations[ i ].inputSwizzle ) + offset );
			BufferNeedsReset |= ImGui::IsItemEdited();

			// output swizzle
			dimensions = 0; offset = 0; count = 0;
			switch ( operationList[ currentOperations[ i ].index ].outputSize ) {
				case 1: dimensions = 1; offset =  1; count = 3; break;
				case 2: dimensions = 2; offset =  4; count = 6; break;
				case 3: dimensions = 3; offset = 10; count = 6; break;
				default: break;
			}
			string outputSwizzleLabel = string( "Output Swizzle (" ) + std::to_string( dimensions ) + string( "D)##" ) + std::to_string( i );
			currentOperations[ i ].outputSwizzle = swizzle( int( currentOperations[ i ].outputSwizzle ) - offset );
			ImGui::Combo( outputSwizzleLabel.c_str(), ( int * ) &currentOperations[ i ].outputSwizzle, swizzleLabels + offset, count );
			currentOperations[ i ].outputSwizzle = swizzle( int( currentOperations[ i ].outputSwizzle ) + offset );
			BufferNeedsReset |= ImGui::IsItemEdited();

			int numArgs = operationList[ currentOperations[ i ].index ].numArgs;
			string args = string( "Arguments##" ) + std::to_string( i );
			switch ( numArgs ) {
				case 0: break; // no op
				case 1:
					ImGui::SliderFloat( args.c_str(), ( float * ) &currentOperations[ i ].args, -2.0f, 2.0f );
					BufferNeedsReset |= ImGui::IsItemEdited();
					break;
				case 2:
					ImGui::SliderFloat2( args.c_str(), ( float * ) &currentOperations[ i ].args, -2.0f, 2.0f );
					BufferNeedsReset |= ImGui::IsItemEdited();
					break;
				case 3:
					ImGui::SliderFloat3( args.c_str(), ( float * ) &currentOperations[ i ].args, -2.0f, 2.0f );
					BufferNeedsReset |= ImGui::IsItemEdited();
					break;
			}

			// color for this operation
			string colorLabel = string( "Color##" ) + std::to_string( i );
			ImGui::ColorEdit3( colorLabel.c_str(), ( float * ) &currentOperations[ i ].color, ImGuiColorEditFlags_PickerHueWheel );
			BufferNeedsReset |= ImGui::IsItemEdited();

			ImGui::Unindent();
		}
		// button to add another one after the list
		if ( ImGui::Button( " Add Random Operation " ) ) {
			currentOperations.push_back( GetRandomOperation() );
			BufferNeedsReset = true;
			RendererNeedsReset = true;
		}
		ImGui::Unindent();

		ImGui::End();
		// =================================

		if ( tonemap.showTonemapWindow ) {
			TonemapControlsWindow();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );
			glUniform1f( glGetUniformLocation( shader, "brightness" ), brightness );
			glUniform1f( glGetUniformLocation( shader, "brightnessPower" ), brightnessPower );

			textureManager.BindImageForShader( "IFS Accumulator R", "ifsAccumulatorR", shader, 2 );
			textureManager.BindImageForShader( "IFS Accumulator G", "ifsAccumulatorG", shader, 3 );
			textureManager.BindImageForShader( "IFS Accumulator B", "ifsAccumulatorB", shader, 4 );

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

			if ( screenshotRequested != 0 ) {
				if ( screenshotRequested == 1 ) {
					const GLuint tex = textureManager.Get( "Display Texture" );
					uvec2 dims = textureManager.GetDimensions( "Display Texture" );
					std::vector< float > imageBytesToSave;
					imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
					glBindTexture( GL_TEXTURE_2D, tex );
					glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
					Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
					screenshot.FlipVertical();
					screenshot.RGBtoSRGB();
					const string filename = string( "ifs-" ) + timeDateString() + string( ".png" );
					screenshot.Save( filename );
				} else if ( screenshotRequested == 2 ) {
					// todo: what texture has the floating point version?
					const GLuint tex = textureManager.Get( "Accumulator" );
					uvec2 dims = textureManager.GetDimensions( "Accumulator" );
					std::vector< float > imageBytesToSave;
					imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
					glBindTexture( GL_TEXTURE_2D, tex );
					glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
					Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
					const string filename = string( "ifs-" ) + timeDateString() + string( ".exr" );
					screenshot.Save( filename, Image_4F::backend::TINYEXR );
				}
				screenshotRequested = 0;
			}
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		// application-specific update code
		ZoneScoped; scopedTimer Start( "Update" );

		// reset the buffer, when needed
		if ( RendererNeedsReset == true ) {
			RendererNeedsReset = false;

			// reset the value tracking the max
			constexpr uint32_t countValue = 0;
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, maxBuffer );

			// reset the accumulator
			const GLuint shader = shaders[ "Clear" ];
			glUseProgram( shader );

			textureManager.BindImageForShader( "IFS Accumulator R", "ifsAccumulatorR", shader, 2 );
			textureManager.BindImageForShader( "IFS Accumulator G", "ifsAccumulatorG", shader, 3 );
			textureManager.BindImageForShader( "IFS Accumulator B", "ifsAccumulatorB", shader, 4 );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_ALL_BARRIER_BITS );
		}

		if ( BufferNeedsReset ) {
			BufferNeedsReset = false;
			PassOperationData();
		}

		const GLuint shader = shaders[ "Update" ];
		glUseProgram( shader );

		static rngi wangSeeder = rngi( 0, 10000000 );
		glUniform1i( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );
		glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
		glUniform2f( glGetUniformLocation( shader, "offset" ), offset.x, offset.y );
		glUniform1i( glGetUniformLocation( shader, "numTransforms" ), currentOperations.size() );
		glUniform1i( glGetUniformLocation( shader, "initMode" ), initMode );

		// get the trident matrix
		mat3 tridentMatrix = mat3( trident.basisX, trident.basisY, trident.basisZ );
		glUniformMatrix3fv( glGetUniformLocation( shader, "tridentMatrix" ), 1, GL_FALSE, glm::value_ptr( tridentMatrix ) );

		textureManager.BindImageForShader( "IFS Accumulator R", "ifsAccumulatorR", shader, 2 );
		textureManager.BindImageForShader( "IFS Accumulator G", "ifsAccumulatorG", shader, 3 );
		textureManager.BindImageForShader( "IFS Accumulator B", "ifsAccumulatorB", shader, 4 );

		glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
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

		// event handling
		HandleCustomEvents();
		HandleQuitEvents();
		// trident for orientation
		HandleTridentEvents();
		if ( trident.Dirty() ) {
			RendererNeedsReset = true;
			trident.ClearDirty();
		}

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	ifs engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
