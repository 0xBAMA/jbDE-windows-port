#ifndef DEBUG
#define DEBUG

#include "../../engine/includes.h"

static int numMsDelayAfterCallback = 100;

// want to override these with the config
static bool show_high_severity = true;
static bool show_medium_severity = true;
static bool show_low_severity = true;
static bool show_notification_severity = false;

static int severeCallsToKill = 36;

//gl debug dump
void GLAPIENTRY MessageCallback ( 	GLenum source,
						GLenum type,
						GLuint id,
						GLenum severity,
						GLsizei length,
						const GLchar* message,
						const void* userParam ) {

	const char* errorText = ( type == GL_DEBUG_TYPE_ERROR ) ? "** GL ERROR **" : "";

	switch ( severity ) {
		case GL_DEBUG_SEVERITY_HIGH:
			if ( show_high_severity )
				fprintf( stderr, "\t GL CALLBACK: %s type: 0x%x, severity: HIGH, message: %s\n", errorText, type, message );
			if ( !severeCallsToKill-- ) abort(); // kill after N calls to high severity
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			if ( show_medium_severity )
				fprintf( stderr, "\t GL CALLBACK: %s type: 0x%x, severity: MEDIUM, message: %s\n", errorText, type, message );
			break;
		case GL_DEBUG_SEVERITY_LOW:
			if ( show_low_severity )
				fprintf( stderr, "\t GL CALLBACK: %s type: 0x%x, severity: LOW, message: %s\n", errorText, type, message );
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			if ( show_notification_severity )
				fprintf( stderr, "\t GL CALLBACK: %s type: 0x%x, severity: NOTIFICATION, message: %s\n", errorText, type, message );
			break;
	}
	SDL_Delay( numMsDelayAfterCallback ); // hang a short time so the spew doesn't make it impossible to get back to the error
}

void GLDebugEnable () {
	//DEBUG ENABLE
	glEnable( GL_DEBUG_OUTPUT );
	glDebugMessageCallback( MessageCallback, 0 );

	// GLint val[ 3 ];
	// glGetIntegeri_v( GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &val[ 0 ] );
	// glGetIntegeri_v( GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &val[ 1 ] );
	// glGetIntegeri_v( GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &val[ 2 ] );
	// cout << "max work group count is " << val[ 0 ] << " " << val[ 1 ] << " " << val [ 2 ] << endl;
}

#endif
