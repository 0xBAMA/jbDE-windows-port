#include "../../../engine/engine.h"

// current state of the animation
struct animation_t {
	// config flags
	bool animationRunning = false;
	bool saveFrames = false;
	bool resetAccumulatorsOnFrameComplete = false;

	// configured quality settings + state tracking
	uint32_t numSamples = 256;
	uint32_t numFrames = 720 * 2;
	uint32_t frameNumber = 0;

	// setup operations, then per-frame operations
	json animationData;
};

#define OUTPUT		0
#define ACCUMULATOR	1
#define NORMAL		2
#define NORMALR		3
#define DEPTH		4

// program parameters and state
struct sirenConfig_t {

	// performance settings / monitoring
	uint32_t performanceHistorySamples;
	std::deque< float > timeHistory;	// ms per frame
	uint32_t numFullscreenPasses = 0;
	int32_t sampleCountCap;				// -1 for unlimited
	bool showTimeStamp = false;

	// add time since last reset ( + report avg time per fullscreen pass )
	std::chrono::time_point< std::chrono::steady_clock > tLastReset = std::chrono::steady_clock::now();

	uint32_t outputMode = OUTPUT;

	// renderer state
	uint32_t tileSize;
	uint32_t targetWidth;
	uint32_t targetHeight;
	uint32_t tilesBetweenQueries;
	float tilesMSLimit;
	bool tileListNeedsUpdate = true;	// need to generate new tile list ( if e.g. tile size changes )
	bool rendererNeedsUpdate = true;	// eventually to allow for the preview modes to render once between orientation etc changes
	std::vector< ivec2 > tileOffsets;	// shuffled list of tiles
	uint32_t tileOffset = 0;			// offset into tile list - start at first element
	int subpixelJitterMethod;
	float exposure;
	float renderFoV;
	float uvScalar;
	int cameraType;

	// thin lens parameters
	bool thinLensEnable;
	float thinLensFocusDistance;
	float thinLensJitterRadius;
	int bokehMode = 0;

	// noise, rng
	ivec2 blueNoiseOffset;
	rngi wangSeeder = rngi( 0, 100000000 );	// initial value for the wang ( +x,y offset per invocation )

	// viewer position, orientation
	vec3 viewerPosition;
	vec3 basisX = vec3( 1.0f, 0.0f, 0.0f );
	vec3 basisY = vec3( 0.0f, 1.0f, 0.0f );
	vec3 basisZ = vec3( 0.0f, 0.0f, 1.0f );

	// enhanced sphere tracing paper - potential for some optimizations - over-relaxation, refraction, dynamic epsilon
		// https://erleuchtet.org/~cupe/permanent/enhanced_sphere_tracing.pdf

	// raymarch parameters
	uint32_t raymarchMaxSteps;
	uint32_t raymarchMaxBounces;
	float raymarchMaxDistance;
	float raymarchEpsilon;
	float raymarchUnderstep;
	// bool skipRaymarch = false;
	bool skipRaymarch = true;
	bool skipGreeble = false;

	vec4 backgroundColor = vec4( vec3( 0.01618f ), 1.0f );	// background color for the image view
	vec3 skylightColor = vec3( 0.0f );		// ray escape color

	// informs the location of the sun
	bool invertSky = false;
	float skyTime = 5.16;
	ivec2 skyDims = ivec2( 4096, 2048 );

	// questionable need:
		// dither parameters ( mode, colorspace, pattern )
			// dithering the pathtrace result, seems, questionable - tbd
		// depth fog parameters ( mode, scalar )
			// will probably add this in
			// additionally, with depth + normals stored, SSAO? might add something, but will have to evaluate

	// properties of the animation
	animation_t animation;

	// list of active spheres ( xyz position, radius, rgb color, material ID )
	GLuint sphereSSBO;
	std::vector< vec4 > sphereLocationsPlusColors;
	const uint32_t maxSpheres = 256;
};

class Siren final : public engineBase {
public:
	Siren () { Init(); OnInit(); PostInit(); }
	~Siren () { Quit(); }

	sirenConfig_t sirenConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			ReloadDefaultConfig();
			ReloadShaders();

			if ( glIsProgram( shaders[ "Pathtrace" ] ) == GL_FALSE || glIsProgram( shaders[ "Postprocess" ] ) == GL_FALSE )
				pQuit = true; // tired of struggling to quit through error spew - this is still broken

			// remove the 16-bit accumulator, because we're going to want a 32-bit version for this
			textureManager.Remove( "Accumulator" );

			// remove the default display texture, since we need one sized for the accumulators
			textureManager.Remove( "Display Texture" );

			// create the new accumulator(s)
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32F;
			opts.width			= sirenConfig.targetWidth;
			opts.height			= sirenConfig.targetHeight;
			opts.minFilter		= GL_LINEAR;
			opts.magFilter		= GL_LINEAR;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			textureManager.Add( "Depth/Normals Accumulator", opts );
			textureManager.Add( "Color Accumulator", opts );
			textureManager.Add( "Color Accumulator Scratch", opts );

			// and for the display texture, LDR
			opts.dataType		= GL_RGBA8;
			opts.magFilter		= GL_NEAREST;
			opts.minFilter		= GL_LINEAR_MIPMAP_LINEAR;
			textureManager.Add( "Display Texture", opts );

			// and the cache texture, for the 2:1 rectilinear mapping of the sky color
			opts.dataType		= GL_RGBA16F;
			opts.width			= sirenConfig.skyDims.x;
			opts.height			= sirenConfig.skyDims.y;
			opts.wrap			= GL_CLAMP_TO_EDGE;
			textureManager.Add( "Sky Cache", opts );

			// setup performance monitor
			sirenConfig.timeHistory.resize( sirenConfig.performanceHistorySamples );

			// initialize the animation
			// InitiailizeAnimation( "src/projects/PathTracing/Siren/dummyAnimation.json" );

			cout << "Sphere Data running" << endl;
			InitSphereData(); // initialize the list of spheres
			glGenBuffers( 1, &sirenConfig.sphereSSBO ); // create the corresponding SSBO and populate it
			SendSphereSSBO();
			cout << "done" << endl;

			// create the stack
			cout << "Prepping Glyph buffer" << endl;
			PrepGlyphBuffer();
			cout << "done" << endl;
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		const bool * state = SDL_GetKeyboardState( NULL );

		// modifier keys state
		const SDL_Keymod k		= SDL_GetModState();
		const bool shift		= ( k & SDL_KMOD_SHIFT );
		const bool control		= ( k & SDL_KMOD_CTRL );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		// https://wiki.libsdl.org/SDL2/SDL_GetMouseState
			// want to pick depth at mouse location, map the position and then take data from the depth buffer, on click
				// maybe the basis for some kind of autofocus feature for the thin lens? I really like this idea

		// https://wiki.libsdl.org/SDL2/SDL_GetRelativeMouseState
			// I believe this is related to clicking and dragging

		float mouseX, mouseY;
		uint32_t mouseState = SDL_GetMouseState( &mouseX, &mouseY );
		if ( mouseState != 0 ) {
			// cout << "Mouse at " << mouseX << " " << mouseY << " with bitmask " << std::bitset<32>( mouseState ) << endl;
			vec2 fractionalPosition = vec2( mouseX / float( config.width ), 1.0f - ( mouseY / float( config.height ) ) );
			ivec2 pickPosition = ivec2( fractionalPosition.x * sirenConfig.targetWidth, fractionalPosition.y * sirenConfig.targetHeight );

			if ( pickPosition.x >= 0 && pickPosition.y >= 0 && pickPosition.x < config.width && pickPosition.y < config.height ) {
				// get the value from the depth buffer - helps determine where to set the focal plane for the DoF
				float value[ 4 ];
				const GLuint texture = textureManager.Get( "Depth/Normals Accumulator" );
				glGetTextureSubImage( texture, 0, pickPosition.x, pickPosition.y, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, 16, &value );
				// cout << "Picked depth at " << config.width << " " << config.height << " is " << value[ 3 ] << endl;
				if ( shift == true ) {
					sirenConfig.thinLensFocusDistance = value[ 3 ];
				}
			}
		}

		if ( state[ SDL_SCANCODE_R ] && shift ) {
			ResetAccumulators();
		}

		if ( state[ SDL_SCANCODE_T ] && shift ) {
			glMemoryBarrier( GL_ALL_BARRIER_BITS );
			ScreenShots( false, false, true );
		}

		// reload shaders
		if ( state[ SDL_SCANCODE_Y ] ) {
			ReloadShaders();
		}

		if ( state[ SDL_SCANCODE_Z ] ) {
			PrepGlyphBuffer();
		}

