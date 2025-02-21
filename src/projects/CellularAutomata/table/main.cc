#include "../../../engine/engine.h"

struct CAConfig_t {
	// CA buffer dimensions
	uint32_t dimensionX;
	uint32_t dimensionY;

	// deciding how to populate the buffer
	float generatorThreshold;

	// toggling buffers
	bool oddFrame = false;

	// int rule[ 25 ] = {
	// 	2, 0, 2, 2, 1,
	// 	0, 2, 0, 2, 2,
	// 	1, 1, 0, 0, 2,
	// 	0, 1, 1, 2, 1,
	// 	2, 2, 1, 1, 2
	// };

	int rule[ 25 ] = {
		0, 1, 0, 2, 2,
		0, 2, 2, 1, 0,
		2, 0, 0, 2, 1,
		0, 0, 0, 2, 0,
		2, 0, 0, 2, 2
	};

	std::vector< glm::uvec2 > encodedRules;
	uint32_t currentRule = 0;
};

class CAHistory final : public engineBase {
public:
	CAHistory () { Init(); OnInit(); PostInit(); }
	~CAHistory () { Quit(); }

	CAConfig_t CAConfig;

	void newRule () {
		// rngi gen( 0, 2 );
		// for ( int i = 0; i < 25; i++ ) {
		// 	CAConfig.rule[ i ] = gen();
		// }

		// for ( size_t i = 0; i < CAConfig.dimensionX * CAConfig.dimensionY; i++ ) {
		// 	uint32_t value = 0;
		// 	for ( size_t b = 0; b < 32; b++ ) {
		// 		value = value << 1;
		// 		value = value | ( ( gen() < CAConfig.generatorThreshold ) ? 1u : 0u );
		// 	}
		// 	initialData.push_back( value );
		// }

		{
			const GLuint shader = shaders[ "Rule Write" ];
			glUseProgram( shader );

			textureManager.BindImageForShader( "Automata Rule Buffer", "ruleBuffer", shader, 4 );

			static rngi rulePick( 0, CAConfig.encodedRules.size() );
			CAConfig.currentRule = rulePick();
			glUniform1i( glGetUniformLocation( shader, "rulePick" ), CAConfig.currentRule );

			glUniform1i( glGetUniformLocation( shader, "numRules" ), CAConfig.encodedRules.size() );
			glUniform2uiv( glGetUniformLocation( shader, "rules" ), CAConfig.encodedRules.size(), ( GLuint * ) &CAConfig.encodedRules[ 0 ] );

			// dispatch the compute shader - go from back buffer to front buffer
			glDispatchCompute( ( CAConfig.dimensionX + 15 ) / 16, ( CAConfig.dimensionY + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			ReloadShaders();

			json j; ifstream i ( "../src/engine/config.json" ); i >> j; i.close();
			CAConfig.dimensionX = j[ "app" ][ "CellularAutomata" ][ "dimensionX" ];
			CAConfig.dimensionY = j[ "app" ][ "CellularAutomata" ][ "dimensionY" ];
			CAConfig.generatorThreshold = j[ "app" ][ "CellularAutomata" ][ "generatorThreshold" ];

		// setup the image buffers for the CA state ( 2x for ping-ponging )
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= CAConfig.dimensionX;
			opts.height			= CAConfig.dimensionY;
			opts.textureType	= GL_TEXTURE_2D;
			opts.pixelDataType	= GL_UNSIGNED_BYTE;
			opts.initialData	= nullptr;
			textureManager.Add( "Automata State Buffer 0", opts );
			textureManager.Add( "Automata State Buffer 1", opts );

			opts.dataType		= GL_RG32UI;
			textureManager.Add( "Automata Rule Buffer", opts );

			// field state buffer ( move to shader )
			BufferReset();

			// loading the list of rules...
			json j2; ifstream i2 ( "../src/projects/CellularAutomata/table/tableCArules.json" ); i2 >> j2; i2.close();
			std::vector< std::vector< int > > rules;
			for ( auto& rule : j2 ) {
				std::vector< int > currentRule;
				for ( auto& entry : rule ) {
					currentRule.push_back( int( entry ) );
				}
				rules.push_back( currentRule );
			}

			for ( auto& rule : rules ) {
				uvec2 encodedRule = uvec2( 0 );
				for ( int i = 0; i < 16; i++ ) { // encode 0..15
					encodedRule.x += ( rule[ i ] << ( 30 - 2 * i ) );
				}

				for ( int i = 16; i < 25; i++ ) { // encode 16..24
					encodedRule.y += ( rule[ i ] << ( 30 - 2 * ( i - 16 ) ) );
				}
				CAConfig.encodedRules.push_back( encodedRule );
			}

			// put some contents into the rule buffer
			newRule();

			/* {
				const int glyphHeight = 17;
				const int glyphWidth = 12;

				// init list of rules...
				std::vector< std::vector< int > > rules;

				// load the mask image
				Image_4U maskPattern;
				maskPattern.Load( "./mask.png" );

				// find the mask locations, keep in a list
				std::vector< ivec2 > maskLocations;
				for ( int y = 0; y < maskPattern.Height(); y++ ) {
					for ( int x = 0; x < maskPattern.Width(); x++ ) {
						if ( maskPattern.GetAtXY( x, y )[ alpha ] == 255 ) {
							// adding an offset
							maskLocations.push_back( ivec2( x, y ) );
						}
					}
				}

				// load the reference patterns
				Image_4U referencePatterns;
				referencePatterns.Load( "./reference.png" );
				Image_4U zero( glyphWidth, glyphHeight ), one( glyphWidth, glyphHeight ), two( glyphWidth, glyphHeight );
				for ( int y = 0; y < glyphHeight; y++ ) {
					for ( int x = 0; x < glyphWidth; x++ ) {
						zero.SetAtXY( x, y, referencePatterns.GetAtXY( x, y ) );
						one.SetAtXY( x, y, referencePatterns.GetAtXY( x, y + glyphHeight ) );
						two.SetAtXY( x, y, referencePatterns.GetAtXY( x, y + 2 * glyphHeight ) );
					}
				}

				// start filesystem crap
				struct pathLeafString {
					std::string operator()( const std::filesystem::directory_entry &entry ) const {
						return entry.path().string();
					}
				};
				std::vector< string > directoryStrings;
				std::filesystem::path p( "../Source/" );
				std::filesystem::directory_iterator start( p );
				std::filesystem::directory_iterator end;
				std::transform( start, end, std::back_inserter( directoryStrings ), pathLeafString() );
				std::sort( directoryStrings.begin(), directoryStrings.end() ); // sort alphabetically
				// end filesystem crap

				std::vector< int > currentRule;
				for ( const auto& i : directoryStrings ) {
					currentRule.clear();

					// for each image in ../Source/
					Image_4U sourceData;
					sourceData.Load( i );

					// for 25 mask locations...
					for ( int j = 0; j < maskLocations.size(); j++ ) {
						Image_4U currentGlyph( glyphWidth, glyphHeight );
						for ( int y = 0; y < glyphHeight; y++ ) {
							for ( int x = 0; x < glyphWidth; x++ ) {
								currentGlyph.SetAtXY( x, y, sourceData.GetAtXY( maskLocations[ j ].x + x, maskLocations[ j ].y + y ) );
							}
						}

						float zeroTotal = 0.0f, oneTotal = 0.0f, twoTotal = 0.0f;
						for ( int y = 0; y < glyphHeight; y++ ) {
							for ( int x = 0; x < glyphWidth; x++ ) {
								Image_4U::color test = currentGlyph.GetAtXY( x, y );
								vec3 testColor = vec3( test[ red ], test[ green ], test[ blue ] );

								Image_4U::color zeroData = zero.GetAtXY( x, y );
								Image_4U::color oneData = one.GetAtXY( x, y );
								Image_4U::color twoData = two.GetAtXY( x, y );

								vec3 zeroColor = vec3( zeroData[ red ], zeroData[ green ], zeroData[ blue ] );
								vec3 oneColor = vec3( oneData[ red ], oneData[ green ], oneData[ blue ] );
								vec3 twoColor = vec3( twoData[ red ], twoData[ green ], twoData[ blue ] );

								zeroTotal += glm::distance( testColor, zeroColor );
								oneTotal += glm::distance( testColor, oneColor );
								twoTotal += glm::distance( testColor, twoColor );
							}
						}

						// match to 0, 1, or 2
						int identifiedDigit = -1;
						float minimumDistance = std::min( std::min( zeroTotal, oneTotal ), twoTotal );

						if ( minimumDistance == zeroTotal ) {
							identifiedDigit = 0;
						} else if ( minimumDistance == oneTotal ) {
							identifiedDigit = 1;
						} else if ( minimumDistance == twoTotal ) {
							identifiedDigit = 2;
						}

						if ( identifiedDigit != -1 ) {
							currentRule.push_back( identifiedDigit );
						}
					}

					// add the rule to the list
					if ( currentRule.size() == 25 ) {
						cout << i << " - Valid Rule: ";
						for ( int j = 0; j < currentRule.size(); j++ ) {
							cout << " " << currentRule[ j ];
						}
						cout << endl;
						rules.push_back( currentRule );
					} else {
						cout << "WRONG NUMBER OF DIGITS IN RULE" << endl;
					}
				}


				// identify duplicate rules, exact matches only
				cout << "Removing duplicates..." << endl;
				std::vector < std::vector< int > > finalRules;
				for ( int i = 0; i < rules.size(); i++ ) {
					std::vector< int > rule = rules[ i ];

					bool found = false;
					for ( int j = 0; j < finalRules.size(); j++ ) {
						// if not found in the list of final rules
						bool match = true;
						for ( int k = 0; k < 25; k++ ) {
							if ( rule[ k ] != finalRules[ j ][ k ] ) {
								match = false;
								break;
							}
						}

						// found match invalidates the rule being added
						if ( match == true ) {
							found = true;
						}
					}

					if ( !found ) {
						finalRules.push_back( rule );
					}
				}
				cout << "Pruned from " << rules.size() << " to " << finalRules.size() << endl;

				json j;
				for ( int i = 0; i < finalRules.size(); i++ ) {
					for ( int k = 0; k < 25; k++ ) {
						j[ std::to_string( i ) ][ std::to_string( k ) ] = finalRules[ i ][ k ];
					}
				}
				std::ofstream o ( "./tableCA.json" ); o << j.dump( 2 ); o.close();
			} */
		}
	}

	void ReloadShaders () {
		shaders[ "Draw" ] = computeShader( "../src/projects/CellularAutomata/table/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "Update" ] = computeShader( "../src/projects/CellularAutomata/table/shaders/update.cs.glsl" ).shaderHandle;
		shaders[ "Rule Write" ] = computeShader( "../src/projects/CellularAutomata/table/shaders/ruleWrite.cs.glsl" ).shaderHandle;
	}

	void BufferReset () { // put random bits in the buffer
		string backBufferLabel = string( "Automata State Buffer " ) + string( CAConfig.oddFrame ? "0" : "1" );

		// random data init
		std::vector< uint32_t > initialData;
		rng gen( 0.0f, 1.0f );

		// for ( size_t i = 0; i < CAConfig.dimensionX * CAConfig.dimensionY; i++ ) {
		// 	uint32_t value = 0;
		// 	for ( size_t b = 0; b < 32; b++ ) {
		// 		value = value << 1;
		// 		value = value | ( ( gen() < CAConfig.generatorThreshold ) ? 1u : 0u );
		// 	}
		// 	initialData.push_back( value );
		// }

		const uint32_t dX = CAConfig.dimensionX;
		const uint32_t dY = CAConfig.dimensionY;

		for ( uint32_t y = 0; y < CAConfig.dimensionY; y++ ) {
			for ( uint32_t x = 0; x < CAConfig.dimensionX; x++ ) {
				// if ( gen() < ( distance( vec2( CAConfig.dimensionX / 2.0f, CAConfig.dimensionY / 2.0f ), vec2( x, y ) ) / 100.0f ) ) {
				// 	initialData.push_back( 1 );
				// } else {
				// 	initialData.push_back( 0 );
				// }

				// initialData.push_back( ( x > 100 && y > 10 && x < dX - 100 && y < dY - 300 ) ? ( gen() < CAConfig.generatorThreshold ? 1 : 0 ) : 0 );

				initialData.push_back( gen() < CAConfig.generatorThreshold ? 1 : 0 );
			}
		}

		// no current functionality for updating the buffer - going to raw OpenGL
		GLuint handle = textureManager.Get( backBufferLabel );
		glBindTexture( GL_TEXTURE_2D, handle );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, CAConfig.dimensionX, CAConfig.dimensionY, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, ( void * ) initialData.data() );
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		const bool * state = SDL_GetKeyboardState( NULL );
		if ( state[ SDL_SCANCODE_R ] ) {
			// reset buffer contents, in the back buffer
			BufferReset();
			SDL_Delay( 20 ); // debounce
		}
	
		if ( state[ SDL_SCANCODE_Y ] ) {
			ReloadShaders();
		}

		if ( state[ SDL_SCANCODE_G ] ) {
			// will need work
			newRule();
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
	}

	void ComputePasses () {
		ZoneScoped;

		string backBufferLabel;
		string frontBufferLabel;

		{
			scopedTimer Start( "Update" );
			bool bufferToggle = CAConfig.oddFrame;
			// for ( int i = 0; i < 64; i++ ) {
				// swap buffers - precalculate strings for use later
				backBufferLabel = string( "Automata State Buffer " ) + string( bufferToggle ? "0" : "1" );
				frontBufferLabel = string( "Automata State Buffer " ) + string( bufferToggle ? "1" : "0" );

				bufferToggle = !bufferToggle;

				{ // update the state of the CA
					const GLuint shader = shaders[ "Update" ];
					glUseProgram( shader );

					// bind front buffer, back buffer
					textureManager.BindImageForShader( backBufferLabel, "backBuffer", shader, 2 );
					textureManager.BindImageForShader( frontBufferLabel, "frontBuffer", shader, 3 );
					textureManager.BindImageForShader( "Automata Rule Buffer", "ruleBuffer", shader, 4 );

					// glUniform1iv( glGetUniformLocation( shader, "rule" ), 25, &CAConfig.rule[ 0 ] );

					// dispatch the compute shader - go from back buffer to front buffer
					glDispatchCompute( ( CAConfig.dimensionX + 15 ) / 16, ( CAConfig.dimensionY + 15 ) / 16, 1 );
					glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
				}
			// }
		}

		{ // draw the current state of the front buffer
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );

			// pass the current front buffer, to display it
			glUniform2f( glGetUniformLocation( shaders[ "Draw" ], "resolution" ),
				( float( config.width ) / float( CAConfig.dimensionX ) ) * float( CAConfig.dimensionX ),
				( float( config.height ) / float( CAConfig.dimensionY ) ) * float( CAConfig.dimensionY ) );
			textureManager.BindTexForShader( frontBufferLabel, "CAStateBuffer", shaders[ "Draw" ], 2 );

			// put it in the accumulator
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

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			 for ( int i = 0; i < 5; i++ ) {
			 	string str;
			 	for ( int j = 0; j < 5; j++ ) {
			 		str += " " + to_string( CAConfig.rule[ i * 5 + j ] );
			 	}
			 	textRenderer.DrawBlackBackedString( 6 - i, str + " " );
			 }

			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// buffer swap
		CAConfig.oddFrame = !CAConfig.oddFrame;
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
		HandleTridentEvents();
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	CAHistory engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
