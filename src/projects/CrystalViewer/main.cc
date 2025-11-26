#include "../../engine/engine.h"

//=================================================================================================
#include "../utils/gif.h" // gif output lib - https://github.com/charlietangora/gif-h
/* Basic Usage:
#include <vector>
#include <cstdint>
#include <gif.h>
int main() {
	int width = 100;
	int height = 200;
	std::vector< uint8_t > black( width * height * 4, 0 );
	std::vector< uint8_t > white( width * height * 4, 255 );

	auto fileName = "bwgif.gif";
	int delay = 100; // in 100th's of a second, not strictly enforced
	GifWriter g;
	GifBegin( &g, fileName, width, height, delay );
	GifWriteFrame( &g, black.data(), width, height, delay );
	GifWriteFrame( &g, white.data(), width, height, delay );
	GifEnd( &g );

	return 0;
} */
//=================================================================================================

class engineDemo final : public engineBase { // sample derived from base engine class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { EndGif(); Quit(); }

	GLuint pointBuffer = 0;
	int numPoints;
	float scale = 2.0f;
	int offset = 0;
	int n = 0; // number of points to show
	vec3 color1 = vec3( 0.3f );
	vec3 color2 = vec3( 0.9f, 0.2f, 0.0f );
	vec3 color3 = vec3( 0.0f ), color4 = vec3( 1.0f );

	const int imageWidth = 1920;
	const int imageHeight = 1080;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// config.width = imageWidth;
			// config.height = imageHeight;

			// =============================================================================================================
			// something to put some basic data in the accumulator texture
			ReloadShaders();
			// =============================================================================================================

			// texture(s) to splat into
			{
				textureOptions_t opts;
				opts.width = imageWidth;
				opts.height = imageHeight;
				opts.depth = 128;
				opts.dataType = GL_R32UI;
				opts.textureType = GL_TEXTURE_3D;
				textureManager.Add( "SplatBuffer", opts );
			}

			// buffer for the points
			// LoadCrystal();

			static rng palettePick( 0.0f, 1.0f );
			palette::PickRandomPalette();
			color1 = palette::paletteRef( palettePick() );
			color2 = palette::paletteRef( palettePick() );
			color3 = palette::paletteRef( palettePick() );
			color4 = palette::paletteRef( palettePick() );