		// quaternion based rotation via retained state in the basis vectors - much easier to use than euler angles or the trident
		const float scalar = shift ? 0.1f : ( control ? 0.0005f : 0.02f );
		if ( state[ SDL_SCANCODE_W ] ) {
			glm::quat rot = glm::angleAxis( -scalar, sirenConfig.basisX ); // basisX is the axis, therefore remains untransformed
			sirenConfig.basisY = ( rot * vec4( sirenConfig.basisY, 0.0f ) ).xyz();
			sirenConfig.basisZ = ( rot * vec4( sirenConfig.basisZ, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_S ] ) {
			glm::quat rot = glm::angleAxis( scalar, sirenConfig.basisX );
			sirenConfig.basisY = ( rot * vec4( sirenConfig.basisY, 0.0f ) ).xyz();
			sirenConfig.basisZ = ( rot * vec4( sirenConfig.basisZ, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_A ] ) {
			glm::quat rot = glm::angleAxis( -scalar, sirenConfig.basisY ); // same as above, but basisY is the axis
			sirenConfig.basisX = ( rot * vec4( sirenConfig.basisX, 0.0f ) ).xyz();
			sirenConfig.basisZ = ( rot * vec4( sirenConfig.basisZ, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_D ] ) {
			glm::quat rot = glm::angleAxis( scalar, sirenConfig.basisY );
			sirenConfig.basisX = ( rot * vec4( sirenConfig.basisX, 0.0f ) ).xyz();
			sirenConfig.basisZ = ( rot * vec4( sirenConfig.basisZ, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_Q ] ) {
			glm::quat rot = glm::angleAxis( -scalar, sirenConfig.basisZ ); // and again for basisZ
			sirenConfig.basisX = ( rot * vec4( sirenConfig.basisX, 0.0f ) ).xyz();
			sirenConfig.basisY = ( rot * vec4( sirenConfig.basisY, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_E ] ) {
			glm::quat rot = glm::angleAxis( scalar, sirenConfig.basisZ );
			sirenConfig.basisX = ( rot * vec4( sirenConfig.basisX, 0.0f ) ).xyz();
			sirenConfig.basisY = ( rot * vec4( sirenConfig.basisY, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}

		// f to reset basis, shift + f to reset basis and home to origin
		if ( state[ SDL_SCANCODE_F ] ) {
			if ( shift ) {
				sirenConfig.viewerPosition = vec3( 0.0f, 0.0f, 0.0f );
			}
			// reset to default basis
			sirenConfig.basisX = vec3( 1.0f, 0.0f, 0.0f );
			sirenConfig.basisY = vec3( 0.0f, 1.0f, 0.0f );
			sirenConfig.basisZ = vec3( 0.0f, 0.0f, 1.0f );
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_UP ] ) {
			sirenConfig.viewerPosition += scalar * sirenConfig.basisZ;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_DOWN ] ) {
			sirenConfig.viewerPosition -= scalar * sirenConfig.basisZ;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_RIGHT ] ) {
			sirenConfig.viewerPosition += scalar * sirenConfig.basisX;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_LEFT ] ) {
			sirenConfig.viewerPosition -= scalar * sirenConfig.basisX;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_PAGEUP ] ) {
			sirenConfig.viewerPosition += scalar * sirenConfig.basisY;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_PAGEDOWN ] ) {
			sirenConfig.viewerPosition -= scalar * sirenConfig.basisY;
			sirenConfig.rendererNeedsUpdate = true;
		}
	}

