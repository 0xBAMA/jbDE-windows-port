#include "../../../engine/includes.h"
#include "../../../data/colors.h"

#ifndef FONTRENDERER_H
#define FONTRENDERER_H

// double bar frame
constexpr unsigned char TOP_LEFT_DOUBLE_CORNER = 201;
constexpr unsigned char TOP_RIGHT_DOUBLE_CORNER = 187;
constexpr unsigned char BOTTOM_LEFT_DOUBLE_CORNER = 200;
constexpr unsigned char BOTTOM_RIGHT_DOUBLE_CORNER = 188;
constexpr unsigned char VERTICAL_DOUBLE = 186;
constexpr unsigned char HORIZONTAL_DOUBLE = 205;

// single bar frame
constexpr unsigned char TOP_LEFT_SINGLE_CORNER = 218;
constexpr unsigned char TOP_RIGHT_SINGLE_CORNER = 191;
constexpr unsigned char BOTTOM_LEFT_SINGLE_CORNER = 192;
constexpr unsigned char BOTTOM_RIGHT_SINGLE_CORNER = 217;
constexpr unsigned char VERTICAL_SINGLE = ( unsigned char ) 179;
constexpr unsigned char HORIZONTAL_SINGLE = 196;
constexpr unsigned char TEE_UDR = ( unsigned char ) 198;

// curly scroll thingy
constexpr unsigned char CURLY_SCROLL_TOP = 244;
constexpr unsigned char CURLY_SCROLL_BOTTOM = 245;
constexpr unsigned char CURLY_SCROLL_MIDDLE = 179;

// percentage fill blocks
constexpr unsigned char FILL_0 = 32;
constexpr unsigned char FILL_25 = 176;
constexpr unsigned char FILL_50 = 177;
constexpr unsigned char FILL_75 = 178;
constexpr unsigned char FILL_100 = 219;

// some colors
constexpr glm::ivec3 GOLD = glm::ivec3( 191, 146,  23 );
constexpr glm::ivec3 GREEN = glm::ivec3( 100, 186,  20 );
constexpr glm::ivec3 BLUE = glm::ivec3(  50, 103, 184 );
constexpr glm::ivec3 RED = glm::ivec3( 255,   0,   0 );
constexpr glm::ivec3 WHITE = glm::ivec3( 255, 255, 255 );
constexpr glm::ivec3 GREY = glm::ivec3( 169, 169, 169 );
constexpr glm::ivec3 GREY_D = glm::ivec3( 100, 100, 100 );
constexpr glm::ivec3 GREY_DD = glm::ivec3(  50,  50,  50 );
constexpr glm::ivec3 BLACK = glm::ivec3(  16,  16,  16 );

// terminal colors
#define TERMBG	glm::ivec3(  17,  35,  24 )
#define TERMFG	glm::ivec3( 137, 162,  87 )

extern std::vector< paletteEntry > paletteList;

class TextRenderLayer {
public:
	TextRenderLayer ( int32_t w, int32_t h, textureManager_t * tml, string label ) : width( w ), height( h ), textureManager_local( tml ), layerLabel( label ) {
		Resize( w, h );

		// add a texture with the given label
		textureOptions_t opts;
		opts.width = w;
		opts.height = h;
		opts.dataType = GL_RGBA8UI;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.textureType = GL_TEXTURE_2D;
		textureManager_local->Add( label, opts );
	}

	void Resize ( int32_t w, int32_t h ) {
		if ( bufferBase != nullptr ) { free( bufferBase ); }
		bufferBase = ( cChar * ) malloc( sizeof( cChar ) * w * h );
		ClearBuffer();
	}

