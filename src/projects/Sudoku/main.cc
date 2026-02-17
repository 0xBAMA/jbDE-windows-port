#include "../../engine/engine.h"

enum class cellState {
	NOTES, SOLVED, INCORRECT
};

struct cell {
	bool notes[ 10 ] = { false };		// bool values for the user's notetaking
	cellState state = cellState::NOTES;	// relevant display state
	int pickedValue = 0;				// integer value solution
};

struct SudokuBoard {
	// dump to command line
	void printBoard () {
		cout << endl;
		for ( int i = 0; i < 9; i++ ) {
			for ( int j = 0; j < 9; j++ ) {
				if ( board[ i ][ j ].state != cellState::NOTES ) {
					// we have contents... correct or otherwise
					cout << " " << board[ i ][ j ].pickedValue << " ";
				} else {
					cout << " _ ";
				}
			}
			cout << endl;
		}
	}

	ivec2 cursor = { 5, 5 };
	void setCursorClamped( ivec2 newPos ) { cursor.x = std::clamp( newPos.x, 0, 8 ); cursor.y = std::clamp( newPos.y, 0, 8 ); }
	void cursorUp() { setCursorClamped( cursor + ivec2( 0, -1 ) ); };
	void cursorDown() { setCursorClamped( cursor + ivec2( 0, 1 ) ); };
	void cursorLeft() { setCursorClamped( cursor + ivec2(  -1, 0 ) ); };
	void cursorRight() { setCursorClamped( cursor + ivec2( 1, 0 ) ); };

	// the state of the board
	cell board[ 9 ][ 9 ];

	// board, as solved
	cell refboard[ 9 ][ 9 ];

	// rng for shuffling arrays
	std::default_random_engine rng = std::default_random_engine {};

	void clearCell ( ivec2 p ) {
		board[ p.x ][ p.y ].state = cellState::NOTES;
		board[ p.x ][ p.y ].pickedValue = 0;
		for ( int k = 0; k < 10; k++ ) {
			board[ p.x ][ p.y ].notes[ k ] = false;
		}
	}

	void rowClear ( int row ) {
		for ( int i = 0; i < 9; i++ ) {
			clearCell( ivec2( i, row ) );
		}
	}

	void boardClear () {
		for ( int i = 0; i < 9; i++ ) {
			for ( int j = 0; j < 9; j++ ) {
				clearCell( ivec2( i, j ) );
			}
		}
	}

	ivec2 findRandomUnsolved () {
		std::vector< ivec2 > unsolved;
		for ( int i = 0; i < 9; i++ ) {
			for ( int j = 0; j < 9; j++ ) {
				if ( board[ i ][ j ].state == cellState::NOTES ) {
					unsolved.push_back( ivec2( i, j ) );
				}
			}
		}
		std::shuffle( unsolved.begin(), unsolved.end(), rng );
		return unsolved[ 0 ];
	}

	void attemptSolveRow ( int row ) {
		vector< int > solved;
		vector< ivec2 > open;
		for ( int i = 0; i < 9; i++ ) {
			if ( board[ i ][ row ].state == cellState::SOLVED ) {
				solved.push_back( i );
			} else {
				open.push_back( ivec2( i, row ) );
			}
		}
		vector< int > unsolved;
		for ( int i = 0; i < 9; i++ ) {
			bool found = false;
			for ( int j = 0; j < solved.size(); j++ ) {
				if ( solved[ j ] == i ) found = true;
			}
			if ( !found ) unsolved.push_back( i );
		}

		do {
			// we now have a list of candidates to shuffle, and places to put them
			std::shuffle( unsolved.begin(), unsolved.end(), rng );
			std::shuffle( open.begin(), open.end(), rng );

			// apply the selected ordering... list of positions is the same, so no need to roll back old set
			for ( int i = 0; i < unsolved.size(); i++ ) {
				board[ open[ i ].x ][ open[ i ].y ].state = cellState::SOLVED;
				board[ open[ i ].x ][ open[ i ].y ].pickedValue = unsolved[ i ];
				for ( int j = 0; j < 10; j++ ) {
					board[ open[ i ].x ][ open[ i ].y ].notes[ j ] = ( j == unsolved[ i ] ) ? true : false;
				}
			}
		} while ( violation() );
	}

