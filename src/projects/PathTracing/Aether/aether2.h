#pragma once

#include "../../../engine/engine.h"
#include "spectralToolkit.h"

class AetherConfig_t {

// light class used for:
	// imgui interaction
	// preparation of GPU data
	class Light {
	public:
	// parameters for the emitter
		int idx;
		Light ( int idx_in ) : idx ( idx_in ) {}

		// basic parameterization
		vec3 position, direction;
		vec2 spread;

		// describing the source distribution
		int maskSelect;
		vec2 maskScale;

	// parameters for the light
		int sourcePDF = 0;
		std::vector< int > gelStack;
		std::vector< float > PDFScratch;
		Image_4U PDFPreview{ 450 + 104, 64 };

	// later, we need to be able to get:
		// access to the parameter data
		// the SSBO representation ( 14 floats, pad to 16 )
		// iCDF strip representation, with filter stack applied
	};

	// current list of lights
	std::vector< Light > lightList;

	// add a default constructed light
	void AddLight () {
		static int lightIndexer = 0;
		int myIndex = lightIndexer++;
		lightList.emplace_back( myIndex );
		ComputeLightStack( myIndex );
	}

	// remove this one from the list
	void RemoveLight ( int idx ) {
		// todo
	}

	// randomize the parameters for a given light
	void RandomizeLight ( int idx ) {
		// todo, need to determine parameter ranges

		// pick a new light
		rngi lightPick = rngi( 0, numSourcePDFs - 1 );
		lightList[ idx ].sourcePDF = lightPick();

		// recompute the preview
		ComputeLightStack( idx );
	}

	// memory associated with the xRite color chip reflectances
	const float** xRiteReflectances = nullptr;

	// memory associated with the source PDFs
	int numSourcePDFs = 0;
	const float** sourcePDFs = nullptr;
	const char** sourcePDFLabels = nullptr;

	// information about the gel filters
	int numGelFilters = 0;
	const float** gelFilters = nullptr;
	const char** gelFilterLabels = nullptr;
	const char** gelFilterDescriptions = nullptr;
	const vec3* gelPreviewColors = nullptr;

	void PrecomputesRGBReflectances () {
		// populating the xRite color checker card
		const vec3 sRGBConstants[] = {
			vec3( 115,  82,  68 ), // dark skin
			vec3( 194, 150, 120 ), // light skin
			vec3(  98, 122, 157 ), // blue sky
			vec3(  87, 108,  67 ), // foliage
			vec3( 133, 128, 177 ), // blue flower
			vec3( 103, 189, 170 ), // bluish green
			vec3( 214, 126,  44 ), // orange
			vec3(  80,  91, 166 ), // purplish blue
			vec3( 193,  90,  99 ), // moderate red
			vec3(  94,  60, 108 ), // purple
			vec3( 157, 188,  64 ), // yellow green
			vec3( 244, 163,  46 ), // orange yellow
			vec3(  56,  61, 150 ), // blue
			vec3(  70, 148,  73 ), // green
			vec3( 175,  54,  60 ), // red
			vec3( 231, 199,  31 ), // yellow
			vec3( 187,  86, 149 ), // magenta
			vec3(   8, 133, 161 ), // cyan
			vec3( 243, 243, 242 ), // white
			vec3( 200, 200, 200 ), // neutral 8
			vec3( 160, 160, 160 ), // neutral 6.5
			vec3( 122, 122, 121 ), // neutral 5
			vec3(  85,  85,  85 ), // neutral 3.5
			vec3(  52,  52,  52 ) // black
		};

	// there is some resources associated with this sampling process...
		// we first need to load the LUT from Jakob's paper
		// https://rgl.epfl.ch/publications/Jakob2019Spectral
		RGB2Spec *model = rgb2spec_load( "../src/data/Jakob2019Spectral/supplement/tables/srgb.coeff" );

		// once we have that, we can use an sRGB constant to derive a reflectance curve
			// we are going to do that by 1nm bands to match the other data
		xRiteReflectances = ( const float ** ) malloc( 24 * sizeof( float * ) );

		// for each of the reflectances
		for ( int i = 0; i < 24; i++ ) {
			// first step get the reflectance coefficients
			float rgb[ 3 ] = { sRGBConstants[ i ].r / 255.0f, sRGBConstants[ i ].g / 255.0f, sRGBConstants[ i ].b / 255.0f }, coeff[ 3 ];

			rgb2spec_fetch( model, rgb, coeff );
			// printf( "fetch(): %f %f %f\n", coeff[ 0 ], coeff[ 1 ], coeff[ 2 ] );

			// allocate and populate the reflectance corresponding to this color chip
			xRiteReflectances[ i ] = ( const float * ) malloc( 450 * sizeof( float ) );
			for ( int l = 0; l < 450; l++ ) {
				( float& ) xRiteReflectances[ i ][ l ] = rgb2spec_eval_precise( coeff, float( l + 380 ) );
			}
		}
	}