	void Resend () {
		// this will also need some massaging, with the new texture manager 
		glActiveTexture( GL_TEXTURE2 );
		glBindTexture( GL_TEXTURE_2D, textureManager_local->Get( layerLabel ) );
		if ( bufferDirty ) {
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8UI, width, height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, bufferBase );
			bufferDirty = false;
		}
	}

	void Draw () { // bind the data texture and dispatch
		if ( bufferDirty ) {
			Resend();
		}

		// bind the data texture to slot 1 - this will need massaging eventually to use the new bindsets
		glBindImageTexture( 1, textureManager_local->Get( layerLabel ), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );

		// this is actually very sexy - workgroup is 8x16, same as a glyph's dimensions
		glDispatchCompute( width, height, 1 );
	}

	void ClearBuffer () const {
		size_t numBytes = sizeof( cChar ) * width * height;
		memset( ( void * ) bufferBase, 0, numBytes );
	}

	const cChar GetCharAt ( glm::uvec2 position ) const {
		if ( position.x < width && position.y < height ) // >= 0 is implicit with unsigned
			return *( bufferBase + sizeof( cChar ) * ( position.x + position.y * width ) );
		else
			return cChar();
	}

	void WriteCharAt ( glm::uvec2 position, cChar c ) {
		if ( position.x < width && position.y < height ) {
			int index = position.x + position.y * width;
			bufferBase[ index ] = c;
		}
	}

	void WriteString ( glm::uvec2 min, glm::uvec2 max, std::string str, glm::ivec3 color ) {
		bufferDirty = true;
		glm::uvec2 cursor = min;
		for ( auto c : str ) {
			if ( c == '\t' ) {
				cursor.x += 2;
			} else if ( c == '\n' ) {
				cursor.y++;
				cursor.x = min.x;
				if ( cursor.y >= max.y ) {
					break;
				}
			} else if ( c == 0 ) { // null character, don't draw anything - can use 32 aka space to overwrite with blank
				cursor.x++;
			} else {
				WriteCharAt( cursor, cChar( color, ( unsigned char )( c ) ) );
				cursor.x++;
			}
			if ( cursor.x >= max.x ) {
				cursor.y++;
				cursor.x = min.x;
				if ( cursor.y >= max.y ) {
					break;
				}
			}
		}
	}

	void WriteCCharVector ( glm::uvec2 min, glm::uvec2 max, std::vector< cChar > vec ) {
		bufferDirty = true;
		glm::uvec2 cursor = min;
		for ( unsigned int i = 0; i < vec.size(); i++ ) {
			if ( vec[ i ].data[ 4 ] == '\t' ) {
				cursor.x += 2;
			} else if ( vec[ i ].data[ 4 ] == '\n' ) {
				cursor.y++;
				cursor.x = min.x;
				if ( cursor.y > max.y ) {
					break;
				}
			} else if ( vec[ i ].data[ 4 ] == 0 ) { // special no-write character
				cursor.x++;
			} else {
				WriteCharAt( cursor, vec[ i ] );
				cursor.x++;
			}
			if ( cursor.x > max.x ) {
				cursor.y++;
				cursor.x = min.x;
				if ( cursor.y > max.y ) {
					break;
				}
			}
		}
	}

	void DrawRandomChars ( int n ) {
		bufferDirty = true;
		std::random_device r;
		std::seed_seq s{ r(), r(), r(), r(), r(), r(), r(), r(), r() };
		auto gen = std::mt19937_64( s );
		std::uniform_int_distribution< uint32_t > cDist( 0, 255 );
		std::uniform_int_distribution< uint32_t > xDist( 0, width - 1 );
		std::uniform_int_distribution< uint32_t > yDist( 0, height - 1 );
		for ( int i = 0; i < n; i++ )
			WriteCharAt( glm::uvec2( xDist( gen ), yDist( gen ) ), cChar( glm::ivec3( cDist( gen ), cDist( gen ), cDist( gen ) ), cDist( gen ) ) );
	}

	void DrawDoubleFrame ( glm::uvec2 min, glm::uvec2 max, glm::ivec3 color ) {
		bufferDirty = true;
		WriteCharAt( min, cChar( color, TOP_LEFT_DOUBLE_CORNER ) );
		WriteCharAt( glm::uvec2( max.x, min.y ), cChar( color, TOP_RIGHT_DOUBLE_CORNER ) );
		WriteCharAt( glm::uvec2( min.x, max.y ), cChar( color, BOTTOM_LEFT_DOUBLE_CORNER ) );
		WriteCharAt( max, cChar( color, BOTTOM_RIGHT_DOUBLE_CORNER ) );
		for ( unsigned int x = min.x + 1; x < max.x; x++  ){
			WriteCharAt( glm::uvec2( x, min.y ), cChar( color, HORIZONTAL_DOUBLE ) );
			WriteCharAt( glm::uvec2( x, max.y ), cChar( color, HORIZONTAL_DOUBLE ) );
		}
		for ( unsigned int y = min.y + 1; y < max.y; y++  ){
			WriteCharAt( glm::uvec2( min.x, y ), cChar( color, VERTICAL_DOUBLE ) );
			WriteCharAt( glm::uvec2( max.x, y ), cChar( color, VERTICAL_DOUBLE ) );
		}
	}

	void DrawSingleFrame ( glm::uvec2 min, glm::uvec2 max, glm::ivec3 color ) {
		bufferDirty = true;
		WriteCharAt( min, cChar( color, TOP_LEFT_SINGLE_CORNER ) );
		WriteCharAt( glm::uvec2( max.x, min.y ), cChar( color, TOP_RIGHT_SINGLE_CORNER ) );
		WriteCharAt( glm::uvec2( min.x, max.y ), cChar( color, BOTTOM_LEFT_SINGLE_CORNER ) );
		WriteCharAt( max, cChar( color, BOTTOM_RIGHT_SINGLE_CORNER ) );
		for ( unsigned int x = min.x + 1; x < max.x; x++  ){
			WriteCharAt( glm::uvec2( x, min.y ), cChar( color, HORIZONTAL_SINGLE ) );
			WriteCharAt( glm::uvec2( x, max.y ), cChar( color, HORIZONTAL_SINGLE ) );
		}
		for ( unsigned int y = min.y + 1; y < max.y; y++  ){
			WriteCharAt( glm::uvec2( min.x, y ), cChar( color, VERTICAL_SINGLE ) );
			WriteCharAt( glm::uvec2( max.x, y ), cChar( color, VERTICAL_SINGLE ) );
		}
	}

	void DrawCurlyScroll ( glm::uvec2 start, unsigned int length, glm::ivec3 color ) {
		bufferDirty = true;
		WriteCharAt( start, cChar( color, CURLY_SCROLL_TOP ) );
		for ( unsigned int i = 1; i < length; i++ ) {
			WriteCharAt( start + glm::uvec2( 0, i ), cChar ( color, CURLY_SCROLL_MIDDLE ) );
		}
		WriteCharAt( start + glm::uvec2( 0, length ), cChar( color, CURLY_SCROLL_BOTTOM ) );
	}

	void DrawRectRandom ( glm::uvec2 min, glm::uvec2 max, glm::ivec3 color ) {
		bufferDirty = true;
		std::random_device r;
		std::seed_seq s{ r(), r(), r(), r(), r(), r(), r(), r(), r() };
		auto gen = std::mt19937_64( s );
		std::uniform_int_distribution< uint32_t > fDist( 0, 4 );
		const unsigned char fills[ 5 ] = { FILL_0, FILL_25, FILL_50, FILL_75, FILL_100 };

		for ( unsigned int x = min.x; x <= max.x; x++ ) {
			for ( unsigned int y = min.y; y <= max.y; y++ ) {
				WriteCharAt( glm::uvec2( x, y ), cChar( color, fills[ fDist( gen ) ] ) );
			}
		}
	}

	void DrawRectConstant ( glm::uvec2 min, glm::uvec2 max, cChar c ) {
		bufferDirty = true;
		for ( unsigned int x = min.x; x <= max.x; x++ ) {
			for ( unsigned int y = min.y; y <= max.y; y++ ) {
				WriteCharAt( glm::uvec2( x, y ), c );
			}
		}
	}

	unsigned int width, height;
	textureManager_t * textureManager_local = nullptr;
	bool bufferDirty;
	string layerLabel;
	cChar * bufferBase = nullptr;
};