	void attemptSolveColumn ( int col ) {
		vector< int > solved;
		vector< ivec2 > open;
		for ( int i = 0; i < 9; i++ ) {
			if ( board[ col ][ i ].state == cellState::SOLVED ) {
				solved.push_back( i );
			} else {
				open.push_back( ivec2( col, i ) );
			}
		}
		vector< int > unsolved;
		for ( int i = 0; i < 9; i++ ) {
			bool found = false;
			for ( int j = 0; j < solved.size(); j++ ) {
				if ( solved[ j ] == i ) found = true;
			}
			if ( !found ) unsolved.push_back( i );
		}

		do {
			// we now have a list of candidates to shuffle, and places to put them
			std::shuffle( unsolved.begin(), unsolved.end(), rng );
			std::shuffle( open.begin(), open.end(), rng );

			// apply the selected ordering... list of positions is the same, so no need to roll back old set
			for ( int i = 0; i < unsolved.size(); i++ ) {
				board[ open[ i ].x ][ open[ i ].y ].state = cellState::SOLVED;
				board[ open[ i ].x ][ open[ i ].y ].pickedValue = unsolved[ i ];
				for ( int j = 0; j < 10; j++ ) {
					board[ open[ i ].x ][ open[ i ].y ].notes[ j ] = ( j == unsolved[ i ] ) ? true : false;
				}
			}
		} while ( violation() );
	}

	void randomInit () {
		boardClear();
		randomCorrectBlock( ivec2( 0 ) );

		/*
		for ( int i = 0; i < 9; i++ ) {
			do {
				rowClear( i );
				randomCorrectRow( i );
				// attemptSolveSingleValue();

			} while ( violation() );
			cout << "Eschelon " << i << " locked" << endl;
			printBoard();
		}
		*/

		rngi pick( 0, 1 );
		while ( unsolvedRemaining() ) {
			// pick a random unsolved cell.. we're considering either its row or column
			ivec2 picked = findRandomUnsolved();
			if ( pick() == 0 ) {
				// try to fill in the remaining section of a row or column...
				// if this leads to a violation, roll back these changes
				attemptSolveRow( picked.y );
			} else {
				attemptSolveColumn( picked.x );
			}
			printBoard();
			attemptSolveSingleValue();
		}
	}

	void randomCorrectRow ( int row ) {
		static vector< int > values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
		std::shuffle( values.begin(), values.end(), rng );
		for ( int i = 0; i < 9; i++ ) {
			board[ i ][ row ].state = cellState::SOLVED;
			board[ i ][ row ].pickedValue = values[ i ];
		}
	}

	void randomCorrectBlock ( ivec2 basePoint ) {
		static vector< int > values = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
		std::shuffle( values.begin(), values.end(), rng );
		int k = 0;
		for ( int i = 0; i < 3; i++ ) {
			for ( int j = 0; j < 3; j++ ) {
				board[ basePoint.x + i ][ basePoint.y + j ].state = cellState::SOLVED;
				board[ basePoint.x + i ][ basePoint.y + j ].pickedValue = values[ k ];
				k++;
			}
		}
	}

	void clearRow ( int row ) {
		for ( int i = 0; i < 9; i++ ) {
			board[ i ][ row ].state = cellState::NOTES;
			board[ i ][ row ].pickedValue = 0;
			for( int j = 0; j < 9; j++ ) {
				board[ i ][ row ].notes[ j ] = false;
			}
		}
	}

	bool violation () {
		// is there an issue with the board?
		autoNotes();
		for ( int i = 0; i < 9; i++ ) {
			for ( int j = 0; j < 9; j++ ) {
				if ( board[ i ][ j ].notes[ board[ i ][ j ].pickedValue ] == false && board[ i ][ j ].pickedValue != 0 ) {
					/*
					printBoard();
					cout << "found violation...." << endl;
						cout << i << " " << j << endl;
						cout << board[ i ][ j ].pickedValue << endl;
						cout << ( board[ i ][ j ].state == cellState::SOLVED ? "solved" : "notes" ) <<  endl;
						cout << ( board[ i ][ j ].notes[ board[ i ][ j ].pickedValue ] ? "true" : "false" ) << endl;
					*/
					return true;
				}
			}
		}
		return false;
	}