	void ImguiPass () {
		ZoneScoped;

		{
			static bool MouseHoveringOverImage = true;
			const int heightBottomSection = 400;

			glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Display Texture" ) );
			glGenerateMipmap( GL_TEXTURE_2D );

			// setting fullscreen window
			ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
			ImGui::SetNextWindowSize( ImGui::GetIO().DisplaySize );
			ImGui::Begin( "Viewer Window ( PROTOTYPE )", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ( MouseHoveringOverImage ? ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar : 0 ) );

			static float imageScalar = 1.0f;
			static ImVec2 offset = ImVec2( 0.0f, 0.0f );

		// for changing what is displayed in the display texture
			// basically, we're abusing just the tab bar here, to set state
			if ( ImGui::BeginTabBar( "Tab Bar Parent", ImGuiTabBarFlags_None ) ) {
				// e.g. this branch will set whatever state for element 1, e.g. color
				if ( ImGui::BeginTabItem( " Output Color " ) ) {
					sirenConfig.outputMode = OUTPUT;
					ImGui::EndTabItem();
				}
				if ( ImGui::BeginTabItem( " Accumulator " ) ) {
					sirenConfig.outputMode = ACCUMULATOR;
					ImGui::EndTabItem();
				}
				if ( ImGui::BeginTabItem( " Normal " ) ) {
					sirenConfig.outputMode = NORMAL;
					ImGui::EndTabItem();
				}
				if ( ImGui::BeginTabItem( " Normal (Remapped) " ) ) {
					sirenConfig.outputMode = NORMALR;
					ImGui::EndTabItem();
				}
				if ( ImGui::BeginTabItem( " Depth " ) ) {
					sirenConfig.outputMode = DEPTH;
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}

			// const ImVec2 widgetSize = ImVec2( ImGui::GetWindowSize().x - 20.0f, ImGui::GetWindowSize().y - heightBottomSection - 55.0f );
			const ImVec2 widgetSize = ImVec2( ImGui::GetContentRegionAvail().x - 8.0f, ImGui::GetContentRegionAvail().y - 20.0f );

			const ImVec2 minUV = ImVec2( ( 1.0f - imageScalar ) + offset.x, imageScalar + offset.y );
			const ImVec2 maxUV = ImVec2( imageScalar + offset.x, ( 1.0f - imageScalar ) + offset.y );

			ImTextureID texture = ( ImTextureID ) ( void* ) ( intptr_t ) textureManager.Get( "Display Texture" );
			// line 7717 in imgui_demo.cc has an example where a grid is drawn
			ImGui::ImageButton( " ", texture, widgetSize, minUV, maxUV, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ), ImVec4( 1.0f, 1.0f, 1.0f, 1.0f ) );

			// detect dragging
			MouseHoveringOverImage = false;
			if ( ImGui::IsItemActive() && ( SDL_GetMouseState( NULL, NULL ) & SDL_BUTTON_LMASK ) ) {
				MouseHoveringOverImage = true;

				ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
				ImGui::ResetMouseDragDelta();

				const float adj = ( float ) sirenConfig.targetWidth / ( float ) sirenConfig.targetHeight;
				const float base = 1.0f / sirenConfig.targetWidth;

				offset.x -= currentMouseDrag.x * base * ( 1.0f / adj ) * ( 1.0f / imageScalar );
				offset.y += currentMouseDrag.y * base * ( 1.0f / imageScalar );
			}

			if ( ImGui::IsItemHovered() ) {
				MouseHoveringOverImage = true;
				imageScalar -= ImGui::GetIO().MouseWheel * 0.08f;
			}

			// clamp to a minimum value
			imageScalar = std::max( imageScalar, 0.01f );

			// end of the display section
			// ImGui::SetCursorPosY( widgetSize.y - ( heightBottomSection - 20.0f ) );

			// background for the controls
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			draw_list->AddRectFilled( ImVec2( 12.0f, widgetSize.y - ( heightBottomSection - 10.0f ) ), ImVec2( widgetSize.x + 12.0f, widgetSize.y + 30.0f ), IM_COL32( 25, 18, 16, 200 ) );

			ImGui::SetCursorPosY( widgetSize.y - ( heightBottomSection - 20.0f ) );
			const float oneThirdSectionWidth = ( ImGui::GetContentRegionAvail().x - 60.0f ) / 3.0f;
			// ImGui::Separator();

			ImGui::SetCursorPosX( 30.0f );
			ImGui::BeginChild( "ChildLeftmost", ImVec2( oneThirdSectionWidth, heightBottomSection ), false, 0 );
			ImGui::SeparatorText( " Performance " );
			float ts = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::steady_clock::now() - sirenConfig.tLastReset ).count() / 1000.0f;
			ImGui::Text( "Fullscreen Passes: %d in %.3f seconds ( %.3f samples/sec )", sirenConfig.numFullscreenPasses, ts, ( float ) sirenConfig.numFullscreenPasses / ts );
			ImGui::SeparatorText( " History " );
			// timing history
			const std::vector< float > timeVector = { sirenConfig.timeHistory.begin(), sirenConfig.timeHistory.end() };
			const string timeLabel = string( "Average: " ) + std::to_string( std::reduce( timeVector.begin(), timeVector.end() ) / timeVector.size() ).substr( 0, 5 ) + string( "ms/frame" );
			ImGui::PlotLines( " ", timeVector.data(), sirenConfig.performanceHistorySamples, 0, timeLabel.c_str(), -5.0f, 180.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

			// report number of complete passes, average tiles per second, average effective rays per second

			ImGui::SeparatorText( " Controls " );
			if ( ImGui::Button( " Linear Color EXR " ) ) {
			// EXR screenshot for linear color
				ScreenShots( true, false, false, false );
			}

			ImGui::SameLine();
			if ( ImGui::Button( " Normal/Depth EXR " ) ) {
			// EXR screenshot for normals/depth
				ScreenShots( false, true, false, false );
			}

			ImGui::SameLine();
			if ( ImGui::Button( " Tonemapped LDR PNG " ) ) {
			// PNG screenshot for tonemapped result
				ScreenShots( false, false, true, false );
			}

			if ( ImGui::Button( " Reset Accumulators " ) ) {
				ResetAccumulators();
			}

			ImGui::SameLine();
			if ( ImGui::Button( " Reload Shaders " ) ) {
				ReloadShaders();
			}

			ImGui::SameLine();
			if ( ImGui::Button( " Reload Config " ) ) {
				ReloadDefaultConfig();
			}

			if ( ImGui::Button( " Regen Sphere List " ) ) {
				InitSphereData();
				SendSphereSSBO();
				ResetAccumulators();
			}
			ImGui::SameLine();
			if ( ImGui::Button( " Relax Sphere List " ) ) {
				SphereRelax();
				SendSphereSSBO();
				ResetAccumulators();
			}

			static int filterSelector = 2;
			ImGui::Text( "Filter Mode: " );
			ImGui::SameLine();
			ImGui::RadioButton( " Gaussian ", &filterSelector, 0 );
			ImGui::SameLine();
			ImGui::RadioButton( " Median ", &filterSelector, 1 );
			ImGui::SameLine();
			ImGui::RadioButton( " Combo ", &filterSelector, 2 );
			ImGui::SameLine();
			ImGui::RadioButton( " Kuwahara ", &filterSelector, 3 );
			ImGui::SameLine();
			ImGui::RadioButton( " Random ", &filterSelector, 4 );

			ImGui::Text( "Apply: " );
			ImGui::SameLine();
			if ( ImGui::Button( " 1x " ) ) {
				Filter( filterSelector );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 10x " ) ) {
				for ( int i = 0; i < 10; i++ ) {
					Filter( filterSelector );
				}
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 100x " ) ) {
				for ( int i = 0; i < 100; i++ ) {
					Filter( filterSelector );
				}
			}

			ImGui::Text( "Resolution" );
			static int x = sirenConfig.targetWidth;
			static int y = sirenConfig.targetHeight;
			ImGui::SliderInt( "Accumulator X", &x, 0, 5000 );
			ImGui::SliderInt( "Accumulator Y", &y, 0, 5000 );
			if ( ImGui::Button( "Resize" ) ) {
				ResizeAccumulators( x, y );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 360p " ) ) {
				x = 640;
				y = 360;
				ResizeAccumulators( 640, 360 );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 720p " ) ) {
				x = 1280;
				y = 720;
				ResizeAccumulators( 1280, 720 );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 1080p " ) ) {
				x = 1920;
				y = 1080;
				ResizeAccumulators( 1920, 1080 );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 4K " ) ) {
				x = 3840;
				y = 2160;
				ResizeAccumulators( 3840, 2160 );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " Print Max " ) ) {
				x = 4800;
				y = 2400;
				ResizeAccumulators( 4800, 2400 );
			}

			// probably refit this for optimial resolutions for e.g. 4"x6", 5"x7", 8"x10", etc
				// figured out that 1080p is too low for 8.5"x11" - approx 125dpi - you will see pixels, if you look closely
				// from Tim Lottes: optimal res is around 300dpi for high quality photo prints

			// tile size
			ImGui::Text( "Tile Size: %d", sirenConfig.tileSize );
			ImGui::SameLine();
			// update will require that we rebuild the tile offset list
			if ( ImGui::Button( " - " ) ) {
				sirenConfig.tileSize = std::clamp( sirenConfig.tileSize >> 1, 16u, 4096u );
				sirenConfig.tileListNeedsUpdate = true;
			}
			ImGui::SameLine();
			if ( ImGui::Button( " + " ) ) {
				sirenConfig.tileSize = std::clamp( sirenConfig.tileSize << 1, 16u, 4096u );
				sirenConfig.tileListNeedsUpdate = true;
			}

			ImGui::SliderInt( "Tiles Between Queries", ( int * ) &sirenConfig.tilesBetweenQueries, 0, 45, "%d" );
			ImGui::SliderFloat( "Frame Time Limit (ms)", &sirenConfig.tilesMSLimit, 1.0f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic );

			ImGui::EndChild();
			ImGui::SameLine();

			// controls
			ImGui::BeginChild( "ChildMiddle", ImVec2( oneThirdSectionWidth, heightBottomSection ), false, 0 );

		// rendering parameters
			ImGui::SeparatorText( " Rendering " );
			// view position - consider adding 3 component float input SliderFloat3
			ImGui::SliderFloat( "Viewer X", &sirenConfig.viewerPosition.x, -40.0f, 40.0f );
			ImGui::SliderFloat( "Viewer Y", &sirenConfig.viewerPosition.y, -40.0f, 40.0f );
			ImGui::SliderFloat( "Viewer Z", &sirenConfig.viewerPosition.z, -40.0f, 40.0f );
			ImGui::Text( "Basis Vectors:" );
			ImGui::Text( " X: %.3f %.3f %.3f", sirenConfig.basisX.x, sirenConfig.basisX.y, sirenConfig.basisX.z );
			ImGui::Text( " Y: %.3f %.3f %.3f", sirenConfig.basisY.x, sirenConfig.basisY.y, sirenConfig.basisY.z );
			ImGui::Text( " Z: %.3f %.3f %.3f", sirenConfig.basisZ.x, sirenConfig.basisZ.y, sirenConfig.basisZ.z );

			ImGui::SeparatorText( " Camera " );
			ImGui::SliderFloat( "Render FoV", &sirenConfig.renderFoV, 0.01f, 3.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Screen UV Scalar", &sirenConfig.uvScalar, 0.01f, 5.0f );
			ImGui::SliderFloat( "Exposure", &sirenConfig.exposure, 0.0f, 5.0f );
			ImGui::ColorEdit3( "Sky Color", ( float * ) &sirenConfig.skylightColor, ImGuiColorEditFlags_PickerHueWheel );
			ImGui::SliderFloat( "Sky Time", &sirenConfig.skyTime, -10.0f, 10.0f );
			ImGui::SameLine();
			ImGui::Checkbox( "Invert Direction", &sirenConfig.invertSky );
			ImGui::ColorEdit3( "Background Color", ( float * ) &sirenConfig.backgroundColor, ImGuiColorEditFlags_PickerHueWheel );
			if ( ImGui::IsItemEdited() ) {
				// update the value of the border
				glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Display Texture" ) );
				glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &sirenConfig.backgroundColor[ 0 ] );
			}
			ImGui::Text( " " );

			const char * cameraNames[] = { "NORMAL", "SPHERICAL", "SPHERICAL2", "SPHEREBUG", "SIMPLEORTHO", "ORTHO", "COMPOUND" };
			ImGui::Combo( "Camera Type", &sirenConfig.cameraType, cameraNames, IM_ARRAYSIZE( cameraNames ) );

			const char * subpixelJitterModes[] = { "NONE", "BLUE", "UNIFORM", "WEYL", "WEYL INTEGER" };
			ImGui::Combo( "Subpixel Jitter", &sirenConfig.subpixelJitterMethod, subpixelJitterModes, IM_ARRAYSIZE( subpixelJitterModes ) );

			ImGui::Text( " " );
			ImGui::Checkbox( "Enable Thin Lens DoF", &sirenConfig.thinLensEnable );
			ImGui::SliderFloat( "Thin Lens Focus Distance", &sirenConfig.thinLensFocusDistance, 0.0f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Thin Lens Jitter Radius", &sirenConfig.thinLensJitterRadius, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic );

			const char * bokehModeNames[] = { "NONE", "EDGE BIASED DISK", "UNIFORM DISK", "REJECTION SAMPLED HEXAGON", "UNIFORM SAMPLED HEXAGON", "UNIFORM HEART", "THREE BLADE ROSETTE", "FIVE BLADE ROSETTE", "RING", "PENTAGON", "SEPTAGON", "OCTAGON", "NONAGON", "DECAGON", "11-GON", "5 STAR", "6 STAR", "7 STAR" };
			ImGui::Combo( "Bokeh Mode", &sirenConfig.bokehMode, bokehModeNames, IM_ARRAYSIZE( bokehModeNames ) );

		// raymarch parameters
			ImGui::Text( " " );
			ImGui::SeparatorText( " Raymarch Parameters " );
			ImGui::Checkbox( "Skip Raymarch Geometry", &sirenConfig.skipRaymarch );
			ImGui::Checkbox( "Skip Greeble Geometry", &sirenConfig.skipGreeble );
			ImGui::SliderInt( "Max Steps", ( int * ) &sirenConfig.raymarchMaxSteps, 10, 500 );
			ImGui::SliderInt( "Max Bounces", ( int * ) &sirenConfig.raymarchMaxBounces, 0, 50 );
			ImGui::SliderFloat( "Max Distance", &sirenConfig.raymarchMaxDistance, 1.0f, 500.0f );
			ImGui::SliderFloat( "Epsilon", &sirenConfig.raymarchEpsilon, 0.0001f, 0.5f, "%.5f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Understep", &sirenConfig.raymarchUnderstep, 0.1f, 1.0f );

			ImGui::EndChild();
			ImGui::SameLine();

			ImGui::BeginChild( "ChildRightmost", ImVec2( oneThirdSectionWidth, heightBottomSection ), false, 0 );

			PostProcessImguiMenu();

			ImGui::EndChild();
			ImGui::End();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm modal window, if triggered
	}

	void ColorScreenShotWithFilename ( const string filename ) {
		std::vector< float > imageBytesToSave;
		imageBytesToSave.resize( sirenConfig.targetWidth * sirenConfig.targetHeight * 4, 0 );
		glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Display Texture" ) );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
		Image_4F screenshot( sirenConfig.targetWidth, sirenConfig.targetHeight, &imageBytesToSave.data()[ 0 ] );
		screenshot.FlipVertical();
		if ( config.SRGBFramebuffer ) {
			screenshot.GammaCorrect( 2.2f );
		}
		screenshot.Save( filename );
	}

	void ScreenShots (
		const bool colorEXR = false,
		const bool normalEXR = false,
		const bool tonemappedResult = false,
		const bool tonemappedFullRes = false ) {
		ZoneScoped;

		// consider spawning a worker thread for this

		if ( colorEXR == true ) {
			std::vector< float > imageBytesToSave;
			imageBytesToSave.resize( sirenConfig.targetWidth * sirenConfig.targetHeight * sizeof( float ) * 4, 0 );
			glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Color Accumulator" ) );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
			Image_4F screenshot( sirenConfig.targetWidth, sirenConfig.targetHeight, &imageBytesToSave.data()[ 0 ] );
			const string filename = string( "ColorAccumulator-" ) + timeDateString() + string( ".exr" );
			screenshot.Save( filename, Image_4F::backend::TINYEXR );
		}

		if ( normalEXR == true ) {
			std::vector< float > imageBytesToSave;
			imageBytesToSave.resize( sirenConfig.targetWidth * sirenConfig.targetHeight * sizeof( float ) * 4, 0 );
			glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Depth/Normals Accumulator" ) );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
			Image_4F screenshot( sirenConfig.targetWidth, sirenConfig.targetHeight, &imageBytesToSave.data()[ 0 ] );
			const string filename = string( "NormalDepthAccumulator-" ) + timeDateString() + string( ".exr" );
			screenshot.Save( filename, Image_4F::backend::TINYEXR );
		}

		if ( tonemappedResult == true ) {
			const string filename = string( "Output-" ) + timeDateString() + string( ".png" );
			ColorScreenShotWithFilename( filename );
		}

		if ( tonemappedFullRes == true ) {
			// need to have another buffer, at the corresponding resolution - tonemap into this, rather than the display texture
				// another tile-based thing here, so that it will scale with the rest of the pipeline up to the driver limits on texture resolution
		}
	}

	GLuint64 SubmitTimerAndWait ( GLuint timer ) {
		ZoneScoped;

		glQueryCounter( timer, GL_TIMESTAMP );
		GLint available = 0;
		while ( !available )
			glGetQueryObjectiv( timer, GL_QUERY_RESULT_AVAILABLE, &available );
		GLuint64 t;
		glGetQueryObjectui64v( timer, GL_QUERY_RESULT, &t );
		return t;
	}

	void Filter ( int mode ) {
		GLuint shader = shaders[ "Filter" ];
		glUseProgram( shader );

		if ( mode == 4 ) { // randomize
			static rngi pick = rngi( 0, 3 );
			glUniform1i( glGetUniformLocation( shader, "mode" ), pick() );
		} else {
			glUniform1i( glGetUniformLocation( shader, "mode" ), mode );
		}

		textureManager.BindImageForShader( "Color Accumulator", "sourceData", shader, 0 );
		textureManager.BindImageForShader( "Color Accumulator Scratch", "destData", shader, 1 );
		glDispatchCompute( ( sirenConfig.targetWidth + 15 ) / 16, ( sirenConfig.targetHeight + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		shader = shaders[ "Copy Back" ];
		glUseProgram( shader );
		textureManager.BindImageForShader( "Color Accumulator Scratch", "sourceData", shader, 0 );
		textureManager.BindImageForShader( "Color Accumulator", "destData", shader, 1 );
		glDispatchCompute( ( sirenConfig.targetWidth + 15 ) / 16, ( sirenConfig.targetHeight + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}

	void ComputePasses () {
		ZoneScoped;

		{	// dispatch the shader to update the sky, if needed
			scopedTimer Start( "Sky Update" );
			static float skyTime_cache = 0.0f; // reenble latch once done debugging
			if ( skyTime_cache != sirenConfig.skyTime ) {
				skyTime_cache = sirenConfig.skyTime;
				const GLuint shader = shaders[ "Sky Cache" ];
				glUseProgram( shader );

				// uniforms
				glUniform1f( glGetUniformLocation( shader, "skyTime" ), sirenConfig.skyTime );
				textureManager.BindImageForShader( "Sky Cache", "skyCache", shader, 0 );

				// dispatch
				glDispatchCompute( ( sirenConfig.skyDims.x + 15 ) / 16, ( sirenConfig.skyDims.y + 15 ) / 16, 1 );
			}
		}

		{
			scopedTimer Start( "Tiled Update" );
			const GLuint shader = shaders[ "Pathtrace" ];
			glUseProgram( shader );

			AnimationUpdate();
			SendBasePathtraceUniforms();

			// create OpenGL timery query objects - more reliable than std::chrono, at least in theory
			GLuint t[ 2 ];
			GLuint64 t0, tCheck;
			glGenQueries( 2, t );

			// submit the first timer query, to determine t0, outside the loop
			t0 = SubmitTimerAndWait( t[ 0 ] );

			// for monitoring number of completed tiles
			uint32_t tilesThisFrame = 0;

			while ( 1 && !quitConfirm ) {
				// run some N tiles out of the list
				for ( uint32_t tile = 0; tile < sirenConfig.tilesBetweenQueries; tile++ ) {
					const ivec2 tileOffset = GetTile(); // send uniforms ( unique per loop iteration )
					glUniform2i( glGetUniformLocation( shader, "tileOffset" ),		tileOffset.x, tileOffset.y );
					glUniform1i( glGetUniformLocation( shader, "wangSeed" ),		sirenConfig.wangSeeder() );

					// going to basically say that tilesizes are multiples of 16, to match the group size
					glDispatchCompute( sirenConfig.tileSize / 16, sirenConfig.tileSize / 16, 1 );
					tilesThisFrame++;
				}

				// submit the second timer query, to determine tCheck, inside the loop
				tCheck = SubmitTimerAndWait( t[ 1 ] );

				// evaluate how long it we've taken in the infinite loop, and break if 16.6ms is exceeded
				float loopTime = ( tCheck - t0 ) / 1e6f; // convert ns -> ms
				if ( loopTime > sirenConfig.tilesMSLimit ) {
					// update performance monitors with latest data
					UpdatePerfMonitor( loopTime, tilesThisFrame );
					break;
				}
			}
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			// potentially add stuff like dithering, SSAO, depth fog, etc
			scopedTimer Start( "Postprocess" );
			const GLuint shader = shaders[ "Postprocess" ];
			glUseProgram( shader );

			textureManager.BindTexForShader( "Color Accumulator", "sourceC", shader, 0 );
			textureManager.BindTexForShader( "Depth/Normals Accumulator", "sourceDN", shader, 1 );
			textureManager.BindImageForShader( "Display Texture", "displayTexture", shader, 2 );

			// make sure we're sending the updated color temp every frame
			static float prevColorTemperature = 0.0f;
			static vec3 temperatureColor;
			if ( tonemap.colorTemp != prevColorTemperature ) {
				prevColorTemperature = tonemap.colorTemp;
				temperatureColor = GetColorForTemperature( tonemap.colorTemp );
			}

			// precompute the 3x3 matrix for the saturation adjustment
			static float prevSaturationValue = -1.0f;
			static mat3 saturationMatrix;
			if ( tonemap.saturation != prevSaturationValue ) {
				// https://www.graficaobscura.com/matrix/index.html
				const float s = tonemap.saturation;
				const float oms = 1.0f - s;

				vec3 weights = tonemap.saturationImprovedWeights ?
					vec3( 0.3086f, 0.6094f, 0.0820f ) :	// "improved" luminance vector
					vec3( 0.2990f, 0.5870f, 0.1140f );	// NTSC weights

				saturationMatrix = mat3(
					oms * weights.r + s,	oms * weights.r,		oms * weights.r,
					oms * weights.g,		oms * weights.g + s,	oms * weights.g,
					oms * weights.b,		oms * weights.b,		oms * weights.b + s
				);
			}

			glUniform3fv( glGetUniformLocation( shader, "colorTempAdjust" ), 1,			glm::value_ptr( temperatureColor ) );
			glUniform1i( glGetUniformLocation( shader, "tonemapMode" ),					tonemap.tonemapMode );
			glUniformMatrix3fv( glGetUniformLocation( shader, "saturation" ), 1, false,	glm::value_ptr( saturationMatrix ) );
			glUniform1f( glGetUniformLocation( shader, "gamma" ),						tonemap.gamma );
			glUniform1f( glGetUniformLocation( shader, "postExposure" ),				tonemap.postExposure );
			glUniform2f( glGetUniformLocation( shader, "resolution" ),					sirenConfig.targetWidth, sirenConfig.targetHeight );
			glUniform3fv( glGetUniformLocation( shader, "bgColor" ), 1,					glm::value_ptr( sirenConfig.backgroundColor ) );
			glUniform1i( glGetUniformLocation( shader, "activeMode" ),					sirenConfig.outputMode );

			glDispatchCompute( ( sirenConfig.targetWidth + 15 ) / 16, ( sirenConfig.targetHeight + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// { // text rendering timestamp - required texture binds are handled internally
		// 	if ( sirenConfig.showTimeStamp == true ) {
		// 		scopedTimer Start( "Text Rendering" );
		// 		textRenderer.Update( ImGui::GetIO().DeltaTime );
		// 		textRenderer.Draw( textureManager.Get( "Display Texture" ) );
		// 		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		// 	}
		// }
	}

	void UpdateNoiseOffset () {
		ZoneScoped;

		static rng offsetGenerator = rng( 0, 512 );
		sirenConfig.blueNoiseOffset = ivec2( offsetGenerator(), offsetGenerator() );
	}

	void UpdatePerfMonitor ( float loopTime, float tilesThisFrame ) {
		ZoneScoped;

		sirenConfig.timeHistory.push_back( loopTime );
		sirenConfig.timeHistory.pop_front();
	}

	void ResetAccumulators () {
		ZoneScoped;

		// clear the buffers
		Image_4U zeroes( sirenConfig.targetWidth, sirenConfig.targetHeight );
		GLuint handle = textureManager.Get( "Color Accumulator" );
		glBindTexture( GL_TEXTURE_2D, handle );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, zeroes.Width(), zeroes.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, ( void * ) zeroes.GetImageDataBasePtr() );
		handle = textureManager.Get( "Depth/Normals Accumulator" );
		glBindTexture( GL_TEXTURE_2D, handle );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, zeroes.Width(), zeroes.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, ( void * ) zeroes.GetImageDataBasePtr() );

		// reset number of samples + tile offset
		sirenConfig.numFullscreenPasses = 0;
		sirenConfig.tileOffset = 0;

		// reset time since last reset
		sirenConfig.tLastReset = std::chrono::steady_clock::now();
	}

	void ResizeAccumulators ( uint32_t x, uint32_t y ) {
		ZoneScoped;

		// destroy the existing textures
		textureManager.Remove( "Depth/Normals Accumulator" );
		textureManager.Remove( "Color Accumulator" );
		textureManager.Remove( "Color Accumulator Scratch" );
		textureManager.Remove( "Display Texture" );

		// create the new ones
		textureOptions_t opts;
		opts.dataType		= GL_RGBA32F;
		opts.width			= sirenConfig.targetWidth = x;
		opts.height			= sirenConfig.targetHeight = y;
		opts.minFilter		= GL_LINEAR;
		opts.magFilter		= GL_LINEAR;
		opts.textureType	= GL_TEXTURE_2D;
		opts.wrap			= GL_CLAMP_TO_BORDER;
		textureManager.Add( "Depth/Normals Accumulator", opts );
		textureManager.Add( "Color Accumulator Scratch", opts );
		textureManager.Add( "Color Accumulator", opts );

		opts.dataType		= GL_RGBA8;
		opts.magFilter		= GL_NEAREST;
		opts.minFilter		= GL_LINEAR_MIPMAP_LINEAR;
		textureManager.Add( "Display Texture", opts );

		// rebuild tile list next frame
		sirenConfig.tileListNeedsUpdate = true;
		sirenConfig.numFullscreenPasses = 0;
	}

	void ReloadShaders () {
		ZoneScoped;
		shaders[ "Sky Cache" ]		= computeShader( "../src/projects/PathTracing/Siren/shaders/skyCache.cs.glsl" ).shaderHandle;
		shaders[ "Pathtrace" ]		= computeShader( "../src/projects/PathTracing/Siren/shaders/pathtrace.cs.glsl" ).shaderHandle;
		shaders[ "Postprocess" ]	= computeShader( "../src/projects/PathTracing/Siren/shaders/postprocess.cs.glsl" ).shaderHandle;
		shaders[ "Filter" ]			= computeShader( "../src/projects/PathTracing/Siren/shaders/filter.cs.glsl" ).shaderHandle;
		shaders[ "Copy Back" ]		= computeShader( "../src/projects/PathTracing/Siren/shaders/copyBack.cs.glsl" ).shaderHandle;
	}

	void ReloadDefaultConfig () {
		ZoneScoped;
		json j; ifstream i ( "../src/engine/config.json" ); i >> j; i.close();
		sirenConfig.targetWidth					= j[ "app" ][ "Siren" ][ "targetWidth" ];
		sirenConfig.targetHeight				= j[ "app" ][ "Siren" ][ "targetHeight" ];
		sirenConfig.tileSize					= j[ "app" ][ "Siren" ][ "tileSize" ];
		sirenConfig.tilesBetweenQueries			= j[ "app" ][ "Siren" ][ "tilesBetweenQueries" ];
		sirenConfig.tilesMSLimit				= j[ "app" ][ "Siren" ][ "tilesMSLimit" ];
		sirenConfig.performanceHistorySamples	= j[ "app" ][ "Siren" ][ "performanceHistorySamples" ];
		sirenConfig.raymarchMaxSteps			= j[ "app" ][ "Siren" ][ "raymarchMaxSteps" ];
		sirenConfig.raymarchMaxBounces			= j[ "app" ][ "Siren" ][ "raymarchMaxBounces" ];
		sirenConfig.raymarchMaxDistance			= j[ "app" ][ "Siren" ][ "raymarchMaxDistance" ];
		sirenConfig.raymarchEpsilon				= j[ "app" ][ "Siren" ][ "raymarchEpsilon" ];
		sirenConfig.raymarchUnderstep			= j[ "app" ][ "Siren" ][ "raymarchUnderstep" ];
		sirenConfig.subpixelJitterMethod		= j[ "app" ][ "Siren" ][ "subpixelJitterMethod" ];
		sirenConfig.exposure					= j[ "app" ][ "Siren" ][ "exposure" ];
		sirenConfig.renderFoV					= j[ "app" ][ "Siren" ][ "renderFoV" ];
		sirenConfig.uvScalar					= j[ "app" ][ "Siren" ][ "uvScalar" ];
		sirenConfig.cameraType					= j[ "app" ][ "Siren" ][ "cameraType" ];
		sirenConfig.thinLensEnable				= j[ "app" ][ "Siren" ][ "thinLensEnable" ];
		sirenConfig.thinLensFocusDistance		= j[ "app" ][ "Siren" ][ "thinLensFocusDistance" ];
		sirenConfig.thinLensJitterRadius		= j[ "app" ][ "Siren" ][ "thinLensJitterRadius" ];
		sirenConfig.viewerPosition.x			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "x" ];
		sirenConfig.viewerPosition.y			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "y" ];
		sirenConfig.viewerPosition.z			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "z" ];
		sirenConfig.basisX.x					= j[ "app" ][ "Siren" ][ "basisX" ][ "x" ];
		sirenConfig.basisX.y					= j[ "app" ][ "Siren" ][ "basisX" ][ "y" ];
		sirenConfig.basisX.z					= j[ "app" ][ "Siren" ][ "basisX" ][ "z" ];
		sirenConfig.basisY.x					= j[ "app" ][ "Siren" ][ "basisY" ][ "x" ];
		sirenConfig.basisY.y					= j[ "app" ][ "Siren" ][ "basisY" ][ "y" ];
		sirenConfig.basisY.z					= j[ "app" ][ "Siren" ][ "basisY" ][ "z" ];
		sirenConfig.basisZ.x					= j[ "app" ][ "Siren" ][ "basisZ" ][ "x" ];
		sirenConfig.basisZ.y					= j[ "app" ][ "Siren" ][ "basisZ" ][ "y" ];
		sirenConfig.basisZ.z					= j[ "app" ][ "Siren" ][ "basisZ" ][ "z" ];
		sirenConfig.showTimeStamp				= j[ "app" ][ "Siren" ][ "showTimeStamp" ];

		// normalize the loaded basis vectors
		sirenConfig.basisX = glm::normalize( sirenConfig.basisX );
		sirenConfig.basisY = glm::normalize( sirenConfig.basisY );
		sirenConfig.basisZ = glm::normalize( sirenConfig.basisZ );
	}

	void LookAt ( const vec3 eye, const vec3 at, const vec3 up ) {
		ZoneScoped;
		sirenConfig.viewerPosition = eye;
		sirenConfig.basisZ = normalize( at - eye );
		sirenConfig.basisX = normalize( glm::cross( up, sirenConfig.basisZ ) );
		sirenConfig.basisY = normalize( glm::cross( sirenConfig.basisZ, sirenConfig.basisX ) );
	}

	ivec2 GetTile () {
		ZoneScoped;
		if ( sirenConfig.tileListNeedsUpdate == true ) {
			// construct the tile list ( runs at frame 0 and again any time the tilesize changes )
			sirenConfig.tileListNeedsUpdate = false;
			sirenConfig.tileOffset = 0;
			sirenConfig.tileOffsets.resize( 0 );
			for ( uint32_t x = 0; x <= sirenConfig.targetWidth; x += sirenConfig.tileSize ) {
				for ( uint32_t y = 0; y <= sirenConfig.targetHeight; y += sirenConfig.tileSize ) {
					sirenConfig.tileOffsets.push_back( ivec2( x, y ) );
				}
			}
			// shuffle the generated array
			std::random_device rd;
			std::mt19937 rngen( rd() );
			std::shuffle( sirenConfig.tileOffsets.begin(), sirenConfig.tileOffsets.end(), rngen );
		} else { // check if the offset needs to be reset, this means a full pass has been completed
			if ( ++sirenConfig.tileOffset == sirenConfig.tileOffsets.size() ) {
				sirenConfig.tileOffset = 0;
				sirenConfig.numFullscreenPasses++;
				UpdateNoiseOffset();
			}
		}
		return sirenConfig.tileOffsets[ sirenConfig.tileOffset ];
	}

	void ProcessAnimationJson ( json j ) {
		ZoneScoped;

		// use this for parsing setup ops + per-frame ops
		for ( auto& element : j.items() ) {
			string label = element.key();

			// this is where any settings will change, based on the label of the operation
			// cout << "got an operation: " << label << " with value " << element.value() << endl;

		// operations that can be specified
			if ( label == "numSamples" ) { // number of samples, dynamic per frame if desired
				sirenConfig.animation.numSamples = element.value();
			} else if ( label == "viewerPosition" ) { // viewer position
				sirenConfig.viewerPosition.x = element.value()[ "x" ];
				sirenConfig.viewerPosition.y = element.value()[ "y" ];
				sirenConfig.viewerPosition.z = element.value()[ "z" ];
			} else if ( label == "basisX" ) { // basis vectors
				sirenConfig.basisX.x = element.value()[ "x" ];
				sirenConfig.basisX.y = element.value()[ "y" ];
				sirenConfig.basisX.z = element.value()[ "z" ];
				sirenConfig.basisX = glm::normalize( sirenConfig.basisX );
			} else if ( label == "basisY" ) {
				sirenConfig.basisY.x = element.value()[ "x" ];
				sirenConfig.basisY.y = element.value()[ "y" ];
				sirenConfig.basisY.z = element.value()[ "z" ];
				sirenConfig.basisY = glm::normalize( sirenConfig.basisY );
			} else if ( label == "basisZ" ) {
				sirenConfig.basisZ.x = element.value()[ "x" ];
				sirenConfig.basisZ.y = element.value()[ "y" ];
				sirenConfig.basisZ.z = element.value()[ "z" ];
				sirenConfig.basisZ = glm::normalize( sirenConfig.basisZ );
			} else if ( label == "LookAt" ) { // also support basis, position via LookAt()
				vec3 eye = vec3( element.value()[ "eye" ][ "x" ], element.value()[ "eye" ][ "y" ], element.value()[ "eye" ][ "x" ] );
				vec3 at = vec3( element.value()[ "at" ][ "x" ], element.value()[ "at" ][ "y" ], element.value()[ "at" ][ "x" ] );
				vec3 up = vec3( element.value()[ "up" ][ "x" ], element.value()[ "up" ][ "y" ], element.value()[ "up" ][ "x" ] );
				LookAt( eye, at, up );
			} else if ( label == "clear" ) {
				// when this works, will need to remove the default clear in the loop

				// clear the buffer... partial clears, as well, via setting sample count lower than
					// the existing values ( color accumulator alpha )- tbd semantics on this one, to support partial clears

				// eventually maybe get into some kind of reprojection of the data in the buffer, tbd

			} else if ( label == "exposure" ) { // exposure adjustment
				sirenConfig.exposure = element.value();
			} else if ( label == "raymarchMaxSteps" ) {
				sirenConfig.raymarchMaxSteps = element.value();
			} else if ( label == "raymarchMaxBounces" ) {
				sirenConfig.raymarchMaxBounces = element.value();
			} else if ( label == "raymarchMaxDistance" ) {
				sirenConfig.raymarchMaxDistance = element.value();
			} else if ( label == "raymarchEpsilon" ) {
				sirenConfig.raymarchEpsilon = element.value();
			} else if ( label == "raymarchUnderstep" ) {
				sirenConfig.raymarchUnderstep = element.value();
			} else if ( label == "tonemapMode" ) {
				tonemap.tonemapMode = element.value();
			} else if ( label == "gamma" ) {
				tonemap.gamma = element.value();
			} else if ( label == "saturation" ) {
				tonemap.saturation = element.value();
			} else if ( label == "colorTemperature" ) {
				tonemap.colorTemp = element.value();
			} else if ( label == "thinLensEnable" ) { // thin lens enable
				sirenConfig.thinLensEnable = element.value();
			} else if ( label == "thinLensFocusDistance" ) { // thin lens focus distance
				sirenConfig.thinLensFocusDistance = element.value();
			} else if ( label == "thinLensJitterRadius" ) { // thin lens focus effect intensity
				sirenConfig.thinLensJitterRadius = element.value();
			} else if ( label == "autofocus" ) {
				// autofocus - take one sample, then set thin lens parameters... implementation tbd
					// probably want to specify the x,y picking location

			} else if ( label == "renderFoV" ) { // render field of view
				sirenConfig.renderFoV = element.value();
			} else if ( label == "uvScalar" ) { // uv scalar
				sirenConfig.uvScalar = element.value();
			} else if ( label == "cameraType" ) { // camera type
				sirenConfig.cameraType = element.value();
			} else if ( label == "skylightColor" ) { // skylight color - tbd if this is going to extend to handle skybox
				sirenConfig.skylightColor.r = element.value()[ "r" ];
				sirenConfig.skylightColor.g = element.value()[ "g" ];
				sirenConfig.skylightColor.b = element.value()[ "b" ];
			}
		}
	}

	void InitiailizeAnimation ( string filename ) {
		ZoneScoped;

		// load the json from the specified file
		json j; ifstream i ( filename ); i >> j; i.close();
		sirenConfig.animation.animationData = j;

		// initial setup
		sirenConfig.animation.numFrames		= j[ "setup" ][ "numFrames" ];
		sirenConfig.animation.numSamples	= j[ "setup" ][ "numSamples" ];

		// resolution, perf scalars are configured through the default config or on the menus

		// do any desired setup operations
		ProcessAnimationJson( sirenConfig.animation.animationData[ "setup" ] );
	}

	void AnimationUpdate () {
		ZoneScoped;
		if ( sirenConfig.animation.animationRunning ) {

			if ( sirenConfig.numFullscreenPasses > sirenConfig.animation.numSamples ) {
				// save out this frame's image + reset the accumulators if configured to do so
					// disabling one or both of these flags and setting to 2-5 samples gives an easy preview mode
				if ( sirenConfig.animation.saveFrames ) {
					// may need to extend a bit, if I want to do the one-or-more-CPU-side-postprocess-ops thing
					ColorScreenShotWithFilename( string( "frames/" ) + fixedWidthNumberString( sirenConfig.animation.frameNumber ) + string( ".png" ) );
				}

				if ( sirenConfig.animation.resetAccumulatorsOnFrameComplete ) {
					ResetAccumulators();
				}

				// update any desired operations for this frame - run only once, at frame init
				ProcessAnimationJson( sirenConfig.animation.animationData[ to_string( sirenConfig.animation.frameNumber ) ] );

				// increment frame number - 0..numFrames-1, so this will hit numFrames for the check
				sirenConfig.animation.frameNumber++;
				if ( sirenConfig.animation.frameNumber == sirenConfig.animation.numFrames ) {
					cout << "finished at " << timeDateString() << " after " << TotalTime() / 1000.0f << " seconds" << endl;
					abort(); // maybe do this in a nicer way
				}

			}
		}
	}

	void SendBasePathtraceUniforms () {
		// send uniforms ( initial, shared across all tiles dispatched this frame )
		const GLuint shader = shaders[ "Pathtrace" ];
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ),				sirenConfig.blueNoiseOffset.x, sirenConfig.blueNoiseOffset.y );
		glUniform1i( glGetUniformLocation( shader, "raymarchMaxSteps" ),		sirenConfig.raymarchMaxSteps );
		glUniform1i( glGetUniformLocation( shader, "raymarchMaxBounces" ),		sirenConfig.raymarchMaxBounces );
		glUniform1f( glGetUniformLocation( shader, "raymarchMaxDistance" ),		sirenConfig.raymarchMaxDistance );
		glUniform1f( glGetUniformLocation( shader, "raymarchEpsilon" ),			sirenConfig.raymarchEpsilon );
		glUniform1f( glGetUniformLocation( shader, "raymarchUnderstep" ),		sirenConfig.raymarchUnderstep );
		glUniform1f( glGetUniformLocation( shader, "exposure" ),				sirenConfig.exposure );
		glUniform1f( glGetUniformLocation( shader, "FoV" ),						sirenConfig.renderFoV );
		glUniform1f( glGetUniformLocation( shader, "uvScalar" ),				sirenConfig.uvScalar );
		glUniform1i( glGetUniformLocation( shader, "cameraType" ),				sirenConfig.cameraType );
		glUniform1i( glGetUniformLocation( shader, "subpixelJitterMethod" ),	sirenConfig.subpixelJitterMethod );
		glUniform1i( glGetUniformLocation( shader, "sampleNumber" ),			sirenConfig.numFullscreenPasses );
		glUniform1i( glGetUniformLocation( shader, "frameNumber" ),				sirenConfig.animation.frameNumber );
		glUniform1i( glGetUniformLocation( shader, "thinLensEnable" ),			sirenConfig.thinLensEnable );
		glUniform1f( glGetUniformLocation( shader, "thinLensFocusDistance" ),	sirenConfig.thinLensFocusDistance );
		glUniform1f( glGetUniformLocation( shader, "thinLensJitterRadius" ),	sirenConfig.thinLensJitterRadius );
		glUniform1i( glGetUniformLocation( shader, "bokehMode" ),				sirenConfig.bokehMode );
		glUniform1i( glGetUniformLocation( shader, "invertSky" ),				sirenConfig.invertSky ); // add to config, animation control
		glUniform1i( glGetUniformLocation( shader, "skipRaymarch" ),			sirenConfig.skipRaymarch ); // add to config, animation control
		glUniform1i( glGetUniformLocation( shader, "skipGreeble" ),				sirenConfig.skipGreeble ); // add to config, animation control
		glUniform3fv( glGetUniformLocation( shader, "skylightColor" ), 1,		glm::value_ptr( sirenConfig.skylightColor ) );
		glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1,				glm::value_ptr( sirenConfig.basisX ) );
		glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1,				glm::value_ptr( sirenConfig.basisY ) );
		glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1,				glm::value_ptr( sirenConfig.basisZ ) );
		glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1,		glm::value_ptr( sirenConfig.viewerPosition ) );
		// color, depth, normal targets, blue noise source data
		textureManager.BindImageForShader( "Color Accumulator", "accumulatorColor", shader, 0 );
		textureManager.BindImageForShader( "Depth/Normals Accumulator", "accumulatorNormalsAndDepth", shader, 1 );
		textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 2 );
		textureManager.BindImageForShader( "TextBuffer", "textBuffer", shader, 3 );
		textureManager.BindTexForShader( "Sky Cache", "skyCache", shader, 4 );
	}

	void InitSphereData () {
		// start timing
		Tick();

		// clear it out
		sirenConfig.sphereLocationsPlusColors.clear();

		// pick new palette
		palette::PickRandomPalette( true );

		// // first implementation, randomizing all parameters
		rng c = rng( 0.3f, 1.0f );
		rng o = rng( -1.0f, 1.0f );
		// rng y = rng( 0.0f, 15.0f );
		rng r = rng( 0.1f, 0.4f );
		// rngi p = rngi( 0, 1 );
		// rng p = rng( 0.0f, 1.0f );
		// rngN iorGen = rngN( 1.0f / 1.5f, 0.1f );
		for ( uint32_t x = 0; x < sirenConfig.maxSpheres; x++ ) {
			sirenConfig.sphereLocationsPlusColors.push_back( vec4( o(), o(), o(), r() ) );	// position
			// color
			// sirenConfig.sphereLocationsPlusColors.push_back( vec4( c(), c() * 0.5f, c() * 0.2f, p() ) );
			// sirenConfig.sphereLocationsPlusColors.push_back( vec4( c(), c() * 0.5f, c() * 0.2f, ( p() < 0.3f ) ? 7 : ( p() < 0.9f ) ? 9 : 1 ) );
			// sirenConfig.sphereLocationsPlusColors.push_back( vec4( c(), c() * 0.5f, c() * 0.2f, ( p() < 0.5f ) ? 6 : 11 ) );
			// sirenConfig.sphereLocationsPlusColors.push_back( vec4( c(), c() * 0.5f, 1.0f / 1.3f, 12.0f ) );
			sirenConfig.sphereLocationsPlusColors.push_back( vec4( c(), c() * 0.5f, c(), 7.0f ) );
		}


		// // jittered grid, noise informing material properties
		// PerlinNoise p;
		// rng r = rng( 0.2f, 2.9f );
		// for ( int x = 0; x < 16; x++ ) {
		// 	for ( int y = 0; y < 16; y++ ) {
		// 		vec3 pos = vec3( RemapRange( x, 0, 15, -10.0f, 10.0f ), 5.0f, RemapRange( y, 0, 15, -10.0f, 10.0f ) );
		// 		sirenConfig.sphereLocationsPlusColors.push_back( vec4( pos, r() ) );	// position
		// 		// sirenConfig.sphereLocationsPlusColors.push_back( vec4( 0.0f, 0.0f, 0.0f, ( p() < 0.5f ) ? 7 : 9 ) ); // color
		// 		sirenConfig.sphereLocationsPlusColors.push_back( vec4( 0.0f, 0.0f, 0.0f, ( p.noise( pos.x / 2.0f, pos.y / 2.0f, pos.z / 2.0f ) < 0.5f ) ? 11 : 12 ) ); // color
		// 	}
		// }


	/*

		// stochastic sphere packing, inside the volume
		vec3 min = vec3( -8.0f, -1.5f, -8.0f );
		vec3 max = vec3(  8.0f,  1.5f,  8.0f );
		uint32_t maxIterations = 500;
		float currentRadius = 1.3f;
		rng paletteRefVal = rng( 0.0f, 1.0f );
		int material = 6;
		vec4 currentMaterial = vec4( palette::paletteRef( paletteRefVal() ), material );
		while ( ( sirenConfig.sphereLocationsPlusColors.size() / 2 ) < sirenConfig.maxSpheres ) {
			rng x = rng( min.x + currentRadius, max.x - currentRadius );
			rng y = rng( min.y + currentRadius, max.y - currentRadius );
			rng z = rng( min.z + currentRadius, max.z - currentRadius );
			uint32_t iterations = maxIterations;
			// redundant check, but I think it's the easiest way not to run over
			while ( iterations-- && ( sirenConfig.sphereLocationsPlusColors.size() / 2 ) < sirenConfig.maxSpheres ) {
				// generate point inside the parent cube
				vec3 checkP = vec3( x(), y(), z() );
				// check for intersection against all other spheres
				bool foundIntersection = false;
				for ( uint idx = 0; idx < sirenConfig.sphereLocationsPlusColors.size() / 2; idx++ ) {
					vec4 otherSphere = sirenConfig.sphereLocationsPlusColors[ 2 * idx ];
					if ( glm::distance( checkP, otherSphere.xyz() ) < ( currentRadius + otherSphere.a ) ) {
						// cout << "intersection found in iteration " << iterations << endl;
						foundIntersection = true;
						break;
					}
				}
				// if there are no intersections, add it to the list with the current material
				if ( !foundIntersection ) {
					// cout << "adding sphere, " << currentRadius << endl;
					sirenConfig.sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
					sirenConfig.sphereLocationsPlusColors.push_back( currentMaterial );
					cout << "\r" << fixedWidthNumberString( sirenConfig.sphereLocationsPlusColors.size() / 2, 6, ' ' ) << " / " << sirenConfig.maxSpheres << " after " << Tock() / 1000.0f << "s                          ";
				}
			}
			// if you've gone max iterations, time to halve the radius and double the max iteration count, get new material
				currentMaterial = vec4( palette::paletteRef( paletteRefVal() ), material );
				currentRadius /= 1.618f;
				maxIterations *= 3;

				// doing this makes it pack flat
				// min.y /= 1.5f;
				// max.y /= 1.5f;

				// slowly shrink bounds to accentuate the earlier placed spheres
				min *= 0.95f;
				max *= 0.95f;
		}

		cout << "Packing operation completed in " << Tock() / 1000.0f << "s" << endl;

		// offset the spheres after running through, because otherwise it messes up the packing algorithm
		for ( uint i = 0; i < sirenConfig.sphereLocationsPlusColors.size() / 2; i++ ) {
			sirenConfig.sphereLocationsPlusColors[ i * 2 ] = sirenConfig.sphereLocationsPlusColors[ i * 2 ] - vec4( 0.0f, 4.0f, 0.0f, 0.0f );
		}
	*/

		// generate 50 extra, and then pop 50 off the front - will create cavities where some of the earliest, largest spheres are

	}

	void SphereRelax () {
		rng o = rng( -0.1f, 0.1f );
		int iterations = 0;
		const int maxIterations = 100;
		while ( 1 ) { // relaxation step
			// walk the list, repulstion force between pairs
			for ( uint32_t i = 0; i < sirenConfig.maxSpheres; i++ ) {
				for ( uint32_t j = i + 1; j < sirenConfig.maxSpheres; j++ ) {
					// sphere i and sphere j move slightly away from one another if intersecting
					vec4 sphereI = sirenConfig.sphereLocationsPlusColors[ 2 * i ];
					vec4 sphereJ = sirenConfig.sphereLocationsPlusColors[ 2 * j ];
					float combinedRadius = sphereI.w + sphereJ.w;
					vec3 displacement = sphereI.xyz() - sphereJ.xyz();
					float lengthDisplacement = glm::length( displacement );
					if ( lengthDisplacement < combinedRadius ) {
						const float offset = combinedRadius - lengthDisplacement;
						sirenConfig.sphereLocationsPlusColors[ 2 * i ] = glm::vec4( sphereI.xyz() + ( offset / 2.0f + o() ) * glm::normalize( displacement ), sphereI.w );
						sirenConfig.sphereLocationsPlusColors[ 2 * j ] = glm::vec4( sphereJ.xyz() - ( offset / 2.0f + o() ) * glm::normalize( displacement ), sphereJ.w );
					}
				}
			}
			// break out when there are no intersections
			bool existsIntersections = false;
			for ( uint32_t i = 0; i < sirenConfig.maxSpheres; i++ ) {
				for ( uint32_t j = i + 1; j < sirenConfig.maxSpheres; j++ ) {
					vec4 sphereI = sirenConfig.sphereLocationsPlusColors[ 2 * i ];
					vec4 sphereJ = sirenConfig.sphereLocationsPlusColors[ 2 * j ];
					float combinedRadius = sphereI.w + sphereJ.w;
					vec3 displacement = sphereI.xyz() - sphereJ.xyz();
					if ( glm::length( displacement ) < combinedRadius ) {
						existsIntersections = true;
					}
				}
			}
			// we have made sure nobody intersects, or something is hanging
			if ( !existsIntersections || ( iterations++ == maxIterations ) ) break;
		}
	}

	void SendSphereSSBO () {
		// send the data from the vector to the GPU
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, sirenConfig.sphereSSBO );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * sirenConfig.maxSpheres, ( GLvoid * ) &sirenConfig.sphereLocationsPlusColors[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, sirenConfig.sphereSSBO );
	}

	void PrepGlyphBuffer () {

		// block dimensions
		uint32_t texW = 96;
		uint32_t texH = 64;
		uint32_t texD = 512;

		texW = 256;
		texH = 256;
		texD = 1024;

		Image_4U data( texW, texH * texD );

		// create the voxel model inside the flat image
			// update the data
		const uint32_t numOps = 10;
		rngi opPick = rngi( 0, 34 );
		rngi xPick = rngi( 0, texW - 1 ); rngi xDPick = rngi( 4, 20 );
		rngi yPick = rngi( 0, texH - 1 ); rngi yDPick = rngi( 4, 12 );
		rngi zPick = rngi( 0, texD - 1 ); rngi zDPick = rngi( 4, 10 );
		rngi glyphPick = rngi( 176, 223 );
		// rngi glyphPick = rngi( 0, 255 );
		rngi thinPick = rngi( 0, 4 );
		// rngi rPick = rngi( 22, 255 );
		// rngi gPick = rngi( 4, 128 );
		// rngi bPick = rngi( 0, 255 );

		// look at a random, narrow slice of a random palette
		palette::PickRandomPalette( true );
		rng ppPick = rng( 0.1f, 0.9f );
		rng pwPick = rng( 0.01f, 0.3f );
		rngN pPick = rngN( ppPick(), pwPick() );

		color_4U col = color_4U( { 0u, 0u, 0u, 1u } );
		uint32_t length = 0;
		PerlinNoise p;

		rng noiseScale = rng( 10.0f, 50.0f );
		const vec3 n = vec3( noiseScale(), noiseScale(), 10.0f * noiseScale() );

		rng offset = rng( -10.0f, 10.0f );
		const vec3 o = vec3( offset(), offset(), offset() );

		auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
		auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();
		fnFractal->SetSource( fnSimplex );
		fnFractal->SetOctaveCount( 5 );

		rngi noiseSeedGen = rngi( 0, 1000000 );
		int noiseSeed = noiseSeedGen();
		int noiseSeed2 = noiseSeedGen();

		for ( uint32_t x = 0; x < texW; x++ )
		for ( uint32_t y = 0; y < texH; y++ )
		for ( uint32_t z = 0; z < texD; z++ ) {
			// float d = glm::distance( vec3( texW / 2.0f, texH / 2.0f, texD / 2.0f ), vec3( x, y, z ) );
			// float d2 = glm::distance( vec2( texW / 2.0f, texH / 2.0f ), vec2( x, y ) );
			// if ( d < 64.0f && d2 > 26.0f || d2 < 22.0f && d2 > 12.0f ) {

				// float noiseValue = p.noise( x / n.x + o.x, y / n.y + o.y, z / n.z + o.z );
				// if ( noiseValue < 0.5f ) {
					// vec3 c = palette::paletteRef( pPick() ) * 255.0f;
					// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
					// vec3 c = palette::paletteRef( noiseValue ) * 255.0f;
					// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
					// data.SetAtXY( x, y + z * texH, col );
				// }
			// }

			// frame test
			const ivec3 numBeams = ivec3( 2, 2, 4 );
			const ivec3 beamWidths = ivec3( 12, 16, 46 );
			const bool xValid = ( x % ( ( texW - beamWidths.x / 2 ) / numBeams.x ) < beamWidths.x );
			const bool yValid = ( y % ( ( texH - beamWidths.y / 2 ) / numBeams.y ) < beamWidths.y );
			const bool zValid = ( z % ( ( texD - beamWidths.z / 2 ) / numBeams.z ) < beamWidths.z );
			if ( ( xValid && yValid ) || ( yValid && zValid ) || ( xValid && zValid ) ) {
				// vec3 c = palette::paletteRef( 0.5f );
				// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
				// col = color_4U( { 0u, 128u, 0u, ( uint8_t ) glyphPick() } );
				vec3 c = palette::paletteRef( pPick() ) * 255.0f;
				col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
				data.SetAtXY( x, y + z * texH, col );
			}

			// // noise test
			// if ( fnFractal->GenSingle3D( x / 40.0f, y / 40.0f, z / 500.0f, noiseSeed ) < -0.4f ) {
			// 	vec3 c = palette::paletteRef( pPick() ) * 255.0f;
			// 	col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			// 	data.SetAtXY( x, y + z * texH, col );
			// }

		}
		cout << "first loop done" << endl;

		palette::PickRandomPalette( true );

		std::vector< float > noiseOutput( texW * texH * texD );
		std::vector< float > noiseOutput2( texW * texH * texD );
		// std::vector< float > noiseOutput3( texD * texH );
		fnFractal->GenUniformGrid3D( noiseOutput.data(), 0, 0, 0, texD, texH, texW, 0.01f, noiseSeed );
		fnFractal->GenUniformGrid3D( noiseOutput2.data(), 0, 0, 0, texW, texH, texD, 0.005f, noiseSeed2 );
		// fnFractal->GenUniformGrid2D( noiseOutput3.data(), 0, 0, texD, texH, 0.01f, noiseSeed2 );

		cout << "done generating noise" << endl;

		int index = 0;
		// int index2 = 0;
		for ( uint32_t x = 0; x < texW; x++ ) {
			for ( uint32_t y = 0; y < texH; y++ ) {
				float heightmapRead = fnFractal->GenSingle3D( x / 120.0f, y / 120.0f, 10.0f, noiseSeed ) * 0.5f + 0.45f;
				for ( uint32_t z = 0; z < texD; z++ ) {
					// noise test
					int i = index++;
					float noiseValue = noiseOutput[ i ];
					float noiseValue2 = noiseOutput2[ i ];
					// if ( noiseValue < -0.4f ) {
					// if ( ( z / texD ) < heightmapRead ) {
					if ( float( z ) / float( texD ) < heightmapRead && noiseValue < -0.3f ) {
						// vec3 c = palette::paletteRef( abs( noiseValue2 ) ) * 255.0f;
						vec3 c = palette::paletteRef( abs( heightmapRead ) ) * 255.0f;
						col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
						// col = color_4U( { ( uint8_t ) heightmapRead * 255, ( uint8_t ) heightmapRead * 255, ( uint8_t ) heightmapRead * 255, ( uint8_t ) glyphPick() } );
						data.SetAtXY( x, y + z * texH, col );
					}
				}
			}
		}

		cout << "second loop done" << endl;

		// beam/block test
		// palette::PickRandomPalette( true );
		// // rngi xD = rngi( texH / 2.0f - 50.0f, texH / 2.0f + 50.0f );
		// // rngi yD = rngi( texW / 2.0f - 50.0f, texW / 2.0f + 50.0f );
		// // rngi zD = rngi( texD / 2.0f - 50.0f, texD / 2.0f + 50.0f );
		// rngN xD = rngN( texH / 2.0f, 20.0f );
		// rngN yD = rngN( texW / 2.0f, 20.0f );
		// rngN zD = rngN( texD / 2.0f, 20.0f );
		// for ( int i = 0; i < 400000; i++ ) {
		// 	vec3 c = palette::paletteRef( pPick() ) * 255.0f;
		// 	col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
		// 	data.SetAtXY( uint32_t( xD() ), uint32_t( yD() ) + uint32_t( zD() ) * texD, col );
		// }

		// swatch test
		// palette::PickRandomPalette( true );
		// for ( uint x = 0; x < 16; x++ )
		// for ( uint y = 0; y < 16; y++ ) {
		// 	rngN dist = rngN( 0.0, 2.0f );
		// 	rngN distL = rngN( 0.0, 20.0f );
		// 	for ( int i = 0; i < 1000; i++ ) {
		// 		vec3 c = palette::paletteRef( ( x + 16 * y ) / 255.0f ) * 255.0f;
		// 		col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) ( x + 16 * y ) } );
		// 		data.SetAtXY( uint32_t( x * 16.0f + dist() + 8 ), uint32_t( y * 16.0f + dist() + 8 ) + uint32_t( distL() + 128 ) * texD, col );
		// 	}
		// }

		// do N randomly selected
		for ( uint32_t op = 0; op < numOps; op++ ) {
			vec3 c = palette::paletteRef( pPick() ) * 255.0f;
			switch ( opPick() ) {
			case 0: // AABB
			case 1:
			{
				// col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
				col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );

				uvec3 base = uvec3( xPick(),  yPick(),  zPick() );
				uvec3 dims = uvec3( xDPick(), yDPick(), zDPick() );
				for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
				for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
				for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
					data.SetAtXY( x, y + z * texH, col );
				break;
			}

			case 2: // lines
			case 3:
			case 4:
			case 5:
			case 6:
			{
				col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
				uvec3 base = uvec3( xPick(), yPick(), zPick() );
				length = xDPick();
				for ( uint32_t x = base.x; x < base.x + length; x++ )
					data.SetAtXY( x, base.y + base.z * texH, col );
				break;
			}

			case 7: // other lines
			case 8:
			case 9:
			case 10:
			case 11:
			{
				col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
				uvec3 base = uvec3( xPick(), yPick(), zPick() );
				length = yDPick();
				for ( uint32_t y = base.y; y < base.y + length; y++ )
					data.SetAtXY( base.x, y + base.z * texH, col );
				break;
			}

			case 12: // more other-er lines
			case 13:
			case 14:
			case 15:
			case 16:
			case 17:
			{
				col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
				uvec3 base = uvec3( xPick(), yPick(), zPick() );
				length = zDPick();
				for ( uint32_t z = base.z; z < base.z + length; z++ )
					data.SetAtXY( base.x, base.y + z * texH, col );
				break;
			}

			case 18: // should come out emissive
			case 20:
			case 21:
			{
				col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
				// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );

				uvec3 base = uvec3( xPick(), yPick(), zPick() );
				uvec3 dims = uvec3( thinPick() + 1, thinPick() + 1, thinPick() + 1 );
				for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
				for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
				for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
					data.SetAtXY( x, y + z * texH, col );
				break;
			}

			case 22: // string text
			case 23:
			case 24:
			case 25:
			case 26:
			{
				static rngi wordPick = rngi( 0, colorWords.size() - 1 );
				string word = colorWords[ wordPick() ];
				uvec3 basePt = uvec3( xPick(), yPick(), zPick() );
				for ( size_t i = 0; i < word.length(); i++ ) {
					col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) word[ i ] } );
					data.SetAtXY( basePt.x + i, basePt.y + basePt.z * texH, col );
				}
				break;
			}

			case 27:
			case 28:
			case 29:
			case 30:
			case 31:
			case 32:
			case 33:
			case 34:
			{	// clear out an AABB
				col = color_4U( { 0u, 0u, 0u, 0u } );
				uvec3 base = uvec3( xPick(),  yPick(),  zPick() );
				uvec3 dims = uvec3( xDPick(), yDPick(), zDPick() );
				for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
				for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
				for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
					data.SetAtXY( x, y + z * texH, col );
				break;
			}

			case 35:
			{
				col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
				ivec3 base = ivec3( xPick(),  yPick(),  zPick() );
				data.SetAtXY( base.x, base.y + base.z * texH, col );
				break;
			}

			default:
				break;
			}
		}

		cout << "third loop done" << endl;

		// texture is ready to be used in the pathtrace
			// create a texture, and send the data
		static bool firstRun = true;
		if ( firstRun ) {
			firstRun = false;
			textureOptions_t opts;
			opts.width			= texW;
			opts.height			= texH;
			opts.depth			= texD;
			opts.dataType		= GL_RGBA8UI;
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			opts.textureType	= GL_TEXTURE_3D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			opts.initialData = ( void * ) data.GetImageDataBasePtr();
			textureManager.Add( "TextBuffer", opts );
		} else {
			// pass the new generated texture data to the existing texture
			glBindTexture( GL_TEXTURE_3D, textureManager.Get( "TextBuffer" ) );
			glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA8UI, texW, texH, texD, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, ( void * ) data.GetImageDataBasePtr() );
		}
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		ComputePasses();			// multistage update of displayTexture
		{	// imgui is now used to show the content of the program directly via ImGui::ImageButton
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
	Siren sirenInstance;
	while( !sirenInstance.MainLoop() );
	return 0;
}