class layerManager {
public:
	textureManager_t * textureManager_local = nullptr;
	layerManager () {}
	void Init ( int w, int h, GLuint shader ) {
		width = w;
		height = h;

		// how many complete 8x16px glyphs to cover the image ( x and y )
		numBinsWidth = std::floor( width / 8 );
		numBinsHeight = std::floor( height / 16 );

	// Initialize Layers
		// terminal background, foreground, highlight
		layers.push_back( TextRenderLayer( numBinsWidth, numBinsHeight, textureManager_local, "Terminal Background" ) );
		layers.push_back( TextRenderLayer( numBinsWidth, numBinsHeight, textureManager_local, "Terminal Foreground" ) );
		layers.push_back( TextRenderLayer( numBinsWidth, numBinsHeight, textureManager_local, "Terminal Highlight" ) );

		cout << newline << "Text Renderer Created with Dimensions: " << numBinsWidth << " x " << numBinsHeight << newline;

		// hexxDump
		// layers.push_back( TextRenderLayer( numBinsWidth, numBinsHeight ) ); // hexxDump background
		// layers.push_back( TextRenderLayer( numBinsWidth, numBinsHeight ) ); // hexxDump foreground

		// set basepoint for the orientation widget
		basePt = glm::ivec2( 8 * ( numBinsWidth - 20 ), 16 );

		// get the compiled shader
		fontWriteShader = shader;
	}

