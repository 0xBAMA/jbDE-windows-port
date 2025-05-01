#include "../../../engine/engine.h"
#include "generate.h"

struct ChorizoConfig_t {
	GLuint vao;
	GLuint shapeParametersBuffer;
	GLuint boundsTransformBuffer;
	GLuint pointSpriteParametersBuffer;
	GLuint lightsBuffer;
	GLuint primaryFramebuffer[ 2 ];

	vec3 eyePosition = vec3( -1.0f, 0.0f, -3.0f );
	mat4 viewTransform;
	mat4 viewTransformInverse;
	mat4 projTransform;
	mat4 projTransformInverse;
	mat4 combinedTransform;
	mat4 combinedTransformInverse;
	bool enableThinLens = false;

	vec3 basisX = vec3( 1.0f, 0.0f, 0.0f );
	vec3 basisY = vec3( 0.0f, 1.0f, 0.0f );
	vec3 basisZ = vec3( 0.0f, 0.0f, 1.0f );

	float nearZ = 0.1f;
	float farZ = 100.0f;

	float FoV = 45.0f;

	float volumetricStrength = 0.03f;
	vec3 volumetricColor = vec3( 0.1f, 0.2f, 0.3f );
	vec3 ambientColor = vec3( 0.0 );

	int numLights = 64;
	std::vector< vec4 > lights;

	bool progressiveBlend = false;
	int progressiveFrameCount = 0;

	float blendAmount = 0.1f;
	float focusDistance = 3.5f;
	float apertureAdjust = 0.01f;

	// separate palettes for each generator
	int lightPaletteID = 0;
	float lightPaletteMin = 0.0f;
	float lightPaletteMax = 1.0f;

	int paletteID = 0;
	float paletteMin = 0.0f;
	float paletteMax = 1.0f;

	rngi wangSeeder = rngi( 0, 10042069 );

	geometryManager_t geometryManager;
	int numPrimitives = 0;
	int numPointSprites = 0;
	uint32_t frameCount = 0;
};

class Chorizo final : public engineBase {
public:
	Chorizo () { Init(); OnInit(); PostInit(); }
	~Chorizo () { Quit(); }

