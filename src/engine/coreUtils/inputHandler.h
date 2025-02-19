#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H
#include <includes.h>

// key enum
enum keyName_t {
	KEY_NONE = 0,

	// alphas
	KEY_A = 1,
	KEY_B = 2,
	KEY_C = 3,
	KEY_D = 4,
	KEY_E = 5,
	KEY_F = 6,
	KEY_G = 7,
	KEY_H = 8,
	KEY_I = 9,
	KEY_J = 10,
	KEY_K = 11,
	KEY_L = 12,
	KEY_M = 13,
	KEY_N = 14,
	KEY_O = 15,
	KEY_P = 16,
	KEY_Q = 17,
	KEY_R = 18,
	KEY_S = 19,
	KEY_T = 20,
	KEY_U = 21,
	KEY_V = 22,
	KEY_W = 23,
	KEY_X = 24,
	KEY_Y = 25,
	KEY_Z = 26,

	// numerics
	KEY_0 = 27,
	KEY_1 = 28,
	KEY_2 = 29,
	KEY_3 = 30,
	KEY_4 = 31,
	KEY_5 = 32,
	KEY_6 = 33,
	KEY_7 = 34,
	KEY_8 = 35,
	KEY_9 = 36,

	// assorted punctuation + control
	KEY_ENTER = 37,
	KEY_ESCAPE = 38,
	KEY_BACKSPACE = 39,
	KEY_TAB = 40,
	KEY_SPACE = 41,
	KEY_MINUS = 42,
	KEY_EQUALS = 43,
	KEY_LEFT_BRACKET = 44,
	KEY_RIGHT_BRACKET = 45,
	KEY_BACKSLASH = 46,
	KEY_SEMICOLON = 47,
	KEY_APOSTROPHE = 48,
	KEY_VERTICAL_BAR = 49,
	KEY_GRAVE = 50,
	KEY_COMMA = 51,
	KEY_PERIOD = 52,
	KEY_SLASH = 53,
	KEY_HOME = 54,
	KEY_END = 55,
	KEY_PAGEUP = 56,
	KEY_PAGEDOWN = 57,
	KEY_DELETE = 58,

	// there's more... but not super worried about them right now

	// cursor keys
	KEY_UP = 59,
	KEY_DOWN = 60,
	KEY_LEFT = 61,
	KEY_RIGHT = 62,

	// keypad stuff
	KEY_KP_DIVIDE = 63,
	KEY_KP_MULTIPLY = 64,
	KEY_KP_MINUS = 65,
	KEY_KP_PLUS = 66,
	KEY_KP_ENTER = 67,
	KEY_KP_1 = 68,
	KEY_KP_2 = 69,
	KEY_KP_3 = 70,
	KEY_KP_4 = 71,
	KEY_KP_5 = 72,
	KEY_KP_6 = 73,
	KEY_KP_7 = 74,
	KEY_KP_8 = 75,
	KEY_KP_9 = 76,
	KEY_KP_0 = 77,
	KEY_KP_PERIOD = 78,
	KEY_KP_EQUALS = 79,
	KEY_KP_COMMA = 80,

	// F keys
	KEY_F1 = 81,
	KEY_F2 = 82,
	KEY_F3 = 83,
	KEY_F4 = 84,
	KEY_F5 = 85,
	KEY_F6 = 86,
	KEY_F7 = 87,
	KEY_F8 = 88,
	KEY_F9 = 89,
	KEY_F10 = 90,
	KEY_F11 = 91,
	KEY_F12 = 92,

	// modifier keys
	KEY_LEFT_CTRL = 93,
	KEY_LEFT_SHIFT = 94,
	KEY_LEFT_ALT = 95,
	KEY_LEFT_SUPER = 96,
	KEY_RIGHT_CTRL = 97,
	KEY_RIGHT_SHIFT = 98,
	KEY_RIGHT_ALT = 99,
	KEY_RIGHT_SUPER = 100,

