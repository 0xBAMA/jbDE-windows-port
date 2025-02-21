#ifndef WINDOWHANDLER_H
#define WINDOWHANDLER_H
#include "../includes.h"

class windowHandler {
public:
	windowHandler () {}

	void PreInit () {
		if ( !SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) ) {
			cout << "Error: " << SDL_GetError() << newline;
		}
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,       1 );
		SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE,           8 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE,         8 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE,          8 );
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE,         8 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,        24 );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE,       8 );
		// multisampling AA, for edges when evaluating API geometry
		//if ( config->MSAACount > 1 ) {
		//	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		//	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, config->MSAACount );
		//}
		vsyncState = config->vSyncEnable;
	}

	void Init () {
		// prep for window creation

		// SDL2 version
		//SDL_DisplayMode displayMode;
		//SDL_GetDesktopDisplayMode( 0, &displayMode );

		// SDL3 version gives an array of them
		int numDisplays = 0;
		SDL_DisplayID * displays;
		displays = SDL_GetDisplays( &numDisplays );
		const SDL_DisplayMode * displayMode = SDL_GetCurrentDisplayMode(
			displays[ ( config->startOnScreen < numDisplays ) ? config->startOnScreen : 0 ] );

		// 0 or negative numbers will size the window relative to the display
		config->width = ( config->width <= 0 ) ? displayMode->w + config->width : config->width;
		config->height = ( config->height <= 0 ) ? displayMode->h + config->height : config->height;

		// always need OpenGL, always start hidden till init finishes
		config->windowFlags |= SDL_WINDOW_OPENGL;
		config->windowFlags |= SDL_WINDOW_HIDDEN;

		// SDL3 does not do window placement during window creation, just sizing
		// window = SDL_CreateWindow( config->windowTitle.c_str(), config->windowOffset.x + config->startOnScreen * displayMode.w,
			// config->windowOffset.y, config->width, config->height, config->windowFlags );

		window = SDL_CreateWindow( config->windowTitle.c_str(), config->width, config->height, config->windowFlags );
	}

	void OpenGLSetup () {
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
		// defaults to OpenGL 4.3
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, config->OpenGLVersionMajor );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, config->OpenGLVersionMinor );

		GLcontext = SDL_GL_CreateContext( window );
		SDL_GL_MakeCurrent( window, GLcontext );

		// config vsync enable/disable
		SDL_GL_SetSwapInterval( config->vSyncEnable ? 1 : 0 );

		// load OpenGL functions - now using glad, built from source
		gladLoadGL();

		// basic OpenGL Config
		// glEnable( GL_LINE_SMOOTH );

		if ( config->enableDepthTesting ) { glEnable( GL_DEPTH_TEST ); }
		if ( config->SRGBFramebuffer ) {
			// seems to make things washed out - will need to readjust some stuff but I think this is better
			glEnable( GL_FRAMEBUFFER_SRGB );
			SDL_GL_SetAttribute( SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1 );
		}

		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}

	void ShowIfNotHeadless () {
		// if init takes some time, don't show the window before it's done - also oneShot does not need to ever show the window
		if ( !config->oneShot ) {
			SDL_ShowWindow( window );
		}
	}

	ivec2 GetWindowSize () {
		int x, y;
		SDL_GetWindowSize( window, &x, &y );
		return ivec2( x, y );
	}

	void ToggleVSync () {
		vsyncState = !vsyncState;
		SDL_GL_SetSwapInterval( vsyncState ? 1 : 0 );
	}

	void Swap () {
		SDL_GL_SwapWindow( window );
	}

	void Kill () {
		SDL_GL_DestroyContext( GLcontext );
		SDL_DestroyWindow( window );
		SDL_Quit();
	}

	bool vsyncState = true;
	uint32_t flags;
	SDL_Window * window;
	SDL_GLContext GLcontext;
	configData * config;
};

#endif