			// start the gif process
			InitGif();
		}
	}

	void ReloadShaders () {
		shaders[ "Draw" ] = computeShader( "../src/projects/CrystalViewer/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "PointSplat" ] = computeShader( "../src/projects/CrystalViewer/shaders/pointSplat.cs.glsl" ).shaderHandle;
	}

				// additional conditioning step to scale this set of points to a manageable volume ahead of trying to splat it
				vec3 minExtents = vec3( crystalPoints[ 0 ].xyz() );
				vec3 maxExtents = vec3( crystalPoints[ 0 ].xyz() );
				for ( const auto& crystalPoint : crystalPoints ) {
					minExtents = glm::min( minExtents, crystalPoint.xyz() );
					maxExtents = glm::max( maxExtents, crystalPoint.xyz() );
				}

				// position + scaling based on this info
				vec3 midpoint = ( minExtents + maxExtents ) / 2.0f;
				float maxSpan = std::max( std::max( maxExtents.y - minExtents.y, maxExtents.z - minExtents.z ), maxExtents.x - minExtents.x );
				mat4 transform = glm::translate( glm::scale( mat4( 1.0f ), vec3( 1.0f / maxSpan ) ), -midpoint );

				cout << "Processed " << numPoints << " Points" << endl;
				cout << "Detected Max Span: " << maxSpan << endl;
				cout << "Centering About Midpoint: " << to_string( midpoint ) << endl;

				minExtents = vec3( 1000.0f );
				maxExtents = vec3( -1000.0f );
				for ( auto& crystalPoint : crystalPoints ) {
					crystalPoint = transform * crystalPoint;
					minExtents = glm::min( minExtents, crystalPoint.xyz() );
					maxExtents = glm::max( maxExtents, crystalPoint.xyz() );
				}

				midpoint = ( minExtents + maxExtents ) / 2.0f;
				maxSpan = std::max( std::max( maxExtents.y - minExtents.y, maxExtents.z - minExtents.z ), maxExtents.x - minExtents.x );
				cout << "After Transform..." << endl;
				cout << "Detected Max Span: " << maxSpan << endl;
				cout << "Centering About Midpoint: " << to_string( midpoint ) << endl;
				cout << "MinExtents: " << to_string( minExtents ) << endl;
				cout << "MaxExtents: " << to_string( maxExtents ) << endl;

				glBufferData( GL_SHADER_STORAGE_BUFFER, crystalPoints.size() * sizeof( vec4 ), crystalPoints.data(), GL_DYNAMIC_COPY );
				glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, pointBuffer );
			}
		}
	}

	void ReloadShaders () {
		shaders[ "Draw" ] = computeShader( "../src/projects/CrystalViewer/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "PointSplat" ] = computeShader( "../src/projects/CrystalViewer/shaders/pointSplat.cs.glsl" ).shaderHandle;
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// zoom in and out with plus/minus
		if ( inputHandler.getState( KEY_MINUS ) ) {
			scale *= 0.99f;
		}
		if ( inputHandler.getState( KEY_EQUALS ) ) {
			scale /= 0.99f;
		}

		if ( inputHandler.getState4( KEY_R ) == KEYSTATE_RISING ) {
			static rng rotationG = rng( 0, jbDE::tau );
			trident.RotateX( rotationG() );
			trident.RotateY( rotationG() );
			trident.RotateZ( rotationG() );
		}

		if ( inputHandler.getState( KEY_T ) ) {
			screenshotIndicated = true;
			SDL_Delay( 10 );
		}

		if ( inputHandler.getState( KEY_Y ) ) {
			ReloadShaders();
		}

		if ( inputHandler.getState( KEY_G ) ) {
			EndGif();
		}

		if ( inputHandler.getState4( KEY_K ) == KEYSTATE_RISING ) {
			static rng palettePick( 0.0f, 1.0f );
			palette::PickRandomPalette();
			color1 = palette::paletteRef( palettePick() );
			color2 = palette::paletteRef( palettePick() );
			color3 = palette::paletteRef( palettePick() );
			color4 = palette::paletteRef( palettePick() );
			textureManager.ZeroTexture2D( "Accumulator" );
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

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );

		ImGui::Begin( "Controls" );
		ImGui::SliderFloat( "Percentage", &animRatio, 0.0f, 1.0f );
		static std::vector< string > savesList;
		if ( savesList.size() == 0 ) { // get the list
			struct pathLeafString {
				std::string operator()( const std::filesystem::directory_entry &entry ) const {
					return entry.path().string();
				}
			};
			// list of exrs, kept in Documents/EXRs/ (which is ../Crystals)
			std::filesystem::path p( "../Crystals" );
			std::filesystem::directory_iterator start( p );
			std::filesystem::directory_iterator end;
			std::transform( start, end, std::back_inserter( savesList ), pathLeafString() );
			// std::sort( savesList.begin(), savesList.end() ); // sort these alphabetically
		}

#define LISTBOX_SIZE_MAX 256
		const char *listboxItems[ LISTBOX_SIZE_MAX ];
		uint32_t i;
		for ( i = 0; i < LISTBOX_SIZE_MAX && i < savesList.size(); ++i ) {
			listboxItems[ i ] = savesList[ i ].c_str();
		}

		ImGui::Text( "Crystals:" );
		static int listboxSelected = 0;
		ImGui::ListBox( " ", &listboxSelected, listboxItems, i, 24 );

		if ( ImGui::Button( "Load Selected" ) ) {
			LoadCrystal( savesList[ listboxSelected ] );
			static rng palettePick( 0.0f, 1.0f );
			palette::PickRandomPalette();
			color1 = palette::paletteRef( palettePick() );
			color2 = palette::paletteRef( palettePick() );
			color3 = palette::paletteRef( palettePick() );
			color4 = palette::paletteRef( palettePick() );

			static rng rotationG = rng( 0, jbDE::tau );
			trident.RotateX( rotationG() );
			trident.RotateY( rotationG() );
			trident.RotateZ( rotationG() );
		}

		ImGui::SameLine();
		if ( ImGui::Button( "Random" ) ) {
			LoadCrystal();
			static rng palettePick( 0.0f, 1.0f );
			palette::PickRandomPalette();
			color1 = palette::paletteRef( palettePick() );
			color2 = palette::paletteRef( palettePick() );
			color3 = palette::paletteRef( palettePick() );
			color4 = palette::paletteRef( palettePick() );

			static rng rotationG = rng( 0, jbDE::tau );
			trident.RotateX( rotationG() );
			trident.RotateY( rotationG() );
			trident.RotateZ( rotationG() );
		}

		ImGui::End();
	}

	bool screenshotIndicated = false;
	Image_4F scratch;
	Image_4U scratchU;
	void CaptureDisplayIntoScratch () {
		const GLuint tex = textureManager.Get( "Display Texture" );
		uvec2 dims = textureManager.GetDimensions( "Display Texture" );
		std::vector< float > imageBytesToSave;
		imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
		glBindTexture( GL_TEXTURE_2D, tex );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
		scratch = Image_4F( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
		scratchU = Image_4U( dims.x, dims.y );
	}

	GifWriter g;
	void InitGif () {
		// initialize the gif
		const string filename = string( "crystalAnim-" ) + timeDateString() + string( ".gif" );
		GifBegin( &g, filename.c_str(), imageWidth, imageHeight, 4 );
	}

	void AddToGif () {
		CaptureDisplayIntoScratch();
		scratch.FlipVertical();
		scratch.RGBtoSRGB();
		// add to the running gif
		for ( int y = 0; y < imageHeight; y++ )
		for ( int x = 0; x < imageWidth; x++ ) {
			color_4F colorF = scratch.GetAtXY( x, y );
			color_4U color;
			color[ red ] = uint8_t( 255 * colorF[ red ] );
			color[ green ] = uint8_t( 255 * colorF[ green ] );
			color[ blue ] = uint8_t( 255 * colorF[ blue ] );
			color[ alpha ] = uint8_t( 255 * colorF[ alpha ] );
			scratchU.SetAtXY( x, y, color );
		}
		GifWriteFrame( &g, scratchU.GetImageDataBasePtr(), imageWidth, imageHeight, 4 );
	}

	void EndGif () {
		GifEnd( &g );
	}

	void Screenshot () {
		CaptureDisplayIntoScratch();
		scratch.FlipVertical();
		scratch.RGBtoSRGB();
		const string filename = string( "crystalFrame-" ) + timeDateString() + string( ".png" );
		scratch.Save( filename );
	}

	float animRatio = 1.0f;
	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			static rngi wangSeeder = rngi( 0, 10000000 );
			glUniform1i( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );

			vec3 c1 = glm::mix( color1, color3, pow( animRatio, 0.5f ) );
			vec3 c2 = glm::mix( color2, color4, pow( animRatio, 2.0f ) );
			glUniform3fv( glGetUniformLocation( shader, "color1" ), 1, glm::value_ptr( c1 ) );
			glUniform3fv( glGetUniformLocation( shader, "color2" ), 1, glm::value_ptr( c2 ) );

			textureManager.BindImageForShader( "SplatBuffer", "SplatBuffer", shader, 2 );

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

		if ( screenshotIndicated ) {
			screenshotIndicated = false;
			Screenshot();
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

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code

		// we know if something happened and we need to redraw...
		// static int numDraws = 16;
		// if ( trident.Dirty() || ( numDraws > 0 ) ) {
			// we need to resplat points
			// numDraws--;

		if ( trident.Dirty() ) {
				// numDraws = 16;
			// }
			textureManager.ZeroTexture3D( "SplatBuffer" );
			textureManager.ZeroTexture2D( "Accumulator" );
			// }

			const GLuint shader = shaders[ "PointSplat" ];
			glUseProgram( shader );
			const int workgroupsRoundedUp = ( numPoints + 63 ) / 64;
			glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( trident.basisX ) );
			glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( trident.basisY ) );
			glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( trident.basisZ ) );
			glUniform1i( glGetUniformLocation( shader, "n" ), animRatio *  numPoints );
			glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
			glUniform1f( glGetUniformLocation( shader, "animRatio" ), ( animRatio ) );

			textureManager.BindImageForShader( "SplatBuffer", "SplatBuffer", shader, 2 );

			static rngi wangSeeder = rngi( 0, 10000000 );
			for ( int i = 0; i < 4; i++ ) {
				glUniform1i( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );
				glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
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

// #pragma comment( linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup" )

int main ( int argc, char *argv[] ) {
	engineDemo engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
