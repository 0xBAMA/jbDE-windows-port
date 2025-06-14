#include "../../engine/engine.h"

class LumaMesh final : public engineBase { // sample derived from base engine class
public:
	LumaMesh () { Init(); OnInit(); PostInit(); }
	~LumaMesh () { Quit(); }

	GLuint lineVAO;
	GLuint lineVBO;

	// vertex data
	vector< vec2 > xyPos;
	vector< ivec2 > pixelIdx;

	// state
	float scale = 1.0f;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			shaders[ "Draw" ] = computeShader( "../src/projects/LumaMesh/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Line Draw" ] = regularShader( "../src/projects/LumaMesh/shaders/line.vs.glsl", "../src/projects/LumaMesh/shaders/line.fs.glsl" ).shaderHandle;

			// load the image we want to draw, create a texture
			// Image_4F testImage( "../src/projects/LumaMesh/testImages/circuitBoard.png" );
			Image_4F testImage( "../src/projects/LumaMesh/testImages/waves.png" );
			// Image_4F testImage( "../src/projects/LumaMesh/testImages/crinkle.png" );
			// Image_4F testImage( "../src/projects/LumaMesh/testImages/ISLAND_2.png" );
			testImage.Resize( 2.0f );
			testImage.Swizzle( "rgbl" ); // compute luma term into alpha value

			textureOptions_t opts;
			opts.dataType		= GL_RGBA32F;
			opts.width			= testImage.Width();
			opts.height			= testImage.Height();
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			opts.pixelDataType	= GL_FLOAT;
			opts.initialData	= testImage.GetImageDataBasePtr();
			textureManager.Add( "Displacement Image", opts );

			// create the lines and put them in buffers to draw on the GPU
			const float ratio = float( testImage.Height() ) / float( testImage.Width() );
			for ( int x = 1; x < testImage.Width(); x++ )  {
				for ( int y = 1; y < testImage.Height(); y++ ) {

					ivec2 iA = ivec2( x - 1, y - 1 );
					vec2 pA = vec2( RemapRange( iA.x, 0, testImage.Width(), -1.0f, 1.0f ), RemapRange( iA.y, 0, testImage.Height(), -ratio, ratio ) );

					ivec2 iB = ivec2( x - 1, y );
					vec2 pB = vec2( RemapRange( iB.x, 0, testImage.Width(), -1.0f, 1.0f ), RemapRange( iB.y, 0, testImage.Height(), -ratio, ratio ) );

					ivec2 iC = ivec2( x, y - 1 );
					vec2 pC = vec2( RemapRange( iC.x, 0, testImage.Width(), -1.0f, 1.0f ), RemapRange( iC.y, 0, testImage.Height(), -ratio, ratio ) );

					ivec2 iD = ivec2( x, y );
					vec2 pD = vec2( RemapRange( iD.x, 0, testImage.Width(), -1.0f, 1.0f ), RemapRange( iD.y, 0, testImage.Height(), -ratio, ratio ) );

					// four corners of a square - 5 lines to draw
					/*	C	@=======@ D
							|      /|
							|     / |
							|    /  |
							|   /   |
							|  /    |
							| /     |
							|/      |
						A	@=======@ B --> X */

					// line AB
					xyPos.push_back( pA );
					xyPos.push_back( pB );
					pixelIdx.push_back( iA );
					pixelIdx.push_back( iB );

					// line AC
					xyPos.push_back( pA );
					xyPos.push_back( pC );
					pixelIdx.push_back( iA );
					pixelIdx.push_back( iC );

					// line CD
					xyPos.push_back( pC );
					xyPos.push_back( pD );
					pixelIdx.push_back( iC );
					pixelIdx.push_back( iD );

					// line BD
					xyPos.push_back( pB );
					xyPos.push_back( pD );
					pixelIdx.push_back( iB );
					pixelIdx.push_back( iD );

					// line AD
					xyPos.push_back( pA );
					xyPos.push_back( pD );
					pixelIdx.push_back( iA );
					pixelIdx.push_back( iD );
				}
			}

