#include <includes.h>

struct cChar {
	unsigned char data[ 4 ] = { 255, 255, 255, 0 };
	cChar() {}
	cChar( unsigned char c ) {
		data[ 3 ] = c;
	}
	cChar( glm::ivec3 color, unsigned char c ) {
		data[ 0 ] = color.x;
		data[ 1 ] = color.y;
		data[ 2 ] = color.z;
		data[ 3 ] = c;
	}
};

struct coloredStringBuilder {
	// cChar data directly
	std::vector< cChar > data;

	static inline int selectedPalette;
	static constexpr ivec3 colorsets[ 4 ][ 5 ] = {
	{ // ===================
		{  17,  35,  24 },
		{  72, 120,  40 },
		{ 137, 162,  87 },
		{ 191, 146,  23 },
		{ 166, 166, 166 } },
	{ // ===================
		{  58,   0,   0 },
		{ 124,   3,   0 },
		{ 224,  66,  23 },
		{ 242, 109,  31 },
		{ 166, 166, 166 } },
	{ // ===================
		{   7,   8,  16 },
		{  82, 165, 222 },
		{ 172, 214, 246 },
		{ 235, 249, 255 },
		{ 166, 166, 166 } },
	{ // ===================
		{  19,   2,   8 },
		{  70,  14,  43 },
		{ 213,  60, 106 },
		{ 255, 130, 116 },
		{ 166, 166, 166 } } };

	// clear out the data
	void reset () { data.clear(); }

// chainable operations:

	// add a colored timestamp
	coloredStringBuilder& timestamp () {
		string timestamp = fixedWidthTimeString();
		append( cChar( colorsets[ selectedPalette ][ 1 ], timestamp[ 0 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 1 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 2 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 1 ], timestamp[ 3 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 4 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 5 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 1 ], timestamp[ 6 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 7 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 8 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 1 ], timestamp[ 9 ] ) );
		return *this;
	}

	// trim N cChars off of the vector
	coloredStringBuilder& trim ( int amt ) {
		for ( int i = 0; i < amt; i++ ) {
			data.pop_back();
		}
		return *this;
	}

	// operate in cChars directly
	coloredStringBuilder& append ( cChar val ) {
		data.push_back( val );
		return *this;
	}

	// basic append with color select
	coloredStringBuilder& append ( string string_in, ivec3 color = colorsets[ selectedPalette ][ 4 ] ) {
		for ( auto& letter : string_in ) {
			cChar val = cChar( color, letter );
			append( val );
		}
		return *this;
	}

	// integer reference of the current palette
	coloredStringBuilder& append ( string string_in, int color ) {
		append( string_in, colorsets[ selectedPalette ][ color ] );
		return *this;
	}

	// other append options?

	// this comes at the end of a chain of operations, where it will return the data and then reset the state
	std::vector< cChar > flush () {

		// this is a hack, I'm not sure why I'm having to pad the string... last character flickers, otherwise
		append( " " ); // probably something to do with some bounds checking somewhere...

		// make a copy of the data
		std::vector<cChar> dataCopy( data );

		// clear the held data
		reset();

		// return the copy
		return dataCopy;
	}
};

// variable/type definition
enum type_e {
	BOOL = 0,

	INT = 1,
	IVEC2 = 2,
	IVEC3 = 3,
	IVEC4 = 4,

	FLOAT = 5,
	VEC2 = 6,
	VEC3 = 7,
	VEC4 = 8,

	STRING = 9,
	CVAR_NAME = 10
};

const static inline string getStringForType ( type_e type_in ) {
	const string typeStrings[] = { "bool", "int", "int2", "int3", "int4", "float", "vec2", "vec3", "vec4", "string", "cvar_name" };
	return typeStrings[ int( type_in ) ];
}

struct var_t {
	string label;
	type_e type;
	vec4 data = vec4( 0.0f );
	string stringData;
	string description;

	var_t() {}
	var_t( string label_in, type_e type_in, string description_in ) :
		label( label_in ), type( type_in ), description( description_in ) {}

	const string getStringRepresentation () const {
		switch ( type ) {
			case BOOL: return ( data.x == 0.0f ) ? string( "false" ) : string( "true" ); break;

			case INT: return to_string( int( data.x ) ); break;
			case IVEC2: return string( "(" ) + to_string( int( data.x ) ) + string( " " ) + to_string( int( data.y ) ) + string( ")" ); break;
			case IVEC3: return string( "(" ) + to_string( int( data.x ) ) + string( " " ) + to_string( int( data.y ) ) + string( " " ) + to_string( int( data.z ) ) + string( ")" ); break;
			case IVEC4: return string( "(" ) + to_string( int( data.x ) ) + string( " " ) + to_string( int( data.y ) ) + string( " " ) + to_string( int( data.z ) ) + string( " " ) + to_string( int( data.w ) ) + string( ")" ); break;

			case FLOAT: return to_string( data.x ); break;
			case VEC2: return string( "(" ) + to_string( data.x ) + string( " " ) + to_string( data.y ) + string( ")" ); break;
			case VEC3: return string( "(" ) + to_string( data.x ) + string( " " ) + to_string( data.y ) + string( " " ) + to_string( data.z ) + string( ")" ); break;
			case VEC4: return string( "(" ) + to_string( data.x ) + string( " " ) + to_string( data.y ) + string( " " ) + to_string( data.z ) + string( " " ) + to_string( data.w ) + string( ")" ); break;

			case STRING: return stringData; break;
			default: return string(); break;
		}
	}
};