	void DrawProgressBarString ( int offset, progressBar p ) {
		// background
		layers[ 0 ].DrawRectConstant( glm::uvec2( layers[ 0 ].width - p.currentState().length(), offset ), glm::uvec2( layers[ 0 ].width, offset ), cChar( BLACK, FILL_100 ) );

		float fraction = p.getFraction();

		vec3 color;
		if ( fraction < 0.5f ) {
			color = mix( vec3( 0.57f, 0.00f, 0.29f ), vec3( 0.74f, 0.59f, 0.17f ), fraction * 2.0f );
		} else {
			color = mix( vec3( 0.74f, 0.59f, 0.17f ), vec3( 0.15f, 0.60f, 0.05f ), ( fraction - 0.5f ) * 2.0f );
		}
		ivec3 c = ivec3( color.r * 255.0f, color.g * 255.0f, color.b * 255.0f );

		string combo = p.label + string( "[" );
		string filled = p.getFilledBarPortion();
		string empty = p.getEmptyBarPortion() + string( "] " );
		string percentage = p.getPercentageString();

		int x =  layers[ 1 ].width - p.currentState().length();
		layers[ 1 ].WriteString( glm::uvec2( x, offset ), glm::uvec2( layers[ 1 ].width, offset ), combo, WHITE );

		x += combo.length();
		layers[ 1 ].WriteString( glm::uvec2( x, offset ), glm::uvec2( layers[ 1 ].width, offset ), filled, c );

		x += filled.length();
		layers[ 1 ].WriteString( glm::uvec2( x, offset ), glm::uvec2( layers[ 1 ].width, offset ), empty, WHITE );

		x += empty.length();
		layers[ 1 ].WriteString( glm::uvec2( x, offset ), glm::uvec2( layers[ 1 ].width, offset ), percentage, c );

		x += percentage.length();
		layers[ 1 ].WriteString( glm::uvec2( x, offset ), glm::uvec2( layers[ 1 ].width, offset ), string( "%" ), WHITE );

	}

	void DrawBlackBackedString ( int offset, string s ) {
		// std::stringstream ss;
		// ss << " total: " << std::setw( 10 ) << std::setfill( ' ' ) << std::setprecision( 4 ) << std::fixed << ms << "ms";
		// layers[ 1 ].WriteString( glm::uvec2( layers[ 1 ].width - ss.str().length(), 0 ), glm::uvec2( layers[ 1 ].width, 0 ), ss.str(), WHITE );

		const size_t w = s.length();
		layers[ 0 ].DrawRectConstant( glm::uvec2( layers[ 0 ].width - w, offset ), glm::uvec2( layers[ 0 ].width, offset ), cChar( BLACK, FILL_100 ) );
		layers[ 1 ].WriteString( glm::uvec2( layers[ 1 ].width - w, offset ), glm::uvec2( layers[ 1 ].width, offset ), s, WHITE );

	}

	void DrawBlackBackedColorString ( int offset, string s, glm::vec3 color ) {
		// std::stringstream ss;
		// ss << " total: " << std::setw( 10 ) << std::setfill( ' ' ) << std::setprecision( 4 ) << std::fixed << ms << "ms";
		// layers[ 1 ].WriteString( glm::uvec2( layers[ 1 ].width - ss.str().length(), 0 ), glm::uvec2( layers[ 1 ].width, 0 ), ss.str(), WHITE );

		const size_t w = s.length();
		layers[ 0 ].DrawRectConstant( glm::uvec2( layers[ 0 ].width - w, offset ), glm::uvec2( layers[ 0 ].width, offset ), cChar( BLACK, FILL_100 ) );
		layers[ 1 ].WriteString( glm::uvec2( layers[ 1 ].width - w, offset ), glm::uvec2( layers[ 1 ].width, offset ), s, ivec3( color * 255.0f ) );

	}

	void DrawNoBGColorString ( int offset, string s, glm::vec3 color ) {
		const size_t w = s.length();
		layers[ 1 ].WriteString( glm::uvec2( layers[ 1 ].width - w, offset ), glm::uvec2( layers[ 1 ].width, offset ), s, ivec3( color * 255.0f ) );
	}