		// create the vertex buffers
			// we have vec2's for position and ivec2's for pixel index
			glGenVertexArrays( 1, &lineVAO );
			glBindVertexArray( lineVAO );
			glGenBuffers( 1, &lineVBO );
			glBindBuffer( GL_ARRAY_BUFFER, lineVBO );
			glBufferData( GL_ARRAY_BUFFER, sizeof( vec2 ) * xyPos.size() + sizeof( ivec2 ) * pixelIdx.size(), NULL, GL_DYNAMIC_DRAW );
			glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( vec2 ) * xyPos.size(), xyPos.data() );
			glBufferSubData( GL_ARRAY_BUFFER, sizeof( vec2 ) * xyPos.size(), sizeof( ivec2 ) * pixelIdx.size(), pixelIdx.data() );

		// setup the vertex attribs
			glEnableVertexAttribArray( glGetAttribLocation( shaders[ "Line Draw" ], "position" ) );
			glVertexAttribPointer( glGetAttribLocation( shaders[ "Line Draw" ], "position" ), 2, GL_FLOAT, GL_FALSE, 0, ( GLvoid * ) 0 );
			glEnableVertexAttribArray( glGetAttribLocation( shaders[ "Line Draw" ], "pixel" ) );
			glVertexAttribIPointer( glGetAttribLocation( shaders[ "Line Draw" ], "pixel" ), 2, GL_INT, 0, ( GLvoid * ) ( sizeof( vec2 ) * xyPos.size() ) );

		// create the framebuffer
			{
				textureOptions_t opts;

				// ==== Depth =========================
				opts.dataType = GL_DEPTH_COMPONENT32;
				opts.textureType = GL_TEXTURE_2D;
				opts.width = config.width;
				opts.height = config.height;
				textureManager.Add( "Framebuffer Depth", opts );
				// ==== Color =========================
				opts.dataType = GL_RGBA16F;
				textureManager.Add( "Framebuffer Color", opts );
				// ====================================

				// == Framebuffer Objects =============
				glGenFramebuffers( 1, &framebuffer );
				const GLenum bufs[] = { GL_COLOR_ATTACHMENT0 }; // 2x 32-bit primitive ID/instance ID, 2x half float encoded normals

				glBindFramebuffer( GL_FRAMEBUFFER, framebuffer );
				glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager.Get( "Framebuffer Depth" ), 0 );
				glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager.Get( "Framebuffer Color" ), 0 );
				glDrawBuffers( 1, bufs );
				if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
					cout << "framebuffer creation successful" << endl;
				}
			}
		}
	}

	void HandleCustomEvents () { // application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// zoom in and out with plus/minus
		if ( inputHandler.getState( KEY_MINUS ) ) {
			scale /= 0.99f;
		}
		if ( inputHandler.getState( KEY_EQUALS ) ) {
			scale *= 0.99f;
		}
	}

	void ImguiPass () {
		ZoneScoped;

			/*
		ImGui::Begin( "LineSpam", NULL );
		ImGui::Text( "Loaded %d opaque lines", LineSpamConfig.opaqueLines.size() );
		ImGui::Text( "Loaded %d transparent lines", LineSpamConfig.transparentLines.size() );
		ImGui::SliderFloat( "Depth Range", &LineSpamConfig.depthRange, 0.001f, 10.0f );
		ImGui::End();
			*/

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

		// draw the current set of lines
		{
			scopedTimer Start( "Line Draw" );
			const GLuint shader = shaders[ "Line Draw" ];

			glBindFramebuffer( GL_FRAMEBUFFER, framebuffer );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			glUseProgram( shader );
			glBindVertexArray( lineVAO );
			glBindBuffer( GL_ARRAY_BUFFER, lineVBO );

			/*
		// using this requires that the framebuffer be created with multisample buffers... maybe worth playing with at some point
			glEnable( GL_DEPTH_TEST );
			glEnable( GL_MULTISAMPLE );
			glHint(  GL_LINE_SMOOTH_HINT, GL_NICEST );
			glDepthMask( GL_FALSE );
			glEnable( GL_BLEND );
			glEnable( GL_LINE_SMOOTH ); // extremely resource heavy... curious what the actual implementation is
			glDepthFunc( GL_LEQUAL );
			glLineWidth( 0.5f );
			*/

			textureManager.BindImageForShader( "Displacement Image", "displacementImage", shaders[ "Line Draw" ], 0 );

			// perspective projection changes
			rngN jitter( 0.0f, 1.618f );
			glm::mat3 tridentOrientationMatrix = glm::mat3( trident.basisX, trident.basisY, trident.basisZ );
			glm::mat4 transform = glm::mat4( tridentOrientationMatrix ) * glm::scale( vec3( scale ) ) * glm::mat4( 1.0f );
			transform = glm::lookAt( vec3( jitter(), jitter(), -500.0f ), vec3( 0.0f ), vec3( 0.0f, 1.0f, 0.0f ) ) * transform;
			transform = glm::perspective( 0.0025f, float( config.width ) / float( config.height ), 100.0f, 1000.0f ) * transform;
			glUniformMatrix4fv( glGetUniformLocation( shader, "transform" ), 1, GL_FALSE, glm::value_ptr( transform ) );

			glViewport( 0, 0, config.width, config.height );
			glDrawArrays( GL_LINES, 0, xyPos.size() );
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		}

		{ // copy the composited image into accumulatorTexture
			bindSets[ "Drawing" ].apply();
			scopedTimer Start( "Drawing" );
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			textureManager.BindTexForShader( "Framebuffer Depth", "depthResult", shader, 2 );
			textureManager.BindTexForShader( "Framebuffer Color", "colorResult", shader, 3 );
			glUniform1f( glGetUniformLocation( shader, "blendRate" ), blendRate );

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
		*/

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
	LumaMesh engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
