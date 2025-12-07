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
			// InitGif();

			for ( int i = 40; i < 49; ++i ) {
				string numberstring = ( i < 10 ) ? ( "0" + to_string( i ) ) : to_string( i );
				ProcessCrystal( "../Crystals/crystal" + numberstring + ".png", "../Crystals/crystal" + numberstring + ".ply" );
			}

			config.oneShot = true;
		}
	}

	void ReloadShaders () {
		shaders[ "Draw" ] = computeShader( "../src/projects/CrystalViewer/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "PointSplat" ] = computeShader( "../src/projects/CrystalViewer/shaders/pointSplat.cs.glsl" ).shaderHandle;
	}

	void LoadCrystal ( int num = -1 ) {
		static rngi pick( 1, 40 );
		LoadCrystal( "../Crystals/crystal" + to_string( num == -1 ? pick() : num ) + ".png" );
	}

	void LoadCrystal ( string path ) {
		cout << "loading crystal: " << path << endl;
		if ( !pointBuffer )
			glCreateBuffers( 1, &pointBuffer );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, pointBuffer );

		// loading the data from disk... it's a linear array of mat4's, so let's go ahead and process it down to vec4's by transforming p0 by that mat4
		constexpr vec4 p0 = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
		std::vector< vec4 > crystalPoints;

		Image_4U matrixBuffer( path );
		n = numPoints = ( matrixBuffer.Height() - 1 ) * ( matrixBuffer.Width() / 16 ); // 1024 mat4's per row, small crop of bottom row for safety
		crystalPoints.resize( numPoints );

		mat4 *dataAsMat4s = ( mat4 * ) matrixBuffer.GetImageDataBasePtr();
		for ( int i = 0; i < numPoints; ++i ) {
			crystalPoints[ i ] = dataAsMat4s[ i ] * p0;
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

	void ProcessCrystal ( string path, string outpath ) {
		cout << "loading crystal: " << path << endl;

		// loading the data from disk... it's a linear array of mat4's, so let's go ahead and process it down to vec4's by transforming p0 by that mat4
		constexpr vec4 p0 = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
		std::vector< vec4 > crystalPoints;

		Image_4U matrixBuffer( path );
		n = numPoints = ( matrixBuffer.Height() - 1 ) * ( matrixBuffer.Width() / 16 ); // 1024 mat4's per row, small crop of bottom row for safety
		crystalPoints.resize( numPoints );

		mat4 *dataAsMat4s = ( mat4 * ) matrixBuffer.GetImageDataBasePtr();
		for ( int i = 0; i < numPoints; ++i ) {
			crystalPoints[ i ] = dataAsMat4s[ i ] * p0;
			crystalPoints[ i ].w = i;
		}

/*
		// additional conditioning step to scale this set of points to a manageable volume ahead of trying to splat it
		vec3 minExtents = vec3( crystalPoints[ 0 ].xyz() );
		vec3 maxExtents = vec3( crystalPoints[ 0 ].xyz() );
		for ( const auto& crystalPoint : crystalPoints ) {
			minExtents = glm::min( minExtents, crystalPoint.xyz() );
			maxExtents = glm::max( maxExtents, crystalPoint.xyz() );
		}

		cout << "Solved extents: " << to_string( minExtents ) << ", " << to_string( maxExtents ) << endl;

		// dump all data into the hashmap...
		std::unordered_map< ivec3, std::shared_ptr< std::vector< vec4 > > > hashCollection;
		uint32_t gridCellsCreated = 0;
		for ( const auto& crystalPoint : crystalPoints ) {
			const ivec3 iC = ivec3( crystalPoint.xyz() );
			// cout << "Processing point " << to_string( crystalPoint.xyz() ) << endl;
			// cout << "binned: " << to_string( iC ) << endl;
			if ( hashCollection[ iC ] == nullptr) { // pointer is null, create the object
				// cout << "Creating new gridcell..." << endl;
				hashCollection[ iC ] = std::make_shared< std::vector< vec4 > >();
				gridCellsCreated++;
			} else {
				// cout << "Gridcell exists." << endl;
			}
			hashCollection[ iC ]->emplace_back( crystalPoint );
		}

		// once we have all the data in the hashmap, we have a grid based acceleration structure to judge against
			// we basically need to go over all the occupied cells in the hashmap...
		std::vector< vec4 > locations;
		uint32_t completed = 0;
		uint32_t failed = 0;
		for ( auto& ptr : hashCollection ) {
			if ( ptr.second->size() == 0 ) { cout << "this is bad" << endl; continue; }
			if ( ptr.second->size() == 1 ) { locations.emplace_back( ptr.second->at( 0 ) ); continue; }

			const ivec3 iC = ivec3( ptr.first );
			const auto& s = { -1, 0, 1 };
			std::vector< vec4 > localList;
			localList.reserve( 256 );
			for ( int x : s )
				for ( int y : s )
					for ( int z : s ) {
						if ( hashCollection[ iC + ivec3( x, y, z ) ] != nullptr ) {
							for ( int i = 0; i < hashCollection[ iC + ivec3( x, y, z ) ]->size(); ++i ) {
								// we need to collect all the local points
								localList.push_back( hashCollection[ iC + ivec3( x, y, z ) ]->at( i ) );
							}
						}
					}

			cout << "constructed local list of : " << to_string( localList.size() ) << endl;
			// once we have the local list sort by w, and let's see if the current point is the first one in the list within epsilon of this location
			std::sort( localList.begin(), localList.end(), []( vec4 a, vec4 b ) { return a.w < b.w; } );

			// this sucks and is slow, but the problem size is limited for each given particle, to the neighboring grid cells in the hashmap

			// for the list of points in our current gridcell
			shared_ptr< vector< vec4 > > pointsRef = ptr.second;
			if ( pointsRef != nullptr ) {
				for ( int i = 0; i < pointsRef->size(); i++ ) {
					// for the local list... neighborhood points
					for ( auto& p : localList ) {

						// determine if p is already in the list...
						for ( auto& loc : locations ) {
							if ( int( loc.w ) == int( p.w ) ) {
								continue; // no chance, pal
							}
						}

						// if this point in the neighborhood comes before our point, we need to evaluate the distance
						if ( p.w <= pointsRef->at( i ).w ) {
							// if we're within this epsilon, let's only keep the one comes *first* that is, p - by now, we know p is not in the list
							if ( glm::distance( p.xyz(), pointsRef->at( i ).xyz() ) < 0.0001f ) {
								// if I'm first, I go in the list... if there is another point within epsilon, earlier in the list, continue without adding
								if ( locations.size() % 10000 == 0 ) {
									cout << "adding " << locations.size() << " locations" << endl;
								}
								locations.push_back( p );
								// breaking out of local list check, we have decided we have a point to represent this point, sp
								break;
							}
						}
					}
				}
			}

			localList.clear();
		}
		std::sort( locations.begin(), locations.end(), []( vec4 a, vec4 b ) { return a.w < b.w; } );
*/

		happly::PLYData plyOut;

		// convert to vector<vector<float>>
		vector< float > dataX;
		vector< float > dataY;
		vector< float > dataZ;
		vector< float > dataOrder;
		// for ( auto& loc : locations ) {
		for ( const auto& crystalPoint : crystalPoints ) {
			dataX.push_back( crystalPoint.x );
			dataY.push_back( crystalPoint.y );
			dataZ.push_back( crystalPoint.z );
			dataOrder.push_back( crystalPoint.w / float( crystalPoints.size() ) );
		}

		// Add elements
		plyOut.addElement( "Points", dataX.size() );

		// Add properties to those elements
		plyOut.getElement( "Points" ).addProperty< float >( "X", dataX );
		plyOut.getElement( "Points" ).addProperty< float >( "Y", dataY );
		plyOut.getElement( "Points" ).addProperty< float >( "Z", dataZ );
		plyOut.getElement( "Points" ).addProperty< float >( "Order", dataOrder );

		// Write the object to file
		plyOut.write( outpath, happly::DataFormat::Binary );
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

		static bool animate = false;
		if ( animate ) {
			// keepeing track of frames...
			static int i = 0;
			static int frame = 0;
			const int numFrames = 100;
			static int rotation = 0;
			const int numRotations = 5;
			static rngi crystalPick( 1, 23 );
			static rng rotationG = rng( 0, jbDE::tau );
			static rng scaleG = rng( 1.2f, 1.8f );
			static rngN scaleR = rngN( 0.0f, 1.0f );
			static bool firstTime = true;
			static vec3 rotScale = vec3( 1.0f );

			if ( frame == numFrames || firstTime ) {
				rotation++;
				firstTime = false;
				frame = 0;
				i = 0;

				scale = scaleG();
				rotScale = vec3( scaleR(), scaleR(), scaleR() );
				trident.RotateX( rotationG() );
				trident.RotateY( rotationG() );
				trident.RotateZ( rotationG() );

				// lets get some new colors...
				static rng palettePick( 0.0f, 1.0f );
				palette::PickRandomPalette();
				// color1 = palette::paletteRef( palettePick() );
				// color2 = palette::paletteRef( palettePick() );
				color1 = color3;
				color2 = color4;
				color3 = palette::paletteRef( palettePick() );
				color4 = palette::paletteRef( palettePick() );
				LoadCrystal( crystalPick() );
			}

			// a frame is finished...
			if ( i++ >= 100 ) {
				frame++;
				i = 0;
				AddToGif();
				trident.RotateX( 0.005f * rotScale.x );
				trident.RotateY( 0.001f * rotScale.y );
				trident.RotateZ( 0.001f * rotScale.z );

				textureManager.ZeroTexture3D( "SplatBuffer" );
				textureManager.ZeroTexture2D( "Accumulator" );

				animRatio = float( frame ) / float( numFrames );
				// n = ( animRatio ) * numPoints;
				n = numPoints;

				const GLuint shader = shaders[ "PointSplat" ];
				glUseProgram( shader );
				const int workgroupsRoundedUp = ( numPoints + 63 ) / 64;
				glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( trident.basisX ) );
				glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( trident.basisY ) );
				glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( trident.basisZ ) );

				glUniform1i( glGetUniformLocation( shader, "n" ), n );
				glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
				glUniform1f( glGetUniformLocation( shader, "animRatio" ), ( animRatio ) );

				textureManager.BindImageForShader( "SplatBuffer", "SplatBuffer", shader, 2 );

				for ( int i = 0; i < 16; i++ ) {
					glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
					glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
				}

				cout << "advancing to frame " << frame << " of " << numFrames << endl;
			}
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