	void Clear () {
		for ( auto& layer : layers ) {
			layer.ClearBuffer();
		}
	}

	void Update ( float seconds ) {
		// std::string fps( "60.00 fps " );
		// std::string ms( "16.666 ms " );
		// WriteString( glm::uvec2( width - fps.length(), 1 ), glm::uvec2( width, 1 ), fps, WHITE );
		// WriteString( glm::uvec2( width - ms.length(), 0 ), glm::uvec2( width, 0 ), ms, WHITE );

// little bit of input smoothing, average over NUM_FRAMES_SMOOTHING frames - implementation needs work
		#define NUM_FRAMES_SMOOTHING 5
		float ms = seconds * 1000.0f;
		static std::deque< float > msHistory;
		msHistory.push_back( ms );
		if ( msHistory.size() > NUM_FRAMES_SMOOTHING ) {
			msHistory.pop_front();
		}
		ms = 0.0f;
		for ( unsigned int i = 0; i < msHistory.size(); i++ ) {
			ms += ( msHistory[ i ] / msHistory.size() );
		}

		std::stringstream ss;
		ss << " frame total: " << std::setw( 10 ) << std::setfill( ' ' ) << std::setprecision( 4 ) << std::fixed << ms << "ms";
		layers[ 0 ].DrawRectConstant( glm::uvec2( layers[ 0 ].width - ss.str().length(), 0 ), glm::uvec2( layers[ 0 ].width, 0 ), cChar( BLACK, FILL_100 ) );
		layers[ 1 ].WriteString( glm::uvec2( layers[ 1 ].width - ss.str().length(), 0 ), glm::uvec2( layers[ 1 ].width, 0 ), ss.str(), WHITE );

		// the above writes a string 20 chars long
		// layers[ 0 ].DrawRectConstant( glm::uvec2( layers[ 0 ].width - ss.str().length(), 1 ), glm::uvec2( layers[ 0 ].width, 7 ), cChar( GOLD, FILL_25 ) );
	}