	void LoadPDFData () {
		// setup the texture with rows for the specific light types, for the importance sampled emission spectra
		const string LUTPath = "../src/data/spectraLUT/Preprocessed";

		// need to populate the array of LUT filenames
		if ( std::filesystem::exists( LUTPath ) && std::filesystem::is_directory( LUTPath ) ) {
			// Iterate over the directory contents
			std::vector< std::filesystem::path > paths;
			for ( const auto& entry : std::filesystem::directory_iterator( LUTPath ) ) {
				// Check if the entry is a regular file
				if ( std::filesystem::is_regular_file( entry.status() ) ) {
					paths.push_back( entry.path() );
					cout << "adding " << entry.path().filename().stem() << endl;
				}
			}

			// we have a list of filenames, now we need to create the buffers to hold the data + labels
			sourcePDFs = ( const float ** ) malloc( paths.size() * sizeof( const float * ) );
			sourcePDFLabels = ( const char ** ) malloc( paths.size() * sizeof( const char * ) );

			for ( size_t i = 0u; i < paths.size(); i++ ) {
				// populate the labels
				sourcePDFLabels[ i ] = ( const char * ) malloc( strlen( paths[ i ].filename().stem().string().c_str() ) + 1 );
				strcpy( ( char * ) sourcePDFLabels[ i ], paths[ i ].filename().stem().string().c_str() );

				// we need to process each of the source distributions into a PDF
				sourcePDFs[ i ] = ( const float * ) malloc( 450 * sizeof( float ) );

				// load the referenced data to decode the emission spectra PDF
				Image_4F pdfLUT( paths[ i ].string() );
				for ( int x = 0; x < pdfLUT.Width(); x++ ) {
					float sum = 0.0f;
					for ( int y = 0; y < pdfLUT.Height(); y++ ) {
						// invert because lut uses dark for positive indication... maybe fix that
						sum += 1.0f - pdfLUT.GetAtXY( x, y ).GetLuma();
					}
					( float& ) sourcePDFs[ i ][ x ] = sum;
				}

				// and increment count
				numSourcePDFs++;

				/*
				// and the debug dump
				cout << "adding source distribution: " << endl << sourcePDFLabels[ i ] << endl;
				for ( size_t x = 0; x < pdfLUT.Width(); x++ ) {
					cout << " " << sourcePDFs[ i ][ x ];
				}
				cout << endl << endl;
				*/
			}

		} else {
			std::cerr << "Directory does not exist or is not a directory." << std::endl;
		}
	}