	// 5 mouse buttons
	MOUSE_BUTTON_LEFT = 101,
	MOUSE_BUTTON_MIDDLE = 102,
	MOUSE_BUTTON_RIGHT = 103,
	MOUSE_BUTTON_X1 = 104,
	MOUSE_BUTTON_X2 = 105,

	// count
	NUM_INPUTS = 106
};

// keeping the bits for the state of keyboard and mouse
struct keyboardState_t {

	// when did this happen
	std::chrono::time_point<std::chrono::system_clock> timestamp;

	// where was the mouse when it happened
	ivec2 mousePosition = ivec2( 0.0f );

	// keyboard + mouse state
	uint32_t data[ 4 ] = { 0 };

	void reset () {
		data[ 0 ] = data[ 1 ] = data[ 2 ] = data[ 3 ] = 0;
	}

	ivec2 getLocation ( keyName_t key ) {
		int keyIdx = ( int ) key;
		return ivec2( keyIdx / 32, keyIdx % 32 );
	}

	void setState ( keyName_t key ) {
		ivec2 loc = getLocation( key );
		data[ loc.x ] = data[ loc.x ] | ( 1 << loc.y );
	}

	bool getState ( keyName_t key ) {
		ivec2 loc = getLocation( key );
		return ( data[ loc.x ] >> loc.y ) & 1;
	}

	const bool alphasActive () const {
		const uint32_t alphaMask = 0b0000'0111'1111'1111'1111'1111'1111'1110;
		return data[ 0 ] & alphaMask;
	}
};

constexpr int KEYSTATE_ON = 0;
constexpr int KEYSTATE_OFF = 1;
constexpr int KEYSTATE_RISING = 2;
constexpr int KEYSTATE_FALLING = 3;

struct inputHandler_t {

	// state vector (allocated ring buffer for N frames history)
	static constexpr int numStateBuffers = 16;
	keyboardState_t stateBuffer[ numStateBuffers ];

	// ring buffer offset
	int currentOffset = 0;

	// update the ring buffer offset
	void incrementBufferIdx () {
		currentOffset = ( currentOffset + 1 ) % numStateBuffers;
	}

	// integer index into ring buffer, relative to current offset
	const int32_t getBufferOffsetFromCurrent ( int offset ) const {
		offset = currentOffset + offset;
		while ( offset < 0 ) offset += numStateBuffers;
		return ( offset % numStateBuffers );
	}