	ChorizoConfig_t ChorizoConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "../src/projects/Impostors/Chorizo/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Bounds" ] = computeShader( "../src/projects/Impostors/Chorizo/shaders/bounds.cs.glsl" ).shaderHandle;
			shaders[ "Animate" ] = computeShader( "../src/projects/Impostors/Chorizo/shaders/animate.cs.glsl" ).shaderHandle;

			// setup raster shaders
			shaders[ "Bounding Box" ] = regularShader(
				"../src/projects/Impostors/Chorizo/shaders/bbox.vs.glsl",
				"../src/projects/Impostors/Chorizo/shaders/bbox.fs.glsl"
			).shaderHandle;
			shaders[ "Point Sprite" ] = regularShader(
				"../src/projects/Impostors/Chorizo/shaders/pointSprite.vs.glsl",
				"../src/projects/Impostors/Chorizo/shaders/pointSprite.fs.glsl"
			).shaderHandle;

			// create and bind a basic vertex array
			glCreateVertexArrays( 1, &ChorizoConfig.vao );
			glBindVertexArray( ChorizoConfig.vao );

			// single sided polys - only draw those which are facing outwards
			glEnable( GL_CULL_FACE );
			glCullFace( GL_BACK );
			glFrontFace( GL_CCW );
			glDisable( GL_BLEND );
			glEnable( GL_PROGRAM_POINT_SIZE );

			// get some initial data for the impostor shapes
			rngi pick = rngi( 0, palette::paletteListLocal.size() - 1 );
			uint32_t colorCap = 14;
			do { ChorizoConfig.paletteID = pick(); } while ( palette::paletteListLocal[ ChorizoConfig.paletteID ].colors.size() > colorCap );
			do { ChorizoConfig.lightPaletteID = pick(); } while ( palette::paletteListLocal[ ChorizoConfig.lightPaletteID ].colors.size() > colorCap );

			AddLights();
			Generate();
			PrepGeometrySSBOs();

			// == Render Framebuffer(s) ===========
			textureOptions_t opts;
			// ==== Depth =========================
			opts.dataType = GL_DEPTH_COMPONENT32;
			opts.textureType = GL_TEXTURE_2D;
			opts.width = config.width;
			opts.height = config.height;
			textureManager.Add( "Framebuffer Depth 0", opts );
			textureManager.Add( "Framebuffer Depth 1", opts );
			// ==== Primitive ID ==================
			opts.dataType = GL_RG32UI;
			textureManager.Add( "Framebuffer Primitive ID 0", opts );
			textureManager.Add( "Framebuffer Primitive ID 1", opts );
			// ====================================

			// setup the buffers for the rendering process - front and back framebuffers
			glGenFramebuffers( 2, &ChorizoConfig.primaryFramebuffer[ 0 ] );
			const GLenum bufs[] = { GL_COLOR_ATTACHMENT0 }; // both framebuffers have implicit depth + 1 color attachment

			for ( int i = 0; i < 2; i++ ) {
				// creating the actual framebuffers with their attachments
				glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ i ] );
				glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager.Get( "Framebuffer Depth " + std::to_string( i ) ), 0 );
				glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager.Get( "Framebuffer Primitive ID " + std::to_string( i ) ), 0 );
				glDrawBuffers( 1, bufs ); // how many active attachments, and their attachment locations
				if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
					cout << newline << "framebuffer " << i << " creation successful" << newline;
				}
			}

			// bind default framebuffer
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		}
	}

	void Generate () {
		palette::PaletteIndex = ChorizoConfig.paletteID;

		rng pick = rng( -1.0f, 1.0f );
		rng colorPick = rng( ChorizoConfig.paletteMin, ChorizoConfig.paletteMax );

		for ( int i = 0; i < 100; i++ ) {
			vec3 location = vec3( pick(), pick(), pick() );
			ChorizoConfig.geometryManager.AddPointSpriteSphere( location, 0.005f, palette::paletteRef( colorPick() ) );

			vec3 point1 = vec3( pick(), pick(), pick() );
			vec3 point2 = vec3( pick(), pick(), pick() );
			ChorizoConfig.geometryManager.AddCapsule( point1, point2, 0.01f, palette::paletteRef( colorPick() ) );
		}
	}

	void AddLights() {
		// add some point sprite spheres to indicate light positions
		palette::PaletteIndex = ChorizoConfig.lightPaletteID;
		rng dist = rng( ChorizoConfig.lightPaletteMin, ChorizoConfig.lightPaletteMax );

		const int numLightsPerSide = 4;
		const int w = 512;
		const int h = 512;
		const vec2 scale = vec2( 4.0f );

		for ( int x = 1; x <= w; x += w / ( numLightsPerSide ) ) {
			for ( int y = 1; y <= h; y += h / numLightsPerSide ) {

				vec3 position = vec3(
					( float( x - 1 ) / float( w ) - 0.5f ) * scale.x,
					( float( y - 1 ) / float( h ) - 0.5f ) * scale.y, -1.0f ).yzx();
				vec3 color = palette::paletteRef( dist() );

				const float r = -0.013f;
				ChorizoConfig.geometryManager.AddPointSpriteSphere( position, r, color );

				ChorizoConfig.lights.push_back( vec4( position, 0.0f ) );
				ChorizoConfig.lights.push_back( vec4( color / 3.0f, 0.0f ) );
			}
		}
	}

	void PrepGeometrySSBOs () {
		static bool firstTime = true;
		
		ChorizoConfig.numPrimitives = ChorizoConfig.geometryManager.count;
		cout << newline << "Created " << ChorizoConfig.numPrimitives << " primitives" << newline;

		// create the transforms buffer
		if ( firstTime ) {
			glGenBuffers( 1, &ChorizoConfig.boundsTransformBuffer );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.boundsTransformBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( mat4 ) * ChorizoConfig.numPrimitives, NULL, GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, ChorizoConfig.boundsTransformBuffer );

		// shape parameterization buffer
		if ( firstTime ) {
			glGenBuffers( 1, &ChorizoConfig.shapeParametersBuffer );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.shapeParametersBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * ChorizoConfig.numPrimitives * 4, ( GLvoid * ) ChorizoConfig.geometryManager.parametersList.data(), GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, ChorizoConfig.shapeParametersBuffer );

		ChorizoConfig.numPointSprites = ChorizoConfig.geometryManager.countPointSprite;
		cout << newline << "Created " << ChorizoConfig.numPointSprites << " point sprites" << newline;

		// point sprite spheres, separate from the bounding box impostors
		if ( firstTime ) {
			glGenBuffers( 1, &ChorizoConfig.pointSpriteParametersBuffer );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.pointSpriteParametersBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * ChorizoConfig.numPointSprites * 4, ( GLvoid * ) ChorizoConfig.geometryManager.pointSpriteParametersList.data(), GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, ChorizoConfig.pointSpriteParametersBuffer );

		// setup the SSBO, with the initial data
		ChorizoConfig.numLights = ChorizoConfig.lights.size() / 2;
		if ( firstTime ) {
			glGenBuffers( 1, &ChorizoConfig.lightsBuffer );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.lightsBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * ChorizoConfig.numLights * 2, ( GLvoid * ) ChorizoConfig.lights.data(), GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, ChorizoConfig.lightsBuffer );

		firstTime = false;
	}

	vec2 UniformSampleHexagon ( vec2 u ) {
		u = 2.0f * u - vec2( 1.0f );
		float a = sqrt( 3.0f ) - sqrt( 3.0f - 2.25f * abs( u.x ) );
		return vec2( glm::sign( u.x ) * a, u.y * ( 1.0f - a / sqrt( 3.0f ) ) );
	}

	void PrepSceneParameters () {
		// const float time = SDL_GetTicks() / 10000.0f;
		static rng jitterAmount = rng( 0.0f, 1.0f );
		const vec2 pixelJitter = vec2( jitterAmount() - 0.5f, jitterAmount() - 0.5f ) * 0.001f;
		const vec2 hexJitter = UniformSampleHexagon( vec2( jitterAmount(), jitterAmount() ) ) * ChorizoConfig.apertureAdjust;
		const vec3 localEyePos = ChorizoConfig.enableThinLens ? ChorizoConfig.eyePosition +
			( hexJitter.x + pixelJitter.x ) * ChorizoConfig.basisX +
			( hexJitter.y + pixelJitter.y ) * ChorizoConfig.basisY : ChorizoConfig.eyePosition;
		const float aspectRatio = float( config.width ) / float( config.height );

		ChorizoConfig.projTransform = glm::perspective( glm::radians( ChorizoConfig.FoV ), aspectRatio, ChorizoConfig.nearZ, ChorizoConfig.farZ );
		ChorizoConfig.projTransformInverse = glm::inverse( ChorizoConfig.projTransform );

		ChorizoConfig.viewTransform = glm::lookAt( localEyePos, ChorizoConfig.eyePosition + ChorizoConfig.basisZ * ChorizoConfig.focusDistance, ChorizoConfig.basisY );
		ChorizoConfig.viewTransformInverse = glm::inverse( ChorizoConfig.viewTransform );

		ChorizoConfig.combinedTransform = ChorizoConfig.projTransform * ChorizoConfig.viewTransform;
		ChorizoConfig.combinedTransformInverse = glm::inverse( ChorizoConfig.combinedTransform );
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		const bool * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		const SDL_Keymod k	= SDL_GetModState();
		const bool shift		= ( k & SDL_KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		const bool control		= ( k & SDL_KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		// quaternion based rotation via retained state in the basis vectors
		const float scalar = shift ? 0.1f : ( control ? 0.0005f : 0.02f );
		if ( state[ SDL_SCANCODE_W ] ) {
			glm::quat rot = glm::angleAxis( scalar, ChorizoConfig.basisX ); // basisX is the axis, therefore remains untransformed
			ChorizoConfig.basisY = ( rot * vec4( ChorizoConfig.basisY, 0.0f ) ).xyz();
			ChorizoConfig.basisZ = ( rot * vec4( ChorizoConfig.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_S ] ) {
			glm::quat rot = glm::angleAxis( -scalar, ChorizoConfig.basisX );
			ChorizoConfig.basisY = ( rot * vec4( ChorizoConfig.basisY, 0.0f ) ).xyz();
			ChorizoConfig.basisZ = ( rot * vec4( ChorizoConfig.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_A ] ) {
			glm::quat rot = glm::angleAxis( scalar, ChorizoConfig.basisY ); // same as above, but basisY is the axis
			ChorizoConfig.basisX = ( rot * vec4( ChorizoConfig.basisX, 0.0f ) ).xyz();
			ChorizoConfig.basisZ = ( rot * vec4( ChorizoConfig.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_D ] ) {
			glm::quat rot = glm::angleAxis( -scalar, ChorizoConfig.basisY );
			ChorizoConfig.basisX = ( rot * vec4( ChorizoConfig.basisX, 0.0f ) ).xyz();
			ChorizoConfig.basisZ = ( rot * vec4( ChorizoConfig.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_Q ] ) {
			glm::quat rot = glm::angleAxis( scalar, ChorizoConfig.basisZ ); // and again for basisZ
			ChorizoConfig.basisX = ( rot * vec4( ChorizoConfig.basisX, 0.0f ) ).xyz();
			ChorizoConfig.basisY = ( rot * vec4( ChorizoConfig.basisY, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_E ] ) {
			glm::quat rot = glm::angleAxis( -scalar, ChorizoConfig.basisZ );
			ChorizoConfig.basisX = ( rot * vec4( ChorizoConfig.basisX, 0.0f ) ).xyz();
			ChorizoConfig.basisY = ( rot * vec4( ChorizoConfig.basisY, 0.0f ) ).xyz();
		}

		// f to reset basis, shift + f to reset basis and home to origin
		if ( state[ SDL_SCANCODE_F ] ) {
			if ( shift ) ChorizoConfig.eyePosition = vec3( 0.0f, 0.0f, 0.0f );
			ChorizoConfig.basisX = vec3( 1.0f, 0.0f, 0.0f );
			ChorizoConfig.basisY = vec3( 0.0f, 1.0f, 0.0f );
			ChorizoConfig.basisZ = vec3( 0.0f, 0.0f, 1.0f );
		}
		if ( state[ SDL_SCANCODE_UP ] )			ChorizoConfig.eyePosition += scalar * ChorizoConfig.basisZ;
		if ( state[ SDL_SCANCODE_DOWN ] )		ChorizoConfig.eyePosition -= scalar * ChorizoConfig.basisZ;
		if ( state[ SDL_SCANCODE_RIGHT ] )		ChorizoConfig.eyePosition -= scalar * ChorizoConfig.basisX;
		if ( state[ SDL_SCANCODE_LEFT ] )		ChorizoConfig.eyePosition += scalar * ChorizoConfig.basisX;
		if ( state[ SDL_SCANCODE_PAGEDOWN ] )	ChorizoConfig.eyePosition += scalar * ChorizoConfig.basisY;
		if ( state[ SDL_SCANCODE_PAGEUP ] )		ChorizoConfig.eyePosition -= scalar * ChorizoConfig.basisY;

	}

	void ImGuiDrawPalette ( int &palette, string sublabel, float min = 0.0f, float max = 1.0f ) {
		static std::vector< const char * > paletteLabels;
		if ( paletteLabels.size() == 0 ) {
			for ( auto& entry : palette::paletteListLocal ) {
				// copy to a cstr for use by imgui
				char * d = new char[ entry.label.length() + 1 ];
				std::copy( entry.label.begin(), entry.label.end(), d );
				d[ entry.label.length() ] = '\0';
				paletteLabels.push_back( d );
			}
		}

		ImGui::Combo( ( string( "Palette##" ) + sublabel ).c_str(), &palette, paletteLabels.data(), paletteLabels.size() );
		const size_t paletteSize = palette::paletteListLocal[ palette ].colors.size();
		ImGui::Text( "  Contains %.3lu colors:", palette::paletteListLocal[ palette::PaletteIndex ].colors.size() );
		// handle max < min
		float minVal = min;
		float maxVal = max;
		float realSelectedMin = std::min( minVal, maxVal );
		float realSelectedMax = std::max( minVal, maxVal );
		size_t minShownIdx = std::floor( realSelectedMin * ( paletteSize - 1 ) );
		size_t maxShownIdx = std::ceil( realSelectedMax * ( paletteSize - 1 ) );

		bool finished = false;
		for ( int y = 0; y < 8; y++ ) {
			if ( !finished ) {
				ImGui::Text( " " );
			}

			for ( int x = 0; x < 32; x++ ) {

				// terminate when you run out of colors
				const uint32_t index = x + 32 * y;
				if ( index >= paletteSize ) {
					finished = true;
					// goto terminate;
				}

				// show color, or black if past the end of the list
				ivec4 color = ivec4( 0 );
				if ( !finished ) {
					color = ivec4( palette::paletteListLocal[ palette ].colors[ index ], 255 );
					// determine if it is in the active range
					if ( index < minShownIdx || index > maxShownIdx ) {
						color.a = 64; // dim inactive entries
					}
				} 
				if ( color.a != 0 ) {
					ImGui::SameLine();
					ImGui::TextColored( ImVec4( color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f ), "@" );
				}
			}
		}
		// terminate:
	}

	void ImguiPass () {
		ZoneScoped;

		// if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
		// if ( tonemap.showTonemapWindow ) TonemapControlsWindow();
		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		TonemapControlsWindow();

		ImGui::Begin( "Chorizo", NULL, ImGuiWindowFlags_NoScrollWithMouse );

		ImGui::SeparatorText( "Current Totals" );
		ImGui::Indent();
		ImGui::Text( "Bounding Box Impostors: %d", ChorizoConfig.geometryManager.count );
		ImGui::Text( "Point Sprite Impostors: %d", ChorizoConfig.geometryManager.countPointSprite );
		ImGui::Unindent();

		ImGui::SeparatorText( "Controls" );
		ImGui::SliderFloat( "FoV", &ChorizoConfig.FoV, 3.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::Checkbox( "Enable DoF Jitter", &ChorizoConfig.enableThinLens );
		ImGui::SliderFloat( "Blend Amount", &ChorizoConfig.blendAmount, 0.0001f, 0.5f, "%.7f", ImGuiSliderFlags_Logarithmic );
		ImGui::Checkbox( "Progressive Blend", &ChorizoConfig.progressiveBlend );
		if ( ChorizoConfig.progressiveBlend == true ) {
			ChorizoConfig.progressiveFrameCount++;
			ImGui::SameLine();
			if ( ImGui::Button( "Reset" ) ) {
				ChorizoConfig.progressiveFrameCount = 0;
			}
		}
		ImGui::SliderFloat( "Focus Adjust", &ChorizoConfig.focusDistance, 0.01f, 50.0f );
		ImGui::SliderFloat( "Aperture Adjust", &ChorizoConfig.apertureAdjust, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderFloat( "Volumetric Strength", &ChorizoConfig.volumetricStrength, 0.0f, 0.1f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::ColorEdit3( "Volumetric Color", ( float * ) &ChorizoConfig.volumetricColor, ImGuiColorEditFlags_PickerHueWheel );

		ImGui::Text( " " );

		ImGui::SeparatorText( "Generator" );

		if ( ImGui::CollapsingHeader( "Colors" ) ) {
			static uint32_t colorCap = 256;
			ImGui::SliderInt( "Random Select Color Count Cap", ( int* ) &colorCap, 2, 256 );
			ImGui::Text( " " );
			ImGui::Text( " " );

			ImGui::SeparatorText( "Geo" );
			ImGui::Text( " " );
			ImGui::Indent();
			ImGuiDrawPalette( ChorizoConfig.paletteID, "trees", ChorizoConfig.paletteMin, ChorizoConfig.paletteMax );
			if ( ImGui::Button( "Random##geo" ) ) {
				rngi pick = rngi( 0, palette::paletteListLocal.size() - 1 );
				do {
					ChorizoConfig.paletteID = pick();
				} while ( palette::paletteListLocal[ ChorizoConfig.paletteID ].colors.size() > colorCap );
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Reverse##geo" ) ) {
				std::swap( ChorizoConfig.paletteMin, ChorizoConfig.paletteMax );
			}
			ImGui::SliderFloat( "Palette Min##geo", &ChorizoConfig.paletteMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Palette Max##geo", &ChorizoConfig.paletteMax, 0.0f, 1.0f );
			ImGui::Unindent();

			ImGui::SeparatorText( "Lights" );
			ImGui::Text( " " );
			ImGui::Indent();
			ImGuiDrawPalette( ChorizoConfig.lightPaletteID, "lights", ChorizoConfig.lightPaletteMin, ChorizoConfig.lightPaletteMax );
			if ( ImGui::Button( "Random##lights" ) ) {
				rngi pick = rngi( 0, palette::paletteListLocal.size() - 1 );
				do {
					ChorizoConfig.lightPaletteID = pick();
				} while ( palette::paletteListLocal[ ChorizoConfig.lightPaletteID ].colors.size() > colorCap );
			}
			ImGui::SameLine();
			if ( ImGui::Button( "Reverse##lights" ) ) {
				std::swap( ChorizoConfig.lightPaletteMin, ChorizoConfig.lightPaletteMax );
			}
			ImGui::SliderFloat( "Palette Min##lights", &ChorizoConfig.lightPaletteMin, 0.0f, 1.0f );
			ImGui::SliderFloat( "Palette Max##lights", &ChorizoConfig.lightPaletteMax, 0.0f, 1.0f );
			ImGui::Unindent();
		}

		ImGui::End();

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );

		{	ZoneScoped; scopedTimer Start( "Bounding Box Compute" );
			// prepare the bounding boxes
			GLuint shader = shaders[ "Bounds" ];
			glUseProgram( shader );
			const int workgroupsRoundedUp = ( ChorizoConfig.numPrimitives + 63 ) / 64;
			glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{	ZoneScoped; scopedTimer Start( "Bounding Box Impostors" );
			// then draw, using the prepared data
			RasterGeoDataSetup();
			glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ ( ChorizoConfig.frameCount++ % 2 ) ] );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glViewport( 0, 0, config.width, config.height );
			glDrawArrays( GL_TRIANGLES, 0, 36 * ChorizoConfig.numPrimitives );
		}

		// draw the point sprites, as well
		{	ZoneScoped; scopedTimer Start( "Point Sprite Impostors" );
			RasterGeoDataSetupPointSprite();
			glDrawArrays( GL_POINTS, 0, ChorizoConfig.numPointSprites );
		}

		// revert to default framebuffer
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	void RasterGeoDataSetup () {
		const GLuint shader = shaders[ "Bounding Box" ];
		glUseProgram( shader );
		glBindVertexArray( ChorizoConfig.vao );

		glUniform1f( glGetUniformLocation( shader, "numPrimitives" ), ChorizoConfig.numPrimitives );
		glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransform ) );
		glUniform3fv( glGetUniformLocation( shader, "eyePosition" ), 1, glm::value_ptr( ChorizoConfig.eyePosition ) );
	}

	void RasterGeoDataSetupPointSprite () {
		const GLuint shader = shaders[ "Point Sprite" ];
		glUseProgram( shader );

		glUniform2f( glGetUniformLocation( shader, "viewportSize" ), config.width, config.height );
		glUniform1f( glGetUniformLocation( shader, "numPrimitives" ), ChorizoConfig.numPointSprites );
		glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransform ) );
		glUniformMatrix4fv( glGetUniformLocation( shader, "invViewTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.viewTransformInverse ) );
		glUniformMatrix4fv( glGetUniformLocation( shader, "projTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.projTransform ) );
		glUniform3fv( glGetUniformLocation( shader, "eyePosition" ), 1, glm::value_ptr( ChorizoConfig.eyePosition ) );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // doing the deferred processing from rasterizer results to rendered result
			scopedTimer Start( "Drawing" );
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			string index = ChorizoConfig.frameCount % 2 ? "0" : "1";
			string indexBack = ChorizoConfig.frameCount % 2 ? "1" : "0";
			textureManager.BindImageForShader( "Blue Noise", "blueNoiseTexture", shader, 0 );
			textureManager.BindImageForShader( "Accumulator", "accumulatorTexture", shader, 1 );
			textureManager.BindTexForShader( "Framebuffer Depth " + index, "depthTex", shader, 2 );
			textureManager.BindTexForShader( "Framebuffer Depth " + indexBack, "depthTexBack", shader, 3 );
			textureManager.BindTexForShader( "Framebuffer Primitive ID " + index, "primitiveID", shader, 4 );
			textureManager.BindTexForShader( "Framebuffer Primitive ID " + indexBack, "primitiveIDBack", shader, 5 );

			glUniformMatrix4fv( glGetUniformLocation( shader, "combinedTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransform ) );
			glUniformMatrix4fv( glGetUniformLocation( shader, "combinedTransformInverse" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransformInverse ) );

			glUniform1i( glGetUniformLocation( shader, "inSeed" ), ChorizoConfig.wangSeeder() );
			glUniform1f( glGetUniformLocation( shader, "nearZ" ), ChorizoConfig.nearZ );
			glUniform1f( glGetUniformLocation( shader, "farZ" ), ChorizoConfig.farZ );

			if ( ChorizoConfig.progressiveBlend == true ) {
				glUniform1f( glGetUniformLocation( shader, "blendAmount" ), 1.0f / ( ChorizoConfig.progressiveFrameCount + 1.0f ) );
			} else {
				glUniform1f( glGetUniformLocation( shader, "blendAmount" ), ChorizoConfig.blendAmount );
			}

			glUniform1f( glGetUniformLocation( shader, "volumetricStrength" ), ChorizoConfig.volumetricStrength );
			glUniform3fv( glGetUniformLocation( shader, "volumetricColor" ), 1, glm::value_ptr( ChorizoConfig.volumetricColor ) );
			glUniform1i( glGetUniformLocation( shader, "numLights" ), ChorizoConfig.numLights );
			glUniform3fv( glGetUniformLocation( shader, "eyePosition" ), 1, glm::value_ptr( ChorizoConfig.eyePosition ) );
			glUniform3fv( glGetUniformLocation( shader, "ambientColor" ), 1, glm::value_ptr( ChorizoConfig.ambientColor ) );

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
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// const GLuint shader = shaders[ "Animate" ];
		// glUseProgram( shader );
		// glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 3000.0f );
		// const uint workgroupsRoundedUp = ( ChorizoConfig.numPrimitives + 63 ) / 64;
		// glDispatchCompute( 64, workgroupsRoundedUp / 64, 1 );
		// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}

	void OnRender () {
		ZoneScoped;
		PrepSceneParameters();
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		DrawAPIGeometry();			// draw the cubes on top, into the ping-ponging framebuffer
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
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};