	void LoadGelFilterData () {
		// loading the initial data from the JSON records
		json gelatinRecords;
		ifstream i( "../src/data/LeeGelList.json" );
		i >> gelatinRecords; i.close();

		struct gelRecord {
			string label;
			string description;
			vec3 previewColor;
			std::vector< float > filterData;
		};

		std::vector< gelRecord > gelRecords;

		// iterating through and finding all the gel filters that have nonzero ("valid") spectral data
		for ( auto& e : gelatinRecords ) {
			// getting the data we need... problem is we don't know ahead of time, how many there are

			// Need to do some processing to separate label and description
			string text = e[ "text" ];
			size_t firstPos = text.find_first_not_of( " \n" );
			size_t numEnd = text.find( ' ', firstPos );
			std::string number = text.substr( firstPos, numEnd - firstPos );
			size_t secondPos = text.find( number, numEnd );

			// also some processing in anticipation of needing the color, too
			string c = e[ "color" ];
			std::transform( c.begin(), c.end(), c.begin(), [] ( unsigned char cf ) { return std::tolower( cf ); } );
			vec3 color = HexToVec3( c );

			// going through the filter is where we will be able to determine if this entry is valid or not
			bool valid = true;
			vector< float > filter;
			vector< float > filterScratch;
			filter.clear();
			if ( e.contains( "datatext" ) ) {
				for ( int lambda = 405;; lambda += 5 ) {
					if ( e[ "datatext" ].contains( to_string( lambda ) ) ) {
						filter.push_back( std::stof( e[ "datatext" ][ to_string( lambda ) ].get< string >() ) / 100.0f );
					} else {
						// loop exit
						if ( filter.size() != 0 )
							filter.push_back( filter[ filter.size() - 1 ] );
						break;
					}
				}
			}

			// we want to dismiss under two conditions:
				// zero length filter (filter data was not included)
				// filter is all zeroes (filter data was replaced with placeholder)
			if ( filter.size() != 0 ) {
				// let's also determine that we have valid coefficients:
				bool allZeroes = true;
				for ( int f = 0; f < filter.size(); f++ ) {
					if ( filter[ f ] != 0.0f ) {
						allZeroes = false;
					}
				}
				// all zeroes means invalid
				if ( allZeroes ) {
					valid = false;
				} else {
					// great - we have valid filter data...
						// let's pad out the edges and interpolate from 400-700 by 5's to 380-830 by 1's for the engine

					// low side pad with value in index 0 (optionally you could make this 1's or 0's, as desired)
					for ( int w = 380; w < 400; w++ ) {
						filterScratch.push_back( filter[ 0 ] );
					}

					// interpolate the middle section, 400-700nm
					float vprev = filter[ 0 ];
					float v = filter[ 1 ];
					for ( int wOffset = 1; wOffset < filter.size(); wOffset++ ) {
						// each entry spawns 5 elements
						for ( int j = 0; j < 5; j++ ) {
							filterScratch.push_back( glm::mix( vprev, v, ( j + 0.5f ) / 5.0f ) );
						}
						// cycle in the new values
						vprev = v;
						if ( wOffset < filter.size() ) {
							v = filter[ wOffset ];
						}
					}

					// high side pad with value in final index (also optionally force 1's or 0's, if you want)
					while ( filterScratch.size() < 450 ) {
						filterScratch.push_back( filter[ filter.size() - 1 ] );
					}
				}
			} else {
				// empty filter means invalid
				valid = false;
			}

			if ( valid ) {
				gelRecord g;

				// we can split now labels and description strings
				g.label = text.substr( firstPos, secondPos - firstPos );
				g.description = text.substr( secondPos );

				// also want to extract color from the hex codes
				g.previewColor = color;

				// and of course the filter data
				g.filterData = filterScratch;

				// we are going to collect these together to make it easier to sort
				numGelFilters++;
				gelRecords.push_back( g );
			}
		}

		// we have now constructed a list of filter datapoints... sort by labels, so we have basically ascending color codes
		std::sort( gelRecords.begin(), gelRecords.end(), [] ( gelRecord g1, gelRecord g2 ) { return g1.label < g2.label; } );

		// now we need to do the allocations for the menus - this is separated out for labels and descriptions, preview colors and filter coefficients
		gelFilters = ( const float ** ) malloc( numGelFilters * sizeof( const float * ) );
		gelFilterLabels = ( const char ** ) malloc( numGelFilters * sizeof( const char * ) );
		gelFilterDescriptions = ( const char ** ) malloc( numGelFilters * sizeof( const char * ) );
		gelPreviewColors = ( const vec3* ) malloc( numGelFilters * sizeof( const vec3 ) );

		for ( size_t i = 0; i < numGelFilters; i++ ) {
			// Allocate memory for each string and copy it
			gelFilterLabels[ i ] = ( const char * ) malloc( strlen( gelRecords[ i ].label.c_str() ) + 1 );
			strcpy( ( char * ) gelFilterLabels[ i ], gelRecords[ i ].label.c_str() );  // Copy the string

			gelFilterDescriptions[ i ] = ( const char * ) malloc( strlen( gelRecords[ i ].description.c_str() ) + 1 );
			strcpy( ( char * ) gelFilterDescriptions[ i ], gelRecords[ i ].description.c_str() );

			// just do the sRGB convert and avoid doing it every frame
			vec4 sRGB = vec4( gelRecords[ i ].previewColor[ 0 ], gelRecords[ i ].previewColor[ 1 ], gelRecords[ i ].previewColor[ 2 ], 255 );
			bvec4 cutoff = lessThan( sRGB, vec4( 0.04045f ) );
			vec4 higher = pow( ( sRGB + vec4( 0.055f ) ) / vec4( 1.055f ), vec4( 2.4f ) );
			vec4 lower = sRGB / vec4( 12.92f );
			gelRecords[ i ].previewColor =  mix( higher, lower, cutoff );
			( vec3& ) gelPreviewColors[ i ] = gelRecords[ i ].previewColor;

			// filter coefficients slightly more
			gelFilters[ i ] = ( const float * ) malloc( 450 * sizeof( const float ) );
			for ( size_t j = 0; j < 450; j++ ) {
				( float& ) gelFilters[ i ][ j ] = gelRecords[ i ].filterData[ j ];
			}

			/*
			// debug dump would be useful here
			cout << "Created Record: " << endl << gelFilterLabels[ i ] << endl;
			cout << gelFilterDescriptions[ i ] << endl;
			cout << to_string( gelPreviewColors[ i ] ) << endl;
			for ( size_t j = 0; j < 450; j++ ) {
				cout << gelFilters[ i ][ j ];
			}
			cout << endl << endl;
			*/
		}

	}