struct args_t {
	// list of arguments
	std::vector< var_t > args;

	// construct from vector of var_t's
	args_t () {}
	args_t ( std::vector< var_t > args_in ) : args( args_in ) {}

	// operator to get by string
	var_t dummyVar;
	var_t& operator[] ( string label ) {
		for ( uint32_t i = 0; i < args.size(); i++ ) {
			if ( args[ i ].label == label ) {
				return args[ i ];
			}
		}
		// mostly just for warning supression...
		return dummyVar;
	}

	// by index
	var_t& operator[] ( int i ) {
		return args[ i ];
	}

	const size_t count () const {
		return args.size();
	}
};

// command definition
struct command_t {
	std::vector< string > labelAndAliases;
	args_t args;
	std::function< void( args_t args ) > func;
	string description;

	// does the input string match this command or any of its aliases?
	bool commandStringMatch ( string commandString ) {
		for ( auto& cantidate : labelAndAliases ) {
			if ( commandString == cantidate ) {
				return true;
			}
		}
		return false;
	}

	string getAliasString () {
		string value;
		// we have one or more aliases
		for ( uint32_t i = 0; i < labelAndAliases.size(); i++ ) {
			value += labelAndAliases[ i ] + ", ";
		}
		// last ", " needs to come off
		value.pop_back();
		value.pop_back();
		return value;
	}

	void invoke ( args_t args_in ) const { // use the local arguments?
		func( args_in );
	}
};

struct cvarManager_t {
	var_t dummyVar;
	std::vector< var_t > vars;

	void add ( var_t var ) {
		vars.push_back( var );
	}

	// operator to get by string
	var_t& operator[] ( string label ) {
		for ( uint32_t i = 0; i < vars.size(); i++ ) {
			if ( vars[ i ].label == label ) {
				return vars[ i ];
			}
		}
		// mostly just for warning supression...
		return dummyVar;
	}

	// operator to get by index
	var_t& operator[] ( int i ) {
		return vars[ i ];
	}

	const size_t count () const {
		return vars.size();
	}

	// does this string refer to a valid cvar?
	bool isValid ( string label ) {
		for ( uint32_t i = 0; i < vars.size(); i++ ) {
			if ( vars[ i ].label == label ) {
				return true;
			}
		}
		return false;
	}
};

struct terminal_t {

	// eventually move this stuff onto a struct:
	// =========================================

		// location of lower left corner
		uvec2 basePt = uvec2( 2, 2 );

		// size in glyphs
		uvec2 dims = uvec2( 156, 41 );

		// is this terminal active
		bool active = false;

		// navigation cursor
		int cursorX = 0;
		int cursorY = 0;

	// =========================================

	// formatting utility
	coloredStringBuilder csb;

	// tracking if we need to rebuild the list of strings for tab completion
	DictionaryTrie * trie = new DictionaryTrie();
	DictionaryTrie * trieCvar = new DictionaryTrie();

	// corresponding flat vector, all the strings that got added to that acceleration structure
	std::vector< string > allStrings;

	// lines of history, renderered directly
	std::vector< std::vector< cChar > > history;

	// current input buffer
	string currentLine;

	// add a line to the history
	void addHistoryLine( std::vector< cChar > line ) {
		history.push_back( line );
	}

	void addLineBreak () {
		addHistoryLine( csb.flush() );
	}

	cvarManager_t cvars;
	void addCvar ( string label, type_e type, string description ) {
		// if we haven't seen this one before...
		if ( !cvars.isValid( label ) ) {
			// add it
			cvars.add( var_t( label, type, description ) );

			// insert into the main list, and also a second list, for autocompleting arguments
			trie->insert( label, 100 );
			trieCvar->insert( label, 100 );

			// add this also to the list of all strings, and alphabetize (for reporting on tab complete with empty input prompt)
			// allStrings.push_back( string( "$" ) + label );
			allStrings.push_back( label );
			sort( allStrings.begin(), allStrings.end() );
		}
	}

	// how do we want to expose:

		// assignment of value (handling of different types?)
			// assignment specialization for every type, I guess...

		// retrieval of value
			// this should return a copy of the var_t struct, I think

		// ...

	std::vector< command_t > commands;
	void addCommand ( std::vector< string > commandAndOptionalAliases_in,
		std::vector< var_t > argumentList_in,
		std::function< void( args_t args ) > func_in,
		string description_in ) {

			// fill out the struct
			command_t command;
			command.labelAndAliases = commandAndOptionalAliases_in;
			command.args = args_t( argumentList_in );
			command.func = func_in;
			command.description = description_in;

			commands.push_back( command );

			// add command names - I think, for the report, at least, we want to deduplicate these... if you encounter it already in the list, skip adding it again
			for ( uint32_t i = 0; i < commandAndOptionalAliases_in.size(); i++ ) {

				bool foundAlreadyInList = false;
				for ( auto& s : allStrings )
					if ( s == commandAndOptionalAliases_in[ i ] )
						foundAlreadyInList = true;

				if ( !foundAlreadyInList ) {
					// add it to the autocomplete list
					trie->insert( commandAndOptionalAliases_in[ i ], 100 );

					// add this also to the list of all strings, and alphabetize (for reporting on tab complete with empty input prompt)
					allStrings.push_back( commandAndOptionalAliases_in[ i ] );
					sort( allStrings.begin(), allStrings.end() );
				}
			}
	}