	// update function
	void update () {
		// increment the ring buffer index, current becomes previous
		incrementBufferIdx();

		// init current state with zeroes
		stateBuffer[ currentOffset ].reset();

		// get the state from SDL2, and set the individual elements according to their current state
		const bool * state = SDL_GetKeyboardState( NULL );

		// alphas
		if ( state[ SDL_SCANCODE_A ] ) stateBuffer[ currentOffset ].setState( KEY_A );
		if ( state[ SDL_SCANCODE_B ] ) stateBuffer[ currentOffset ].setState( KEY_B );
		if ( state[ SDL_SCANCODE_C ] ) stateBuffer[ currentOffset ].setState( KEY_C );
		if ( state[ SDL_SCANCODE_D ] ) stateBuffer[ currentOffset ].setState( KEY_D );
		if ( state[ SDL_SCANCODE_E ] ) stateBuffer[ currentOffset ].setState( KEY_E );
		if ( state[ SDL_SCANCODE_F ] ) stateBuffer[ currentOffset ].setState( KEY_F );
		if ( state[ SDL_SCANCODE_G ] ) stateBuffer[ currentOffset ].setState( KEY_G );
		if ( state[ SDL_SCANCODE_H ] ) stateBuffer[ currentOffset ].setState( KEY_H );
		if ( state[ SDL_SCANCODE_I ] ) stateBuffer[ currentOffset ].setState( KEY_I );
		if ( state[ SDL_SCANCODE_J ] ) stateBuffer[ currentOffset ].setState( KEY_J );
		if ( state[ SDL_SCANCODE_K ] ) stateBuffer[ currentOffset ].setState( KEY_K );
		if ( state[ SDL_SCANCODE_L ] ) stateBuffer[ currentOffset ].setState( KEY_L );
		if ( state[ SDL_SCANCODE_M ] ) stateBuffer[ currentOffset ].setState( KEY_M );
		if ( state[ SDL_SCANCODE_N ] ) stateBuffer[ currentOffset ].setState( KEY_N );
		if ( state[ SDL_SCANCODE_O ] ) stateBuffer[ currentOffset ].setState( KEY_O );
		if ( state[ SDL_SCANCODE_P ] ) stateBuffer[ currentOffset ].setState( KEY_P );
		if ( state[ SDL_SCANCODE_Q ] ) stateBuffer[ currentOffset ].setState( KEY_Q );
		if ( state[ SDL_SCANCODE_R ] ) stateBuffer[ currentOffset ].setState( KEY_R );
		if ( state[ SDL_SCANCODE_S ] ) stateBuffer[ currentOffset ].setState( KEY_S );
		if ( state[ SDL_SCANCODE_T ] ) stateBuffer[ currentOffset ].setState( KEY_T );
		if ( state[ SDL_SCANCODE_U ] ) stateBuffer[ currentOffset ].setState( KEY_U );
		if ( state[ SDL_SCANCODE_V ] ) stateBuffer[ currentOffset ].setState( KEY_V );
		if ( state[ SDL_SCANCODE_W ] ) stateBuffer[ currentOffset ].setState( KEY_W );
		if ( state[ SDL_SCANCODE_X ] ) stateBuffer[ currentOffset ].setState( KEY_X );
		if ( state[ SDL_SCANCODE_Y ] ) stateBuffer[ currentOffset ].setState( KEY_Y );
		if ( state[ SDL_SCANCODE_Z ] ) stateBuffer[ currentOffset ].setState( KEY_Z );

		// numerics
		if ( state[ SDL_SCANCODE_0 ] ) stateBuffer[ currentOffset ].setState( KEY_0 );
		if ( state[ SDL_SCANCODE_1 ] ) stateBuffer[ currentOffset ].setState( KEY_1 );
		if ( state[ SDL_SCANCODE_2 ] ) stateBuffer[ currentOffset ].setState( KEY_2 );
		if ( state[ SDL_SCANCODE_3 ] ) stateBuffer[ currentOffset ].setState( KEY_3 );
		if ( state[ SDL_SCANCODE_4 ] ) stateBuffer[ currentOffset ].setState( KEY_4 );
		if ( state[ SDL_SCANCODE_5 ] ) stateBuffer[ currentOffset ].setState( KEY_5 );
		if ( state[ SDL_SCANCODE_6 ] ) stateBuffer[ currentOffset ].setState( KEY_6 );
		if ( state[ SDL_SCANCODE_7 ] ) stateBuffer[ currentOffset ].setState( KEY_7 );
		if ( state[ SDL_SCANCODE_8 ] ) stateBuffer[ currentOffset ].setState( KEY_8 );
		if ( state[ SDL_SCANCODE_9 ] ) stateBuffer[ currentOffset ].setState( KEY_9 );

		// assorted punctuation + control
		if ( state[ SDL_SCANCODE_RETURN ] ) stateBuffer[ currentOffset ].setState( KEY_ENTER );
		if ( state[ SDL_SCANCODE_ESCAPE ] ) stateBuffer[ currentOffset ].setState( KEY_ESCAPE );
		if ( state[ SDL_SCANCODE_BACKSPACE ] ) stateBuffer[ currentOffset ].setState( KEY_BACKSPACE );
		if ( state[ SDL_SCANCODE_TAB ] ) stateBuffer[ currentOffset ].setState( KEY_TAB );
		if ( state[ SDL_SCANCODE_SPACE ] ) stateBuffer[ currentOffset ].setState( KEY_SPACE );
		if ( state[ SDL_SCANCODE_MINUS ] ) stateBuffer[ currentOffset ].setState( KEY_MINUS );
		if ( state[ SDL_SCANCODE_EQUALS ] ) stateBuffer[ currentOffset ].setState( KEY_EQUALS );
		if ( state[ SDL_SCANCODE_LEFTBRACKET ] ) stateBuffer[ currentOffset ].setState( KEY_LEFT_BRACKET );
		if ( state[ SDL_SCANCODE_RIGHTBRACKET ] ) stateBuffer[ currentOffset ].setState( KEY_RIGHT_BRACKET );
		if ( state[ SDL_SCANCODE_BACKSLASH ] ) stateBuffer[ currentOffset ].setState( KEY_BACKSLASH );
		if ( state[ SDL_SCANCODE_SEMICOLON ] ) stateBuffer[ currentOffset ].setState( KEY_SEMICOLON );
		if ( state[ SDL_SCANCODE_APOSTROPHE ] ) stateBuffer[ currentOffset ].setState( KEY_APOSTROPHE );
		if ( state[ SDL_SCANCODE_NONUSBACKSLASH ] ) stateBuffer[ currentOffset ].setState( KEY_VERTICAL_BAR );
		if ( state[ SDL_SCANCODE_GRAVE ] ) stateBuffer[ currentOffset ].setState( KEY_GRAVE );
		if ( state[ SDL_SCANCODE_COMMA ] ) stateBuffer[ currentOffset ].setState( KEY_COMMA );
		if ( state[ SDL_SCANCODE_PERIOD ] ) stateBuffer[ currentOffset ].setState( KEY_PERIOD );
		if ( state[ SDL_SCANCODE_SLASH ] ) stateBuffer[ currentOffset ].setState( KEY_SLASH );
		if ( state[ SDL_SCANCODE_HOME ] ) stateBuffer[ currentOffset ].setState( KEY_HOME );
		if ( state[ SDL_SCANCODE_END ] ) stateBuffer[ currentOffset ].setState( KEY_END );
		if ( state[ SDL_SCANCODE_PAGEUP ] ) stateBuffer[ currentOffset ].setState( KEY_PAGEUP );
		if ( state[ SDL_SCANCODE_PAGEDOWN ] ) stateBuffer[ currentOffset ].setState( KEY_PAGEDOWN );
		if ( state[ SDL_SCANCODE_DELETE ] ) stateBuffer[ currentOffset ].setState( KEY_DELETE );
		if ( state[ SDL_SCANCODE_UP ] ) stateBuffer[ currentOffset ].setState( KEY_UP );
		if ( state[ SDL_SCANCODE_DOWN ] ) stateBuffer[ currentOffset ].setState( KEY_DOWN );
		if ( state[ SDL_SCANCODE_LEFT ] ) stateBuffer[ currentOffset ].setState( KEY_LEFT );
		if ( state[ SDL_SCANCODE_RIGHT ] ) stateBuffer[ currentOffset ].setState( KEY_RIGHT );

		// keypad stuff
		if ( state[ SDL_SCANCODE_KP_DIVIDE ] ) stateBuffer[ currentOffset ].setState( KEY_KP_DIVIDE );
		if ( state[ SDL_SCANCODE_KP_MULTIPLY ] ) stateBuffer[ currentOffset ].setState( KEY_KP_MULTIPLY );
		if ( state[ SDL_SCANCODE_KP_MINUS ] ) stateBuffer[ currentOffset ].setState( KEY_KP_MINUS );
		if ( state[ SDL_SCANCODE_KP_PLUS ] ) stateBuffer[ currentOffset ].setState( KEY_KP_PLUS );
		if ( state[ SDL_SCANCODE_KP_ENTER ] ) stateBuffer[ currentOffset ].setState( KEY_KP_ENTER );
		if ( state[ SDL_SCANCODE_KP_PERIOD ] ) stateBuffer[ currentOffset ].setState( KEY_KP_PERIOD );
		if ( state[ SDL_SCANCODE_KP_EQUALS ] ) stateBuffer[ currentOffset ].setState( KEY_KP_EQUALS );
		if ( state[ SDL_SCANCODE_KP_COMMA ] ) stateBuffer[ currentOffset ].setState( KEY_KP_COMMA );
		if ( state[ SDL_SCANCODE_KP_1 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_1 );
		if ( state[ SDL_SCANCODE_KP_2 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_2 );
		if ( state[ SDL_SCANCODE_KP_3 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_3 );
		if ( state[ SDL_SCANCODE_KP_4 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_4 );
		if ( state[ SDL_SCANCODE_KP_5 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_5 );
		if ( state[ SDL_SCANCODE_KP_6 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_6 );
		if ( state[ SDL_SCANCODE_KP_7 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_7 );
		if ( state[ SDL_SCANCODE_KP_8 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_8 );
		if ( state[ SDL_SCANCODE_KP_9 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_9 );
		if ( state[ SDL_SCANCODE_KP_0 ] ) stateBuffer[ currentOffset ].setState( KEY_KP_0 );

		// F keys
		if ( state[ SDL_SCANCODE_F1 ] ) stateBuffer[ currentOffset ].setState( KEY_F1 );
		if ( state[ SDL_SCANCODE_F2 ] ) stateBuffer[ currentOffset ].setState( KEY_F2 );
		if ( state[ SDL_SCANCODE_F3 ] ) stateBuffer[ currentOffset ].setState( KEY_F3 );
		if ( state[ SDL_SCANCODE_F4 ] ) stateBuffer[ currentOffset ].setState( KEY_F4 );
		if ( state[ SDL_SCANCODE_F5 ] ) stateBuffer[ currentOffset ].setState( KEY_F5 );
		if ( state[ SDL_SCANCODE_F6 ] ) stateBuffer[ currentOffset ].setState( KEY_F6 );
		if ( state[ SDL_SCANCODE_F7 ] ) stateBuffer[ currentOffset ].setState( KEY_F7 );
		if ( state[ SDL_SCANCODE_F8 ] ) stateBuffer[ currentOffset ].setState( KEY_F8 );
		if ( state[ SDL_SCANCODE_F9 ] ) stateBuffer[ currentOffset ].setState( KEY_F9 );
		if ( state[ SDL_SCANCODE_F10 ] ) stateBuffer[ currentOffset ].setState( KEY_F10 );
		if ( state[ SDL_SCANCODE_F11 ] ) stateBuffer[ currentOffset ].setState( KEY_F11 );
		if ( state[ SDL_SCANCODE_F12 ] ) stateBuffer[ currentOffset ].setState( KEY_F12 );

		// modifier keys
		if ( state[ SDL_SCANCODE_LCTRL ] ) stateBuffer[ currentOffset ].setState( KEY_LEFT_CTRL );
		if ( state[ SDL_SCANCODE_LSHIFT ] ) stateBuffer[ currentOffset ].setState( KEY_LEFT_SHIFT );
		if ( state[ SDL_SCANCODE_LALT ] ) stateBuffer[ currentOffset ].setState( KEY_LEFT_ALT );
		if ( state[ SDL_SCANCODE_LGUI ] ) stateBuffer[ currentOffset ].setState( KEY_LEFT_SUPER );
		if ( state[ SDL_SCANCODE_RCTRL ] ) stateBuffer[ currentOffset ].setState( KEY_RIGHT_CTRL );
		if ( state[ SDL_SCANCODE_RSHIFT ] ) stateBuffer[ currentOffset ].setState( KEY_RIGHT_SHIFT );
		if ( state[ SDL_SCANCODE_RALT ] ) stateBuffer[ currentOffset ].setState( KEY_RIGHT_ALT );
		if ( state[ SDL_SCANCODE_RGUI ] ) stateBuffer[ currentOffset ].setState( KEY_RIGHT_SUPER );

		// update mouse
		vec2 mouseLoc;
		uint32_t mouseState = SDL_GetMouseState( &mouseLoc.x, &mouseLoc.y );
		stateBuffer[ currentOffset ].mousePosition = ivec2( mouseLoc );

		// mouse buttons
		if ( mouseState & SDL_BUTTON_LMASK ) stateBuffer[ currentOffset ].setState( MOUSE_BUTTON_LEFT );
		if ( mouseState & SDL_BUTTON_MMASK ) stateBuffer[ currentOffset ].setState( MOUSE_BUTTON_MIDDLE );
		if ( mouseState & SDL_BUTTON_RMASK ) stateBuffer[ currentOffset ].setState( MOUSE_BUTTON_RIGHT );
		if ( mouseState & SDL_BUTTON_X1MASK ) stateBuffer[ currentOffset ].setState( MOUSE_BUTTON_X1 );
		if ( mouseState & SDL_BUTTON_X2MASK ) stateBuffer[ currentOffset ].setState( MOUSE_BUTTON_X2 );

		if ( getState4( MOUSE_BUTTON_LEFT ) == KEYSTATE_RISING ) {
			// when you see the left mouse button go down...
			clickPush = mouseLoc;
			dragging = true;
		} else if ( getState4( MOUSE_BUTTON_LEFT ) == KEYSTATE_FALLING ) {
			// and when the mouse is released...
			dragging = false;
		}

		// update the timestamp
		stateBuffer[ currentOffset ].timestamp =  std::chrono::system_clock::now();
	}

	bool dragging = false;
	ivec2 clickPush;
	const ivec2 mouseDragDelta () const {
		if ( dragging == true ) {
			return stateBuffer[ currentOffset ].mousePosition - clickPush;
		} else {
			return ivec2( 0 );
		}
	}

	const ivec2 getMousePos () const {
		return stateBuffer[ currentOffset ].mousePosition;
	}

	// get instant state
	const bool getState ( keyName_t key ) {
		return stateBuffer[ currentOffset ].getState( key );
	}

	// get state, able to represent rising and falling edge
	int getState4 ( keyName_t key ) {
		bool previousState = stateBuffer[ getBufferOffsetFromCurrent( -1 ) ].getState( key );
		bool currentState = stateBuffer[ currentOffset ].getState( key );

		// want to be able to represent the first time that a key is seen, for something like events
		if ( previousState && currentState )
			// -------- previously on, currently on
			return KEYSTATE_ON;
		else if ( !previousState && currentState )
			// ____/--- rising edge: previously off, currently on
			return KEYSTATE_RISING;
		else if ( previousState && !currentState )
			// ----\___ falling edge: previously on, currently off
			return KEYSTATE_FALLING;
		else
			// ________ previously off, currently off
			return KEYSTATE_OFF;
	}

	// get state ( soft ) - averaging over the history window, return a fraction of ( on / total )
	float getState_soft( keyName_t key ) {
		float avg = 0.0f;
		for ( int i = 0; i < numStateBuffers; i++ )
			avg += stateBuffer[ getBufferOffsetFromCurrent( -i ) ].getState( key ) ? 1.0f : 0.0f;
		return avg / float( numStateBuffers );
	}

	float millisecondsSinceLastUpdate () const {
		auto tStart = stateBuffer[ getBufferOffsetFromCurrent( -1 ) ].timestamp;
		auto tEnd = stateBuffer[ getBufferOffsetFromCurrent( 0 ) ].timestamp;
		return std::chrono::duration_cast< std::chrono::microseconds >( tEnd - tStart ).count() / 1000.0f;
	}
};

#endif