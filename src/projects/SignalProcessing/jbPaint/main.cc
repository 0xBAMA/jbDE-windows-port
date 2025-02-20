#include "../../../engine/engine.h"

// float lerp(f0,f1,p)
// float f0, f1, p;
// {
// 	return ( ( f0 * ( 1.0 - p ) ) + ( f1 * p ) );
// }

float _lerp ( float f0, float f1, float p ){
	return ( ( f0 * ( 1.0 - p ) ) + ( f1 * p ) );
}

struct quad {
	vec2 p1, p2, p3, p4;
};

struct strokeFilter_t {
	float curx, cury;
	float velx, vely, vel;
	float accx, accy, acc;
	float angx, angy;
	float mass, drag;
	float lastx, lasty;
	bool fixedangle = true;

	// previously global state
	float currMass = 0.5f, currDrag = 0.15f;
	float initWidth = 1.5f;
	float odelX, odelY;

	// reset filter parameterization
	void init ( vec2 mouse ) {
		// update positions
		lastx = curx = mouse.x;
		lasty = cury = mouse.y;
		// upate other terms
		velx = accx = odelX = 0.0f;
		vely = accy = odelY = 0.0f;
		active = true;
	}

	// apply the filter - return false on static conditions
	void filterApply ( vec2 mouse ) {
		float mass, drag;
		float fx, fy;

	/* calculate mass and drag */
		mass = _lerp( 1.0f, 160.0f, currMass );
		drag = _lerp( 0.00f, 0.5f, currDrag * currDrag );

	/* calculate force and acceleration */
		fx = mouse.x - curx;
		fy = mouse.y - cury;
		acc = sqrt( fx * fx + fy * fy );
		if ( acc < 0.000001f ) {
			active = false; // computed stroke length goes to zero for low acceleration
			return;
		}
		accx = fx / mass;
		accy = fy / mass;

	/* calculate new velocity */
		velx += accx;
		vely += accy;
		vel = sqrt( velx * velx + vely * vely );
		angx = -vely;
		angy = velx;
		if ( vel < 0.000001f ) {
			active = false; // computed stroke length goes to zero for low velocity
			return;
		}

	/* calculate angle of drawing tool */
		angx /= vel;
		angy /= vel;
		if ( fixedangle ) {
			angx = 0.6f;
			angy = 0.2f;
		}

	/* apply drag */
		velx = velx * ( 1.0f - drag );
		vely = vely * ( 1.0f - drag );

	/* update position */
		lastx = curx;
		lasty = cury;
		curx = curx + velx;
		cury = cury + vely;
	}

	quad currentQuad;
	quad getStrokeQuad () {
		float delx, dely;
		float wid;
		float px, py, nx, ny;

		wid = 0.04f - vel;
		wid = wid * initWidth;
		if( wid < 0.00001f ) {
			wid = 0.00001f;
		}
		delx = angx * wid;
		dely = angy * wid;

		px = lastx;
		py = lasty;
		nx = curx;
		ny = cury;

		// the four points for the quad
		currentQuad.p1 = vec2( px + odelX, py + odelY );
		currentQuad.p2 = vec2( px - odelX, py - odelY );
		currentQuad.p3 = vec2( nx - delx, ny - dely );
		currentQuad.p4 = vec2( nx + delx, ny + dely );

		// updating deltas
		odelY = dely;
		odelX = delx;

		return currentQuad;
	}

	bool active = false;
	void update ( vec2 mouse ) {

		if ( active ) {
			// apply the brush dynamics
			filterApply( mouse );
			// this needs to get drawn, eventually
			getStrokeQuad();
		}
	}
};

struct jbPaintState {
	float brushRadius = 25.0f;
	float brushSlope = 10.0f;
	float brushThreshold = 0.4f;
	vec3 brushColor = vec3( 0.1f );

	strokeFilter_t stroke;
};

class jbPaint final : public engineBase { // sample derived from base engine class
public:
	jbPaint () { Init(); OnInit(); PostInit(); }
	~jbPaint () { Quit(); }

	jbPaintState state;

	void CompileShaders () {
		// something to put some basic data in the accumulator texture
		shaders[ "Draw" ] = computeShader( "../src/projects/SignalProcessing/jbPaint/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "Update" ] = computeShader( "../src/projects/SignalProcessing/jbPaint/shaders/update.cs.glsl" ).shaderHandle;
	}

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			CompileShaders();

			// paintBuffer
			textureOptions_t opts;
			opts.dataType		= GL_RGBA16F;
			opts.width			= config.width;
			opts.height			= config.height;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "Paint Buffer", opts );

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