	// transparent background toggle on a cvar...
	bool transparentBackground () {
		return !( cvars[ "terminalTransparent" ].data.x == 0.0f );
	}

	// adding some default set of commands
	void addTerminalLocalCommands () {

		// cannot do e.g. quit here, because of lack of access to pQuit
			// will need to add another step to init, where engine commands are added

		// setting color presets
		addCommand( { "colorPreset" }, {
				{ "select", INT, "Which palette you want to use." }
			}, [=] ( args_t args ) {
				// I would like to have the string indexing back...
				csb.selectedPalette = int( args[ "select" ].data.x );
			}, "Numbered presets (0-3)." );

		addCommand( { "echo", "e" },
			{ // parameters list
				{ "string", STRING, "The string in question." }
			},
			[=] ( args_t args ) {
				history.push_back( csb.append( args[ "string" ].stringData, 4 ).flush() );
			}, "Report back the given string argument." );

		// clearing history
		addCommand( { "clear" }, {},
			[=] ( args_t args ) {
				history.clear();
			}, "Clear the terminal history." );

		addCommand(
			{ "list", "ls" }, {}, // command and aliases, arguments ( if any, none here )
			[=] ( args_t args ) {
				addLineBreak();
				for ( uint32_t i = 0; i < commands.size(); i++ ) {
					addHistoryLine( csb.append( "  ============================================" ).flush() );
					addHistoryLine( csb.append( "  > Command Aliases: " ).append( commands[ i ].getAliasString(), 1 ).flush() );
					addHistoryLine( csb.append( "    Description: " ).append( commands[ i ].description, 1 ).flush() );
					addHistoryLine( csb.append( "    Arguments:" ).flush() );
					if ( commands[ i ].args.count() > 0 ) {
						for ( auto& var : commands[ i ].args.args ) {
							addHistoryLine( csb.append( "      " + var.label + " ( ", 2 ).append( getStringForType( var.type ), 1 ).append( " ): ", 2 ).append( var.description, 1 ).flush() );
						}
						addLineBreak();
					} else {
						addHistoryLine( csb.append( "    None", 1 ).flush() );
						addLineBreak();
					}
				}
			}, "List all the active commands in this terminal." );

		addCvar( "terminalTransparent", BOOL, "Whether to draw transparent or solid color for the background when displaying." );
		cvars[ "terminalTransparent" ].data.x = 1.0f; // would like a "setTrue"... function, maybe?

		addCommand( { "cvars" }, {}, [=] ( args_t args ) {
			addHistoryLine( csb.flush() );
			for ( uint32_t i = 0; i < cvars.count(); i++ ) {
				addHistoryLine( csb.append( "  " + cvars[ i ].label + " ( ", 2 ).append( getStringForType( cvars[ i ].type ), 1 ).append( " )", 2 ).flush() );
				addHistoryLine( csb.append( "    Description: ", 4 ).append( cvars[ i ].description, 1 ).flush() );
				addHistoryLine( csb.append( "    Value: ", 4 ).append( cvars[ i ].getStringRepresentation(), 3 ).flush() );
				addHistoryLine( csb.flush() );
			}
		}, "List all the active cvars, as well as their types and values." );

		// so, looking at the way the call to parseForCommand works, I think we can actually have overloads... as long as one works
			// but it will just pick the first one that works... but I think that's ok because we have type matching and stuff, now

		// because the underlying representation is all the same, we can generalize the lambda for all of these out
		auto assignLambda = [=] ( args_t args ) {
			// trim the dollar signed bit off, so we can consider this a regular cvar label
			if ( args[ "cvar" ].stringData[ 0 ] == '$' ) {
				args[ "cvar" ].stringData.erase( 0, 1 );
			}
			// check for validity
			if ( cvars.isValid( args[ "cvar" ].stringData ) ) {
				if ( args[ "value" ].type == cvars[ args[ "cvar" ].stringData ].type ) {
					cvars[ args[ "cvar" ].stringData ].data.x = args[ "value" ].data.x;
					cvars[ args[ "cvar" ].stringData ].data.y = args[ "value" ].data.y;
					cvars[ args[ "cvar" ].stringData ].data.z = args[ "value" ].data.z;
					cvars[ args[ "cvar" ].stringData ].data.w = args[ "value" ].data.w;
					cvars[ args[ "cvar" ].stringData ].stringData = args[ "value" ].stringData;
				} else {
					// report failure ( type does not match )
					addHistoryLine( csb.append( "  Error: ", 4 ).append( "assignment type mismatch", 3 ).flush() );
				}
			} else {
				// report failure ( cvar not found )
				addHistoryLine( csb.append( "  Error: ", 4 ).append( "cvar \"" + args[ "cvar" ].stringData + "\" not found during assignment", 3 ).flush() );
			}
		};

		// == BOOL ===================================================================================================
		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", BOOL, "The value to assign." )
			}, assignLambda, "Cvar assignment function for bool."
		);

		// == INT ====================================================================================================
		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", INT, "The value to assign." )
			}, assignLambda, "Cvar assignment function for int."
		);

		// == INT VECTORS ============================================================================================
		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", IVEC2, "The value to assign." )
			}, assignLambda, "Cvar assignment function for 2-componenent int vectors."
		);

		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", IVEC3, "The value to assign." )
			}, assignLambda, "Cvar assignment function for 3-componenent int vectors."
		);

		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", IVEC4, "The value to assign." )
			}, assignLambda, "Cvar assignment function for 4-componenent int vectors."
		);

		// == FLOAT ==================================================================================================
		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", FLOAT, "The value to assign." )
			}, assignLambda, "Cvar assignment function for float."
		);

		// == FLOAT VECTORS ==========================================================================================
		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", VEC2, "The value to assign." )
			}, assignLambda, "Cvar assignment function for 2-componenent float vectors."
		);

		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", VEC3, "The value to assign." )
			}, assignLambda, "Cvar assignment function for 3-componenent float vectors."
		);

		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", VEC4, "The value to assign." )
			}, assignLambda, "Cvar assignment function for 4-componenent float vectors."
		);

		// == STRING =================================================================================================
		addCommand( { "assign" },
			{	var_t( "cvar", CVAR_NAME, "The name of the cvar to assign to." ),
				var_t( "value", STRING, "The value to assign." )
			}, assignLambda, "Cvar assignment function for strings."
		);
	}

	int isInt ( const char a[] ) {
		int len = strlen( a );
		int minus = 0;
		int dsum = 0;
		for ( int i = 0; i < len; i++ ) {
			if ( isdigit( a[ i ] ) != 0 ) {
				dsum++;
			} else if ( a[ i ] == '-' ) {
				minus++;
			}
		}
		if ( dsum + minus == len ) {
			return 1;
		} else {
			return 0;
		}
	}

	int isFloat ( const char a[], bool requireDot = false ) {
		int len = strlen( a );
		int dsum = 0;
		int dot = requireDot ? -1 : 0;
		int minus = 0;
		for ( int i = 0; i < len; i++ ) {
			if ( isdigit( a[ i ] ) != 0 ) {
				dsum++;
			} else if ( a[ i ] == '.' ) {
				dot++;
			} else if ( a[ i ] == '-' ) {
				minus++;
			}
		}
		if ( dsum + dot + minus == len ) {
			return 1;
		} else {
			return 0;
		}
	}

	// is this valid input for this command
	bool parseForCommand ( int commandIdx, string argumentString, bool verbose = false ) {

		// so we have matched with command number commandIdx

		// we need to see if we can parse its arguments
		std::stringstream argstream( argumentString );
		bool fail = false;
		int failureMode = -1;

		// for each argument
		for ( uint32_t i = 0; i < commands[ commandIdx ].args.count() && !fail; i++ ) {

			// temp buffers to read into
			bool tempb = false;
			int tempi = 0;
			float tempf = 0.0f;
			string temps;

			if ( commands[ commandIdx ].args[ i ].type == CVAR_NAME ) {

				// we want to read in just a string, we aren't going to dereference the cvar value
				if ( ( argstream >> temps ) ) {
					commands[ commandIdx ].args[ i ].stringData = temps;
				} else {
					failureMode = 0; // cvar name parse error
					argstream.clear();
					fail = true;
				}

			} else {

				// signalling value for a cvar
				if ( argstream.peek() == '$' ) {

					// read in the string, including this signalling character
					string cantidateString;
					argstream >> cantidateString;

					// get rid of '$'
					cantidateString.erase( 0, 1 );

					// so we want to see that it's an existing cvar
					if ( cvars.isValid( cantidateString ) ) {
						// check for matching type
						if ( commands[ commandIdx ].args[ i ].type == cvars[ cantidateString ].type ) {
							// then copy the value(s) from the cvar
							commands[ commandIdx ].args[ i ].data = cvars[ cantidateString ].data;
							commands[ commandIdx ].args[ i ].stringData = cvars[ cantidateString ].stringData;
						} else {
							// types do not match
							failureMode = 1; // cvar types do not match
							argstream.clear();
							fail = true;
						}
					} else {
						// we had some issue, this isn't a valid cvar
						failureMode = 2;
						argstream.clear();
						fail = true;
					}

				} else {

					int count = 0;
					switch ( commands[ commandIdx ].args[ i ].type ) { // for each arg
						case BOOL:
							// read in a bool, put it in x
							if ( ( argstream >> std::boolalpha >> tempb ) ) {
								commands[ commandIdx ].args[ i ].data[ 0 ] = tempb ? 1.0f : 0.0f;
							} else {
								failureMode = 3; // bool failure
								argstream.clear();
								fail = true;
							}
						break;

						case IVEC4:	count++;
						case IVEC3:	count++;
						case IVEC2:	count++;
						case INT:	count++;
							// read in up to four ints, put it in xyzw
							for ( int j = 0; j < count; j++ ) {
								if ( ( argstream >> temps ) ) {

									if ( isFloat( temps.c_str() ) && !isInt( temps.c_str() ) ) {
										failureMode = 5; // float encountered when expecting int
										fail = true;
										argstream.clear();
									}

									// then read in an int and assign it
									if ( isInt( temps.c_str() ) ) {
										size_t pos;
										tempi = stoi( temps, &pos );
										commands[ commandIdx ].args[ i ].data[ j ] = tempi;
									} else {
										failureMode = 5; // int failure mode
										fail = true;
										argstream.clear();
									}
								} else {
									failureMode = 5; // int failure mode
									argstream.clear();
									fail = true;
								}
							}
						break;

						case VEC4:	count++;
						case VEC3:	count++;
						case VEC2:	count++;
						case FLOAT:	count++;
							// read in up to four floats, put it in xyzw
							for ( int j = 0; j < count; j++ ) {
								if ( ( argstream >> temps ) ) {

									// if it's an int, not a float, when we expect a float
									if ( isInt( temps.c_str() ) ) {
										failureMode = 6; // int-as-float failure mode
										argstream.clear();
										fail = true;
									}

									// we need to read in this float, if it's judged valid
									if ( isFloat( temps.c_str() ) ) {
										size_t posf;
										tempf = stof( temps, &posf );
										commands[ commandIdx ].args[ i ].data[ j ] = tempf;
									} else {
										failureMode = 7; // float failure mode
										argstream.clear();
										fail = true;
									}
								} else {
									failureMode = 7; // float failure mode
									argstream.clear();
									fail = true;
								}
							}
						break;

						case STRING:
							if ( ( argstream >> temps ) ) {
								if ( isFloat( temps.c_str() ) || isInt( temps.c_str() ) ) {
								// if this string is a number, we don't want to accept it
									failureMode = 9; // recieved numbers instead
									argstream.clear();
									fail = true;
								} else if ( temps == string( "true" ) || temps == string( "false" ) ) {
								// if this string is "true" or "false", we also don't want to accept it
									failureMode = 10; // recieved bool instead
									argstream.clear();
									fail = true;
								} else {
									commands[ commandIdx ].args[ i ].stringData = temps;
								}
							} else {
								failureMode = 8; // string failure mode
								argstream.clear();
								fail = true;
							}
						break;

						default:
						break;
					}
				}
			}
		}

		if ( !fail ) { // most cases, if we failed, we already .clear()'ed the string
			string temps;
			if ( ( argstream >> temps ) ) { // if there is input remaining, when we have finished the loop
				// stray characters at end of input failure mode
				failureMode = 11;
				fail = true;
			}
		}

		if ( fail || failureMode != -1 ) {
			if ( verbose ) {
				const string errorList[] = {
					"cvar name parse error",
					"cvar types do not match",
					"cvar invalid",
					"bool parse error",
					"float encountered when expecting int",
					"int parse error",
					"int encountered when expecting float",
					"float parse error",
					"string parse error",
					"number passed as string argument",
					"bool passed as string argument",
					"stray characters at end of input"
				};
				addHistoryLine( csb.append( errorList[ failureMode ] ).flush() );
			}
			return false;
		} else {
			return true;
		}
	}

	// init with startup message
	terminal_t () {
		// some initial setup - set color palette to be used
		csb.selectedPalette = 0;

		// for autofilling true and false
		trie->insert( "true", 100 );
		trie->insert( "false", 100 );
		trieCvar->insert( "true", 100 );
		trieCvar->insert( "false", 100 );

		// output welcome string - want to do this up a bit
		addHistoryLine( csb.append( " jbDE - the jb Demo Engine", 4 ).flush() );
		addHistoryLine( csb.append( "By Jon Baker ( 2020 - 2024 )", 4 ).flush() );
		addHistoryLine( csb.append( " https://jbaker.graphics/" ).flush() );
		addLineBreak();
		addHistoryLine( csb.timestamp().append( " Welcome to jbDE" ).flush() );

		// add terminal-local commands and cvars
			// manipulating base point, width, height, etc
		addTerminalLocalCommands();

		// some of these things changing will involve clearing the entire buffer in the text renderer
			// so either: signal that... or be clearing every time already anyways because it's not a major perf hit
	}

	// handle input, etc
	void update ( inputHandler_t &currentInputState ) {
		ZoneScoped;

		// only want to handle this stuff if the terminal is active...
		if ( active ) {

		// modifier keys
			const bool control = currentInputState.getState( KEY_LEFT_CTRL ) || currentInputState.getState( KEY_RIGHT_CTRL );
			const bool shift = currentInputState.getState( KEY_LEFT_SHIFT ) || currentInputState.getState( KEY_RIGHT_SHIFT );

		// navigation through history
			if ( currentInputState.getState4( KEY_UP ) == KEYSTATE_RISING ) { cursorUp(); }
			if ( currentInputState.getState4( KEY_DOWN ) == KEYSTATE_RISING ) { cursorDown(); }

		// scrolling
			if ( currentInputState.getState4( KEY_PAGEUP ) == KEYSTATE_RISING ) { pageup( shift ); }
			if ( currentInputState.getState4( KEY_PAGEDOWN ) == KEYSTATE_RISING ) { pagedown( shift ); }

		// tab
			if ( currentInputState.getState4( KEY_TAB ) == KEYSTATE_RISING ) { tab(); };

		// navigation within line
			if ( currentInputState.getState4( KEY_LEFT ) == KEYSTATE_RISING ) { cursorLeft( control ); }
			if ( currentInputState.getState4( KEY_RIGHT ) == KEYSTATE_RISING ) { cursorRight( control ); }
			if ( currentInputState.getState4( KEY_HOME ) == KEYSTATE_RISING ) { home(); }
			if ( currentInputState.getState4( KEY_END ) == KEYSTATE_RISING ) { end(); }

		// control inputs
			if ( currentInputState.getState4( KEY_BACKSPACE ) == KEYSTATE_RISING ) { backspace( control ); }
			if ( currentInputState.getState4( KEY_ENTER ) == KEYSTATE_RISING ) { enter(); }
			if ( currentInputState.getState4( KEY_DELETE ) == KEYSTATE_RISING ) { deleteKey( control ); }
			if ( currentInputState.getState4( KEY_SPACE ) == KEYSTATE_RISING ) { addChar( ' ' ); }

		// char input
			if ( currentInputState.stateBuffer[ currentInputState.currentOffset ].alphasActive() ) { // is this check useful?
				string letterString = string( " abcdefghijklmnopqrstuvwxyz" );
				for ( int i = 1; i <= 26; i++ ) {
					if ( currentInputState.getState4( ( keyName_t ) i ) == KEYSTATE_RISING ) {
						addChar( shift ? toupper( letterString[ i ] ) : letterString[ i ] );
					}
				}
			}

		// numbers
			string numberString = string( "0123456789" );
			string shiftedString = string( ")!@#$%^&*(" );
			for ( int i = 27; i < 37; i++ ) {
				if ( currentInputState.getState4( ( keyName_t ) i ) == KEYSTATE_RISING ) {
					addChar( shift ? shiftedString[ i - 27 ] : numberString[ i - 27 ] );
				}
			}

		// punctuation and assorted other shit
			if ( currentInputState.getState4( KEY_PERIOD ) == KEYSTATE_RISING ) { addChar( shift ? '>' : '.' ); }
			if ( currentInputState.getState4( KEY_COMMA ) == KEYSTATE_RISING ) { addChar( shift ? '<': ',' ); }
			if ( currentInputState.getState4( KEY_SLASH ) == KEYSTATE_RISING ) { addChar( shift ? '?': '/' ); }
			if ( currentInputState.getState4( KEY_SEMICOLON ) == KEYSTATE_RISING ) { addChar( shift ? ':': ';' ); }
			if ( currentInputState.getState4( KEY_BACKSLASH ) == KEYSTATE_RISING ) { addChar( shift ? '|' : '\\' ); }
			if ( currentInputState.getState4( KEY_LEFT_BRACKET ) == KEYSTATE_RISING ) { addChar( shift ? '{' : '[' ); }
			if ( currentInputState.getState4( KEY_RIGHT_BRACKET ) == KEYSTATE_RISING ) { addChar( shift ? '}' : ']' ); }
			if ( currentInputState.getState4( KEY_APOSTROPHE ) == KEYSTATE_RISING ) { addChar( shift ? '"' : '\'' ); }
			if ( currentInputState.getState4( KEY_GRAVE ) == KEYSTATE_RISING ) { addChar( shift ? '~' : '`' ); }
			if ( currentInputState.getState4( KEY_MINUS ) == KEYSTATE_RISING ) { addChar( shift ? '_' : '-' ); }
			if ( currentInputState.getState4( KEY_EQUALS ) == KEYSTATE_RISING ) { addChar( shift ? '+' : '=' ); }

		}

		// want to toggle the currently active state, when we hit f10 - but that has to happen, even if the terminal is not active
		if ( currentInputState.getState4( KEY_F10 ) == KEYSTATE_RISING ) { active = !active; }
	}

	void addChar ( char c ) {
		// add this char at the cursor
		resetInputHistoryBrowse();
		currentLine.insert( currentLine.begin() + cursorX, c );
		cursorX++;
	}

	// helper function for control modifier on nav + backspace/delete
	bool isDivider ( char c ) {
		// tbd, which ones of these to use
		string dividers = string( " ,./\\'\"!@#$%^&*()_+{}[]/?-=<>:;|`~" );
		for ( auto& symbol : dividers )
			if ( c == symbol )
				return true;
		return false;
	}

	// helper function for tab complete
	string getLastToken ( string str ) {
		while ( !str.empty() && std::isspace( str.back() ) )
			str.pop_back() ; // remove trailing white space

		const auto pos = str.find_last_of( " \t\n" ) ; // locate the last white space

		// if not found, return the entire string else return the tail after the space
		return pos == std::string::npos ? str : str.substr( pos + 1 ) ;
	}

	// cursor keys - up/down history navigation
	int historyOffset = 0;
	string clicache;
	std::vector< string > userInputs;

	void resetInputHistoryBrowse() {
		// whatever needs to happen to reset
		historyOffset = 0; // this is probably it...
	}

	void cursorUp () {
		if ( userInputs.empty() ) return; // no history, nothing to navigate
		if ( historyOffset == 0 ) { // you are at the original prompt
			clicache = currentLine; // so you need to cache before proceeding
		}
		// offset, clamp, and update currentLine
		historyOffset++;
		historyOffset = std::clamp( historyOffset, 0, int( userInputs.size() ) );
		currentLine = userInputs[ userInputs.size() - historyOffset ];
		end(); // go to end of line
	}

	void cursorDown () {
		if ( userInputs.empty() ) return; // no history, no op
		if ( historyOffset == 0 ) return; // at the prompt, this is not meaningful
		if ( historyOffset == 1 ) {	// if we are one above the prompt
			// restoring cached line
			currentLine = clicache;
			historyOffset = 0;
		} else {
			// offset, clamp, and update currentLine
			historyOffset--;
			historyOffset = std::clamp( historyOffset, 0, int( userInputs.size() ) );
			currentLine = userInputs[ userInputs.size() - historyOffset ];
		}
		end(); // go to end of line
	}

	void cursorLeft ( bool control ) {
		if ( control ) {
			do {
				cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
			} while ( !isDivider( currentLine[ cursorX - 1 ] ) && ( cursorX - 1 >= 0 ) );
		} else {
			cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
		}
	}

	void cursorRight ( bool control ) {
		if ( control ) {
			do {
				cursorX = std::clamp( cursorX + 1, 0, int( currentLine.length() ) );
			} while ( !isDivider( currentLine[ cursorX + 1 ] ) && ( cursorX + 1 < int( currentLine.length() ) ) );
		} else {
			cursorX = std::clamp( cursorX + 1, 0, int( currentLine.length() ) );
		}
	}

	// scrolling the output up and down, to see further back in time
	int scrollOffset = 0;
	void pageup ( bool shift ) { scrollOffset += shift ? 10 : 1; }
	void pagedown ( bool shift ) { scrollOffset -= shift ? 10 : 1; if ( scrollOffset < 0 ) scrollOffset = 0; }

	// more navigation
	void home () { cursorX = 0; }
	void end () { cursorX = currentLine.length(); }

	// further manipulation of current input line
	void backspace ( bool control ) {
		resetInputHistoryBrowse();
		if ( control ) {
			if ( currentLine.length() > 0 ) {
				do { // erase till you hit whitespace
					cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
					currentLine.erase( currentLine.begin() + cursorX );
				} while ( !isDivider( currentLine[ cursorX - 1 ] ) && ( cursorX - 1 >= 0 ) );
			}
		} else { // remove char before the cursor
			cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
			if ( currentLine.length() > 0 ) {
				currentLine.erase( currentLine.begin() + cursorX );
			}
		}
	}

	void deleteKey ( bool control ) {
		resetInputHistoryBrowse();
		if ( control ) {
			if ( currentLine.length() > 0 ) {
				do {
					if ( currentLine.length() > 0 && cursorX < int( currentLine.length() ) ) {
						currentLine.erase( currentLine.begin() + cursorX );
					}
				} while ( !isDivider( currentLine[ cursorX ] ) && cursorX < int( currentLine.length() ) );
			}
		} else {
			// remove the char at the cursor
			if ( currentLine.length() > 0 && cursorX < int( currentLine.length() ) ) {
				currentLine.erase( currentLine.begin() + cursorX );
			}
		}
	}

	void tab () {

		// get the last token from the current input + report all strings matching the current token
		int count = 0;
		std::stringstream ss;
		string lastToken = getLastToken( currentLine );

		bool didTrimWhitespace = ( lastToken[ lastToken.length() - 1 ] != currentLine[ currentLine.length() - 1 ] );
		bool lastTokenMarkedCvar = ( lastToken[ 0 ] == '$' );

		// we trimmed whitespace... or the lastToken is marked with the leading "$", in either case list cvars
		if ( didTrimWhitespace || lastTokenMarkedCvar ) {

			// holding the list of cvars
			std::vector< string > output;

			// list matching cvars, assuming we have at least one character after the "$"
			if ( lastTokenMarkedCvar && ( lastToken.length() > 1 ) ) {
				// match the partial string
				output = trieCvar->predictCompletions( lastToken.substr( 1 ), 16 );

				if ( output.size() == 1 ) { // if we only got one completion back, I want to say, ok, you asked for that one
					if ( currentLine.length() == lastToken.length() ) { // the last token is the whole line, just replace
						currentLine = output[ 0 ];
					} else { // if this last token is only part of the current input line, we should trim that off, before adding the suggested string
						currentLine.erase( currentLine.end() - lastToken.length(), currentLine.end() );
						currentLine = currentLine + "$" + output[ 0 ];
					}
					end(); // put the cursor at the end of the line
				}
			}

			// we didn't get any best matches... and we did trim whitespace, which means we are operating from no data
			// or, we have just a "$" at the prompt right now...
			// both cases, we just want to output all the cvars ( as many as are available )
			if ( ( output.empty() && didTrimWhitespace ) || lastToken == string( "$" ) ) {
				for ( uint32_t i = 0; i < cvars.count(); i++ ) {
					output.push_back( cvars[ i ].label );
				}
			}

			// output the list of all the cvars, with leading dollar signs
			for ( uint32_t i = 0; i < output.size(); i++ ) {
				count++;
				if ( count < 10 ) { // we only want to show a maximum of 10
					csb.append( string( "$" ), 2 ).append( output[ i ] + " ", 4 );
				} else if ( count < 11 ) { // append an elipsis if we have a list that would be continuing past 10
					csb.append( "...", 4 );
				}
			}

			// if we added any to the list, go ahead and show them
			if ( output.size() > 0 ) {
				addHistoryLine( csb.flush() );
			}

		} else {

			// we want to consider all possible matching completions, not just matching cvars
			std::vector< string > output = trie->predictCompletions( lastToken, 16 );

			if ( output.size() == 1 ) { // if we only got one completion back, I want to say, ok, you asked for that one
				if ( currentLine.length() == lastToken.length() ) { // the last token is the whole line, just replace
					currentLine = output[ 0 ];
				} else { // if this last token is only part of the current input line, we should trim that off, before adding the suggested string
					currentLine.erase( currentLine.end() - lastToken.length(), currentLine.end() );
					currentLine = currentLine + ( cvars.isValid( output[ 0 ] ) ? "$" : string( "" ) ) + output[ 0 ];
				}
				end(); // put the cursor at the end of the line
			}

			for ( auto& s : output ) {
				count++;
				ss << s << " ";
				if ( count > 10 ) {
					break;
				}
			}

			// handles empty prompt
			if ( currentLine.size() == 0 ) {
				for ( auto& s : allStrings ) {
					count++;
					ss << s << " ";
					if ( count > 10 ) {
						break;
					}
				}
			}

			// if we had more than 10 strings match, indicate this
			if ( count > 10 ) {
				ss << "... ";
			}

			addHistoryLine( csb.append( ss.str(), 2 ).flush() );
		}
	}

	void enter () {
		// add a timestamped history element
		addHistoryLine( csb.timestamp().append( " " ).append( currentLine ).flush() );

		// cache the current input line as a string
		string userInputString = currentLine;

		// clear the input, reset cursor, scroll offset
		currentLine.clear();
		cursorX = 0;
		scrollOffset = 0;

		// reset input history browsing state, as well
		resetInputHistoryBrowse();

		// nonzero length
		if ( userInputString.length() ) {

			// add this to the input history
			userInputs.push_back( userInputString );

			// now breaking this up a little bit - we consider this sequence up to the first space as the command
			string commandText = userInputString.substr( 0, userInputString.find( " " ) );
			string argumentText = userInputString.erase( 0, commandText.length() + 1 );

			// todo - try to match a cvar
				// if you do, report the type and value of that cvar - require "$"?

			// try to match a command
			bool commandFound = false;
			bool commandInvoked = false;
			for ( uint32_t i = 0; i < commands.size(); i++ ) {
				// if you do, try to parse the rest of the input string for the arguments to that command
				if ( commands[ i ].commandStringMatch( commandText ) ) {
					commandFound = true;

				// // debug
				// 	addHistoryLine( csb.append( "Found matching command " + commandText ).flush() );
				// 	addHistoryLine( csb.append( "  Arguments:" ).flush() );
				// 	for ( auto& var : commands[ i ].args.args ) {
				// 		addHistoryLine( csb.append( "    " + var.label + " ( ", 2 ).append( getStringForType( var.type ), 1 ).append( " ): ", 2 ).append( var.description, 1 ).flush() );
				// 	}

					if ( parseForCommand( i, argumentText ) ) {
						// we successfully parsed the arguments for the command, so we can go ahead and run it
						commands[ i ].invoke( commands[ i ].args );
						commandInvoked = true;
					}
				}
			}

			if ( commandFound && !commandInvoked ) {

				// do the full report...
				addLineBreak();
				addHistoryLine( csb.append( "  Error: ", 4 ).append( "command \"" + commandText + "\" argument parse failed...", 3 ).flush() );
				addLineBreak();
				addHistoryLine( csb.append( "  Cantidates:" ).flush() );
				for ( uint32_t i = 0; i < commands.size(); i++ ) {
					if ( commands[ i ].commandStringMatch( commandText ) ) {
						if ( commands[ i ].args.count() != 0 ) {
							addLineBreak();
							addHistoryLine( csb.append( "  Command Aliases: " ).append( commands[ i ].getAliasString(), 1 ).flush() );
							addHistoryLine( csb.append( "  Description: " ).append( commands[ i ].description, 1 ).flush() );
							addHistoryLine( csb.append( "  Arguments:" ).flush() );
							for ( auto& var : commands[ i ].args.args ) {
								addHistoryLine( csb.append( "    " + var.label + " ( ", 2 ).append( getStringForType( var.type ), 1 ).append( " ): ", 2 ).append( var.description, 1 ).flush() );
							}
							addLineBreak();
						}
					}
				}
			}

			if ( !commandFound ) {
				addHistoryLine( csb.append( "Error: ", 4 ).append( "command " + commandText + " not found", 3 ).flush() );
			}
		}
	}
};