	bool unsolvedRemaining () {
		for ( int i = 0; i < 9; i++ ) {
			for ( int j = 0; j < 9; j++ ) {
				if ( board[ i ][ j ].state != cellState::SOLVED ) {
					return true;
				}
			}
		}
		return false;
	}

	// this is by definition correct... however, it often fails
	bool attemptSolveSingleValue () {
		for ( int i = 0; i < 9; i++ ) {
			for ( int j = 0; j < 9; j++ ) {
				if ( board [ i ][ j ].state != cellState::SOLVED ) {
					int count = 0;
					int last = 0;
					for ( int k = 1; k <= 9; k++ ) {
						if ( board[ i ][ j ].notes[ k ] == true ) {
							count++;
							last = k;
						}
					}
					if ( count == 1 ) {
						board[ i ][ j ].state = cellState::SOLVED;
						board[ i ][ j ].pickedValue = last;
						for ( int k = 0; k <= 10; k++ ) {
							board[ i ][ j ].notes[ k ] = false;
						}
						board[ i ][ j ].notes[ last ] = true;
						return true;
					}
				}
			}
		}
		return false;
	}

	// autonotes support functions
	void removeFromRow ( const ivec2 p, const int value ) { // remove a number from the rows and column where it lives
		for ( int i = 0; i < 9; i++ ) {
			board[ i ][ p.y ].notes[ value ] = false;
			board[ p.x ][ i ].notes[ value ] = false;
		}
		// but keep myself, because this is how violations are checked
		board[ p.x ][ p.y ].notes[ value ] = true;
	}

	void removeFromBlock ( const ivec2 p, const int value ) {
		// figure out which block we're in
		ivec2 pQ = 3 * ( p / 3 );
		for ( int i = 0; i < 3; i++ ) {
			for ( int j = 0; j < 3; j++ ) {
				board[ pQ.x + i ][ pQ.y + j ].notes[ value ] = false;
			}
		}
		board[ p.x ][ p.y ].notes[ value ] = true;
	}

	void autoNotes () {
		// initially all flags set true
		for ( int i = 0; i < 9; i++ ) {
			for ( int j = 0; j < 9; j++ ) {
				// if ( board[ i ][ j ].state == cellState::NOTES ) {
					for ( int k = 1; k < 9; k++ ) {
						board[ i ][ j ].notes[ k ] = true;
					}
				// }
			}
		}

		// apply exclusion rules
		for ( int i = 0; i < 9; i++ ) {
			for ( int j = 0; j < 9; j++ ) {
				if ( board[ i ][ j ].state == cellState::SOLVED ) {
					removeFromBlock( ivec2( i, j ), board[ i ][ j ].pickedValue );
					removeFromRow( ivec2( i, j ), board[ i ][ j ].pickedValue );
				}
			}
		}
	}

	// set of display colors for the GPU... plus bg?
	vec4 colors[ 10 ] = { vec4( 0.0f ) };
	void randomColors () {
		palette::PickRandomPalette( true );
		for ( int i = 0; i < 9; i++ ) {
			colors[ i ].rgb = palette::paletteRef( float( i + 0.5f ) / 9.0f );
			colors[ i ].a = 1.0f;
		}
	}

	void initialColors () {
		palette::PickPaletteByLabel( "superjail" );
		for ( int i = 0; i < 9; i++ ) {
			colors[ i ].rgb = palette::paletteRef( sqrt( 1.0f - float( i + 0.5f ) / 9.0f ) );
			colors[ i ].a = 1.0f;
		}
	}

