// CMakeProject3.cpp : Defines the entry point for the application.

#include <includes.h>

bool Running = true;
bool FullScreen = false;

SDL_Window* Window;
SDL_GLContext Context;

int windowWidth = 1920;
int windowHeight = 1080;

int bufferWidth = 1280;
int bufferHeight = 720;

int main () {
	// Create window
	//uint32_t WindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS;
	uint32_t WindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
	Window = SDL_CreateWindow( "OpenGL Test", windowWidth, windowHeight, WindowFlags );

	//SDL_SetHint( SDL_HINT_RENDER_VSYNC, "1" );

	// Create OpenGL context
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 6 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );

	Context = SDL_GL_CreateContext( Window );
	SDL_GL_MakeCurrent( Window, Context );

	// Where am I?
	std::system( "dir" );

	// GLAD for OpenGL function loading
	gladLoadGL();

	cout << endl << T_YELLOW << BOLD << "  jbDE - the jb Demo Engine" << newline;
	cout << " By Jon Baker ( 2020 - 2025 ) " << RESET << newline;
	cout << "  https://jbaker.graphics/ " << newline << newline;

	const GLubyte* vendor = glGetString( GL_VENDOR );
	const GLubyte* renderer = glGetString( GL_RENDERER );
	const GLubyte* version = glGetString( GL_VERSION );
	const GLubyte* glslVersion = glGetString( GL_SHADING_LANGUAGE_VERSION );
	std::cout << "    GPU Info :" << std::endl;
	std::cout << "      Vendor : " << vendor << std::endl;
	std::cout << "      Renderer : " << renderer << std::endl;
	std::cout << "      OpenGL Version Supported : " << version << std::endl;
	std::cout << "      GLSL Version Supported : " << glslVersion << std::endl;
	// Main loop
	while ( Running ) {

		// Event Handling
		SDL_Event Event;
		while ( SDL_PollEvent( &Event ) ) {
			if ( Event.type == SDL_EVENT_KEY_DOWN ) {
				switch ( Event.key.key ) {
				case SDLK_ESCAPE:
					Running = 0;
					break;

				case SDLK_F:
					FullScreen = !FullScreen;
					SDL_SetWindowFullscreen( Window, FullScreen );
					break;

				default:
					break;
				}
			}
			else if ( Event.type == SDL_EVENT_QUIT ) {
				Running = 0;
			}
			else if ( Event.type == SDL_EVENT_WINDOW_RESIZED ) {
				windowWidth = Event.window.data1;
				windowHeight = Event.window.data2;
			}
		}

		// Drawing
			// Clear to constant color
		glViewport( 0, 0, windowWidth, windowHeight );

		// clear not really neccesary, sans depth test
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT );

		SDL_GL_SwapWindow( Window );
	}
	SDL_GL_DestroyContext( Context );
	return 0;
}
