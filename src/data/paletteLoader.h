#pragma once
#ifndef PALETTE_H
#define PALETTE_H

#include <includes.h>

struct paletteEntry {
	string label;
	std::vector< glm::ivec3 > colors;
};

// static void LoadAllHexfiles() { // temporary, but keeping it around for future use
// 	for ( const std::filesystem::directory_entry& file : std::filesystem::recursive_directory_iterator( "hexfiles" ) ) {
// 		palette p;
// 		p.label = file.path().stem().string();
// 		std::ifstream in( file.path() );
// 		uint32_t value;
// 		// read in the value, in hex
// 		while ( in >> std::hex >> value ) { // this is kinda s(h)exy
// 			int r, g, b; // extract the color channel values from the bits
// 			r = ( value & 0xFF0000 ) >> 16;
// 			g = ( value & 0xFF00 ) >> 8;
// 			b = ( value & 0xFF );
// 			p.colors.push_back( glm::ivec3( r, g, b ) );
// 		}
// 		// good to go
// 		paletteList.push_back( p );
// 	}
// }

// static void reexportPalettes () {
// 	// sort by label
// 	std::sort( paletteList.begin(), paletteList.end(), []( paletteEntry a, paletteEntry b ) { return a.label > b.label; } );
// 	Image out( 33 + 256 /* label + max palette entry count */, paletteList.size() );
// 	for ( unsigned int y = 0; y < paletteList.size(); y++ ) {
// 		// write the string for the identifier
// 		paletteList[ y ].label.resize( 32, ' ' );
// 		for ( int l = 0; l < 32; l++ ) {
// 			out.SetAtXY( l, y, { uint8_t( paletteList[ y ].label[ l ] ), 0, 0, 255 } );
// 		}
// 		// // remove duplicates - I didn't want to do this for the matlab palettes, or really at all
// 		// for ( unsigned int entry = 0; entry < paletteList[ y ].colors.size() - 1; entry++ ) {
// 		// 	for ( unsigned int second = entry + 1; second < paletteList[ y ].colors.size(); second++ ) {
// 		// 		if ( paletteList[ y ].colors[ entry ] == paletteList[ y ].colors[ second ] ) {
// 		// 			paletteList[ y ].colors.erase( paletteList[ y ].colors.begin() + second );
// 		// 			second--;
// 		// 		}
// 		// 	}
// 		// }
// 		// write all the palette entry values
// 		for ( unsigned int entry = 0; entry < paletteList[ y ].colors.size(); entry++ ) {
// 			rgba value { uint8_t( paletteList[ y ].colors[ entry ].x ), uint8_t( paletteList[ y ].colors[ entry ].y ), uint8_t( paletteList[ y ].colors[ entry ].z ), 255 };
// 			out.SetAtXY( 33 + entry, y, value );
// 		}
// 	}
// 	out.Save( "test.png" );
// }

static void LoadPalettes ( std::vector< paletteEntry >& paletteList, bool verbose = false ) {
	Image_4U paletteRecord( "../src/data/palettes.png" );
	for ( uint32_t yPos = 0; yPos < paletteRecord.Height(); yPos++ ) {
		paletteEntry p;

		// first 32 pixels' red channels contain the space-padded label
		for ( int x = 0; x < 32; x++ ) { p.label += char( paletteRecord.GetAtXY( x, yPos )[ red ] ); }
		p.label.erase( std::remove( p.label.begin(), p.label.end(), ' ' ), p.label.end() );
		if ( verbose ) {
			cout << fixedWidthNumberString( yPos, 5, ' ' ) << ":" << p.label << endl;
		}

		// then the rest of the row, up to the first { 0, 0, 0, 0 } pixel is the palette data
		for ( int x = 33;; x++ ) {
			color_4U read = paletteRecord.GetAtXY( x, yPos );
			if ( read[ alpha ] == 0 ) { break; }
			p.colors.push_back( glm::ivec3( read[ red ], read[ green ], read[ blue ] ) );
		}

		// data is ready to go
		paletteList.push_back( p );
	}

	// LoadAllHexfiles();
	// reexportPalettes();
}


#endif // PALETTE_H