	// Starting from the selected source PDF, with no filters applied.
	// We need a curve out of this process representing the filtered light.
	void ComputeLightStack ( int lightListIdx ) {
		Light& light = lightList[ lightListIdx ];

		auto LoadPDF = [&] ( Light &light, int idx ) {
			light.PDFScratch.clear();
			for ( int i = 0; i < 450; i++ ) {
				light.PDFScratch.push_back( sourcePDFs[ idx ][ i ] );
			}
		};

		auto ApplyFilter = [&] ( Light &light, int idx ) {
			for ( int i = 0; i < 450; i++ ) {
				light.PDFScratch[ i ] *= gelFilters[ idx ][ i ];
			}
		};

		auto NormalizePDF	= [&] ( Light &light ) {
			float max = 0.0f;
			for ( int i = 0; i < 450; i++ ) {
				// first pass, determine the maximum
				max = std::max( max, light.PDFScratch[ i ] );
			}
			for ( int i = 0; i < 450; i++ ) {
				// second pass we perform the normalization by the observed maximum
				light.PDFScratch[ i ] /= max;
			}
		};

		// load the selected source PDF
		LoadPDF( light, light.sourcePDF );
		NormalizePDF( light );

		// for each selected gel filter
		for ( int i = 0; i < light.gelStack.size(); i++ ) {
			// apply selected gel filter
			ApplyFilter( light, light.gelStack[ i ] );

			// renormalize the PDF
			NormalizePDF( light );
		}

		// clearing the image
		light.PDFPreview.Swizzle( "0001" );

		// at the end, you have the updated light PDF...
			// let's go ahead and compute the preview...
		int xOffset = 0;
		for ( auto& freqBand : light.PDFScratch ) {
			// we know the PDF value at this location...
			for ( int y = 0; y < light.PDFPreview.Height(); y++ ) {
				float fractionalPosition = 1.0f - float( y ) / float( light.PDFPreview.Height() );
				if ( fractionalPosition < freqBand ) {
					// we want to use a representative color for the frequency...
					vec3 c = wavelengthColorLDR( xOffset + 380 ) * 255.0f;
					light.PDFPreview.SetAtXY( xOffset, y, Image_4U::color( { uint8_t( c.x ), uint8_t( c.y ), uint8_t( c.z ), 255 } ) );
				} else {
					// write clear... maybe a grid pattern?
					float xWave = sin( xOffset * 0.5f );
					float yWave = sin( y * 0.5f );
					const float p = 40.0f;
					uint8_t v = std::max( 32.0f * pow( ( 16 + 15 * xWave ) / 32.0f, p ), 32.0f * pow( ( 16 + 15 * yWave ) / 32.0f, p ) );
					light.PDFPreview.SetAtXY( xOffset, y, Image_4U::color( { v, v, v, 255 } ) );
				}
			}
			xOffset++;
		}

		// we know the sRGB xRite color chip reflectances, and the light emission... we need to convolve to get the color result
		vec3 color[ 24 ];
		for ( int chip = 0; chip < 24; chip++ ) {
			// initial accumulation value
			color[ chip ] = vec3( 0.0f );

			// we need to iterate over wavelengths and get an average color value under this illuminant
			for ( int y = 0; y < 450; y++ ) {
				// color[ chip ] += ( wavelengthColorLinear( 380 + y ) * light.PDFScratch[ y ] ) / 450.0f;
				color[ chip ] += 3.5f * glm::clamp( wavelengthColorLinear( 380 + y ) * xRiteReflectances[ chip ][ y ] * light.PDFScratch[ y ], vec3( 0.0f ), vec3( 1.0f ) ) / 450.0f;
			}
		}

		for ( int x = 0; x < 6; x++ ) {
			for ( int y = 0; y < 4; y++ ) {
				int i = x + 6 * y;
				int bx = 455;
				int by = 5;
				int xS = 13;
				int yS = 12;
				int xM = 2;
				int yM = 2;

				for ( int xo = 0; xo < xS; xo++ ) {
					for ( int yo = 0; yo < yS; yo++ ) {
						light.PDFPreview.SetAtXY( bx + ( xS + xM ) * x + xo, by + ( yS + yM ) * y + yo, Image_4U::color( { uint8_t( color[ i ].r * 255 ), uint8_t( color[ i ].g * 255 ), uint8_t( color[ i ].b * 255 ), 255 } ) );
					}
				}
			}
		}

		// debug
		// light.PDFPreview.FlipVertical();
		// light.PDFPreview.Save( "TestPDF.png" );

		string texLabel = "Filtered PDF Preview " + to_string( light.idx );
		if ( textureManager->Get( texLabel ) == std::numeric_limits< GLuint >::max() ) {
			// this is the "not found" condition... create the texture
			// ================================================================================================================
			{	textureOptions_t opts;
				opts.dataType = GL_RGBA8;
				opts.minFilter = GL_LINEAR;
				opts.magFilter = GL_LINEAR;
				opts.width = 450 + 104;
				opts.height = 64;
				opts.textureType = GL_TEXTURE_2D;

			textureManager->Add( texLabel, opts );
			}
		}

		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, textureManager->Get( texLabel ) );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, light.PDFPreview.Width(), light.PDFPreview.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, light.PDFPreview.GetImageDataBasePtr() );
	}