	void Draw ( GLuint writeTarget ) {
		glUseProgram( fontWriteShader );
		// bind the appropriate textures ( atlas( 0 ) + write target( 2 ) )
		glBindImageTexture( 0, textureManager_local->Get( "Font Atlas" ), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
		// this will also need to be massaged a little bit, tbd
		glBindImageTexture( 2, writeTarget, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
		for ( auto layer : layers ) {
			layer.Draw(); // data texture( 1 ) is bound internal to this function, since it is unique to each layer
		}
	}

	int width, height;
	int numBinsWidth;
	int numBinsHeight;

	glm::ivec2 basePt;

	GLuint fontWriteShader;

	// allocation of the textures happens in TextRenderLayer()
	std::vector< TextRenderLayer > layers;

	// HexDump ( SexxDump ) - need to call loadHexx() to get the file data, then you can call drawHexxLayer() to put it in the data texture and load it
	std::vector< uint8_t > hexData;
	void loadHexx ( std::string filename ) {
		std::ifstream in( filename );

		// do not ignore whitespace
		in.unsetf( std::ios::skipws );

		// find the total size of the file
		std::streampos inSize;
		in.seekg( 0, std::ios::end );
		inSize = in.tellg();
		in.seekg( 0, std::ios::beg );

		// preallocate and then load
		hexData.reserve( inSize );
		hexData.insert( hexData.begin(), std::istream_iterator< uint8_t >( in ), std::istream_iterator< uint8_t >() );
	}

	glm::ivec3 getColorForByte ( uint8_t b ) {
		// rethink how this works, once I get the new palette thing working
		return glm::ivec3( 127 + b );
	}

	uint8_t getCharForByte ( uint8_t b ) {
		const bool constrainToAlphanumeric = false;
		if ( constrainToAlphanumeric ) {
			if ( b < 32 || b > 127 ) {
				return '.';
			} else {
				return b;
			}
		} else {
			return b;
		}
	}

	int offset = 0;
	int numColumns = 8;

	void drawHexxLayer () {
		// background layer
		const int fieldWidth = 8 + 8 + ( 3 * 8 + 1 ) * numColumns + 3 + 8 * numColumns + 14;

		// will need to revisit indexing next time I want to use this
		layers[ 2 ].DrawRectConstant( glm::uvec2( 0, 0 ), glm::uvec2( fieldWidth, height ), cChar( BLACK, FILL_100 ) );
		layers[ 3 ].DrawRectConstant( glm::uvec2( 0, 0 ), glm::uvec2( fieldWidth, height ), cChar( BLACK, FILL_100 ) );

		// hex dump layer
		int offsetFromStart = offset;
		for ( int i = numBinsHeight - 6; i >= 6; i-- ) {

			std::stringstream s;

			// draw the address label
			if ( offsetFromStart >= 0 && offsetFromStart < int( hexData.size() ) ) {
				s << std::hex << std::setw( 8 ) << std::setfill( '0' ) << offsetFromStart;
				layers[ 3 ].WriteString( glm::uvec2( 8, i ), glm::uvec2( numBinsWidth, i ), std::string( "0x" ) + s.str(), GREY );

				const int charWriteBasePt = 20 + 3 + ( numColumns * ( 3 * 8 + 1 ) ) + 1;
				layers[ 3 ].WriteCharAt( glm::uvec2( charWriteBasePt - 1, i ), cChar( GREY_D, VERTICAL_SINGLE ) );
				layers[ 3 ].WriteCharAt( glm::uvec2( charWriteBasePt + ( 8 * numColumns ), i ), cChar( GREY_D, VERTICAL_SINGLE ) );

				// write the octets
				for ( int x = 0; x < 8; x++ ) {

					uint8_t currentByte;
					glm::ivec3 currentByteColor;
					uint8_t currentChar;

					for ( int column = 0; column < numColumns; column++ ) {

						int byteIndex = offsetFromStart + x + ( 8 * column );
						if ( byteIndex < int( hexData.size() ) && byteIndex >= 0 ) {

							currentByte = hexData[ byteIndex ];
							currentByteColor = getColorForByte( currentByte );
							currentChar = getCharForByte( currentByte );

							// reset the stringstream
							s.str( std::string() );

							// write for the first set of octets
							s << std::hex << ( ( currentByte >> 4 ) & 0xf ) << ( ( currentByte ) & 0xf );
							layers[ 3 ].WriteString( glm::uvec2( 22 + 3 * x + ( ( 3 * 8 + 1 ) * column ), i ), glm::uvec2( numBinsWidth, i ), s.str(), currentByteColor );

							// write the char interpretation to the right
							layers[ 3 ].WriteCharAt( glm::uvec2( charWriteBasePt + x + 8 * column, i ), cChar( currentByteColor, currentChar ) );
						}
					}
				}
			}
			// increment the pointer
			offsetFromStart += 8 * numColumns;
		}
	}

	void drawTerminal ( terminal_t &t ) {
		// draw terminal state, if active
		if ( t.active ) {
			// clear the area containing the text ( background, then foreground, then cursor layer )
			if ( !t.transparentBackground() ) {
				layers[ 0 ].DrawRectConstant( t.basePt, t.basePt + t.dims, cChar( t.csb.colorsets[ t.csb.selectedPalette ][ 0 ], FILL_100 ) );
			}

			// show the lines of text in the history
			const int sizeHistory = t.history.size();
			for ( int line = sizeHistory - 1 + t.scrollOffset; line >= std::max( int( sizeHistory - t.dims.y ), 0 ); line-- ) {
				if ( ( line - t.scrollOffset ) != std::clamp( line - t.scrollOffset, 0, int( t.history.size() - 1 ) ) ) continue; // no oob reads
				if ( ( -line + sizeHistory ) < 1 ) continue; // don't step on the prompt ( or continue below the prompt )
				layers[ 1 ].WriteCCharVector( t.basePt + uvec2( 0, -line + sizeHistory ), t.basePt + uvec2( t.dims.x, -line + sizeHistory ),
					t.history[ line - t.scrollOffset ] );
			}

			// writing text prompt
			layers[ 1 ].WriteCCharVector( t.basePt, t.basePt + uvec2( t.dims.x, 0 ),
				t.csb.timestamp().append( " " ).append( t.currentLine, 4 ).flush() );

			// writing cursor
			float blink = std::abs( std::sin( SDL_GetTicks() / 1000.0f ) );
			layers[ 2 ].WriteCharAt( t.basePt + uvec2( t.cursorX + 11, 0 ), cChar( ivec3( vec3( t.csb.colorsets[ t.csb.selectedPalette ][ 3 ] ) * blink ), '_' ) );
		}
	}
};

#endif