		{
			ImGui::Begin( "Controls" );
			ImGui::SeparatorText( "Brush" );
			ImGui::SliderFloat( "Radius", &state.brushRadius, 0.001f, 100.0f );
			ImGui::SliderFloat( "Threshold", &state.brushThreshold, 0.0f, 1.0f );
			ImGui::SliderFloat( "Slope", &state.brushSlope, 0.0f, 100.0f );
			ImGui::SliderFloat( "Stroke Width", &state.stroke.initWidth, 0.0f, 5.0f );
			ImGui::ColorEdit3( "Color", ( float * ) &state.brushColor, ImGuiColorEditFlags_PickerHueWheel );
			ImGui::Checkbox( "Fixed Angle", &state.stroke.fixedangle );
			ImGui::Separator();
			ImGui::SliderFloat( "Mass", &state.stroke.currMass, 0.0f, 3.0f );
			ImGui::SliderFloat( "Drag", &state.stroke.currDrag, 0.0f, 1.0f );
			ImGui::End();
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			textureManager.BindImageForShader( "Paint Buffer", "paintBuffer", shader, 2 );

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
			textRenderer.Clear();
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			// show terminal, if active - check happens inside
			textRenderer.drawTerminal( terminal );

			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		if ( inputHandler.getState4( KEY_R ) == KEYSTATE_RISING ) {
			textureManager.ZeroTexture2D( "Paint Buffer" );
		}

		if ( inputHandler.getState4( KEY_Y ) == KEYSTATE_RISING ) {
			CompileShaders();
		}

		const GLuint shader = shaders[ "Update" ];
		glUseProgram( shader );

		textureManager.BindImageForShader( "Blue Noise", "blueNoiseTexture", shader, 0 );
		textureManager.BindImageForShader( "Paint Buffer", "paintBuffer", shader, 2 );

		// to handle more than one state update per frame
		const int numUpdates = 1;
		for ( int i = 0 ; i < numUpdates; i++ ) {

			static rngi noiseOffset = rngi( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

			const float scale = float( std::min( config.width, config.height ) );

			const bool mouseState = inputHandler.getState( MOUSE_BUTTON_LEFT ) && !ImGui::GetIO().WantCaptureMouse;
			ivec2 mousePos = inputHandler.getMousePos();
			vec2 mousePosNorm = vec2( mousePos.x, mousePos.y ) / scale;
			// glUniform3i( glGetUniformLocation( shader, "mouseState" ), mousePos.x, mousePos.y, mouseState );

			if ( mouseState ) {
				switch ( inputHandler.getState4( MOUSE_BUTTON_LEFT ) ) {
					case KEYSTATE_OFF: break;
					case KEYSTATE_RISING:
						// set initial filter parameters
						state.stroke.init( mousePosNorm );
						break;
					case KEYSTATE_ON:
						// update the filter
						state.stroke.update( mousePosNorm + vec2( 0.01f ) );
						break;
					case KEYSTATE_FALLING:
						// state.stroke.active = false;
						break;
				}

				vec2 values[ 4 ] = {
					scale * state.stroke.currentQuad.p1,
					scale * state.stroke.currentQuad.p2,
					scale * state.stroke.currentQuad.p3,
					scale * state.stroke.currentQuad.p4
				};

				glUniform2fv( glGetUniformLocation( shader, "quadPoints" ), 4, ( float* ) &values );

				// glUniform3i( glGetUniformLocation( shader, "mouseState" ), scale * state.stroke.curx, scale * state.stroke.cury, mouseState ? 1 : 0 );
				glUniform3i( glGetUniformLocation( shader, "mouseState" ), scale * state.stroke.lastx, scale * state.stroke.lasty, mouseState ? 1 : 0 );

				glUniform1f( glGetUniformLocation( shader, "brushRadius" ), state.brushRadius * std::min( std::max( ( 1.0f - 29.0f * state.stroke.vel ), 0.1f ), 1.0f ) );
				// glUniform1f( glGetUniformLocation( shader, "brushRadius" ), state.brushRadius );
				glUniform1f( glGetUniformLocation( shader, "brushSlope" ), state.brushSlope );
				glUniform1f( glGetUniformLocation( shader, "brushThreshold" ), state.brushThreshold );
				glUniform3f( glGetUniformLocation( shader, "brushColor" ), state.brushColor.r, state.brushColor.g, state.brushColor.b );

				glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}
		}
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

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal (active check happens inside)
		terminal.update( inputHandler );

		// event handling
		HandleQuitEvents();

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	jbPaint engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