// light interface is not public, managed internally
public:
	void RecompileShaders() {
		// called on init and at runtime if needed
	}

	// engine resource access
	textureManager_t * textureManager;
	std::unordered_map< string, GLuint >* shaders;
	orientTrident* trident;

	void Init ( textureManager_t* textureManager_in, std::unordered_map< string, GLuint >* shaders_in, orientTrident* trident_in ) {
		// cache resource handles
		textureManager = textureManager_in;
		shaders = shaders_in;
		trident = trident_in;

	// intialize...
		// load all data resources
		LoadGelFilterData();
		LoadPDFData();

		// precompute reflectance curves
		PrecomputesRGBReflectances();

		// compile shaders
		// create textures


		// simple testing
		AddLight();
		ComputeLightStack( 0 );

		// create SSBOs

	}

	void Update ( /* some kind of parameter to scale workload */ ) {
		// update the forward light transport simulation
	}

	void Draw () {
		// draw the image into the accumulator
	}

	void ImGuiMenu () {
		ImGui::Begin( "GelFilter" );

		// we need to eventually show the importance sampling structure up top
		for ( int l = 0; l < lightList.size(); l++ ) {
			// disambiguating label hashes
			ImGui::PushID( l );

			bool needsUpdate = false;

			// for now we're going to look at just the light config list (one light):
			// curve texture preview
			ImGui::Text( "" );
			const int w = ImGui::GetContentRegionAvail().x;
			ImGui::Image( ( ImTextureID ) ( void * ) intptr_t( textureManager->Get( "Filtered PDF Preview " + to_string( l ) ) ), ImVec2( w, w * ( 64.0f ) / ( 450.0f + 104 ) ) );
			ImGui::Text( "" );
			ImGui::Indent( 2.0f );

			// source distribution picker
			ImGui::Combo( ( string( "Light Type" ) ).c_str(), &lightList[ l ].sourcePDF, sourcePDFLabels, numSourcePDFs );
			needsUpdate |= ImGui::IsItemEdited();

			ImGui::SameLine();
			if ( ImGui::Button( "Randomize" ) ) {
				RandomizeLight( l );
			}

			// for gels
			for ( int i = 0; i < lightList[ l ].gelStack.size(); i++ ) {
				string iString = to_string( i );

				ImGui::Separator();

				// show gel picker
				ImGui::Combo( ( "Gel##" + iString ).c_str(), &lightList[ l ].gelStack[ i ], gelFilterLabels, numGelFilters );
				needsUpdate |= ImGui::IsItemEdited();

				ImGui::SameLine();
				if ( ImGui::Button( ( "Randomize##" + iString  ).c_str() ) ) {
					rngi gelPick = rngi( 0, numGelFilters - 1 );
					lightList[ l ].gelStack[ i ] = gelPick();
					needsUpdate = true;
				}

				ImGui::SameLine();
				if ( ImGui::Button( ( "Remove##" + iString  ).c_str() ) ) {
					lightList[ l ].gelStack.erase( lightList[ l ].gelStack.begin() + i );
					needsUpdate = true;
				}

				// show selected gel preview color
				vec3 col = gelPreviewColors[ lightList[ l ].gelStack[ i ] ];

				if ( ImGui::ColorButton( ( "##ColorSquare" + iString ).c_str(), ImColor( col.r, col.g, col.b ), ImGuiColorEditFlags_NoAlpha, ImVec2(16, 16 ) ) ) {}
				ImGui::SameLine();

				// show selected gel description
				ImGui::TextWrapped( "%s", gelFilterDescriptions[ lightList[ l ].gelStack[ i ] ] );
			}

			// button to add a new gel to the stack
			if ( ImGui::Button( "Add Gel" ) ) {
				lightList[ l ].gelStack.emplace_back();
				needsUpdate = true;
			}

			if ( needsUpdate ) {
				ComputeLightStack( l );
			}

			// unapply "i" pushID
			ImGui::PopID();
			ImGui::Unindent( 2.0f );
			if ( l != lightList.size() - 1 ) {
				ImGui::Separator();
			}
		}

		if ( ImGui::Button( "Add Light" ) ) {
			AddLight();
		}

		ImGui::End();
	}
};