	// prepared GPU board state... could be more efficient but whatever
	int stateValues[ 81 ];
	int notesValues[ 81 ];
	void prepBoardState () {
		for ( int y = 0; y < 9; y++ ) {
			for ( int x = 0; x < 9; x++ ) {
				stateValues[ x + 9 * y ] = board[ x ][ y ].pickedValue;
				if ( board[ x ][ y ].state == cellState::NOTES ) {
					stateValues[ x + 9 * y ] = 0; // zero value means you should show the notes
				} else if ( board[ x ][ y ].state == cellState::SOLVED ) {
					// just keep the value, we'll have a positive integer
				} else if ( board[ x ][ y ].state == cellState::INCORRECT ) {
					stateValues[ x + 9 * y ] *= -1; // negative value to signal incorrect
				}
				notesValues[ x + 9 * y ] =
					( 1 << 9 ) * ( board[ x ][ y ].notes[ 9 ] ? 1 : 0 ) +
					( 1 << 8 ) * ( board[ x ][ y ].notes[ 8 ] ? 1 : 0 ) +
					( 1 << 7 ) * ( board[ x ][ y ].notes[ 7 ] ? 1 : 0 ) +
					( 1 << 6 ) * ( board[ x ][ y ].notes[ 6 ] ? 1 : 0 ) +
					( 1 << 5 ) * ( board[ x ][ y ].notes[ 5 ] ? 1 : 0 ) +
					( 1 << 4 ) * ( board[ x ][ y ].notes[ 4 ] ? 1 : 0 ) +
					( 1 << 3 ) * ( board[ x ][ y ].notes[ 3 ] ? 1 : 0 ) +
					( 1 << 2 ) * ( board[ x ][ y ].notes[ 2 ] ? 1 : 0 ) +
					( 1 << 1 ) * ( board[ x ][ y ].notes[ 1 ] ? 1 : 0 ) +
					( 1 << 0 ) * ( board[ x ][ y ].notes[ 0 ] ? 1 : 0 );
			}
		}
	}
};

class Sudoku final : public engineBase { // sample derived from base engine class
public:
	Sudoku () { Init(); OnInit(); PostInit(); }
	~Sudoku () { Quit(); }

	SudokuBoard board;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "../src/projects/Sudoku/shaders/draw.cs.glsl" ).shaderHandle;

			board.randomInit();
			board.initialColors();
			// board.autoNotes();
			board.printBoard();

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		if ( inputHandler.getState4( KEY_L ) == KEYSTATE_RISING ||  inputHandler.getState4( KEY_A ) == KEYSTATE_RISING || inputHandler.getState4( KEY_RIGHT ) == KEYSTATE_RISING ) {
			board.cursorRight();
		}
		if ( inputHandler.getState4( KEY_H ) == KEYSTATE_RISING ||  inputHandler.getState4( KEY_D ) == KEYSTATE_RISING || inputHandler.getState4( KEY_LEFT ) == KEYSTATE_RISING ) {
			board.cursorLeft();
		}
		if ( inputHandler.getState4( KEY_K ) == KEYSTATE_RISING ||  inputHandler.getState4( KEY_W ) == KEYSTATE_RISING || inputHandler.getState4( KEY_UP ) == KEYSTATE_RISING ) {
			board.cursorUp();
		}
		if ( inputHandler.getState4( KEY_J ) == KEYSTATE_RISING ||  inputHandler.getState4( KEY_S ) == KEYSTATE_RISING || inputHandler.getState4( KEY_DOWN ) == KEYSTATE_RISING ) {
			board.cursorDown();
		}

		if ( inputHandler.getState4( KEY_R ) == KEYSTATE_RISING ) {
			board.randomColors();
			board.randomInit();
			board.autoNotes();
		}
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

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );

		ImGui::Begin( "Debug" );
		for ( int i = 0; i < 10; i++ ) {
			ImGui::ColorEdit4( ( string( "Color " ) + to_string( i ) ).c_str(), ( float * ) &board.colors[ i ] );
		}
		ImGui::End();
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		// draw some shit - need to add a hello triangle to this, so I have an easier starting point for raster stuff
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			const GLuint shader = shaders[ "Draw" ];

			glUseProgram( shader );
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			glUniform2i( glGetUniformLocation( shader, "cursor" ), board.cursor.x, board.cursor.y );
			glUniform4fv( glGetUniformLocation( shader, "colors" ), 10, glm::value_ptr( board.colors[ 0 ] ) );

			board.prepBoardState();
			glUniform1iv( glGetUniformLocation( shader, "notes" ), 81, &board.notesValues[ 0 ] );
			glUniform1iv( glGetUniformLocation( shader, "state" ), 81, &board.stateValues[ 0 ] );

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

		// shader to apply dithering
			// ...

		// other postprocessing
			// ...

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
		DrawAPIGeometry();			// draw any API geometry desired
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

// #pragma comment( linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup" )

int main ( int argc, char *argv[] ) {
	Sudoku engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
