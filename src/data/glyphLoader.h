#pragma once
#ifndef GLYPH_H
#define GLYPH_H

#include "../engine/coreUtils/image2.h"

struct glyph {
	int index = 0;
	std::vector< std::vector< uint8_t > > glyphData;

	void print () {
		for ( uint32_t y = 0; y < glyphData.size(); y++ ) {
			cout << "\n\t";
			for ( uint32_t x = 0; x < glyphData[ 0 ].size(); x++ ) {
				cout << ( int ) glyphData[ y ][ x ];
			}
			cout << "\n";
		}
	}
};

const color_4U clear ( {   0,   0,   0,   0 } );
const color_4U black ( {  69,  69,  69, 255 } );
const color_4U white ( { 205, 205, 205, 255 } );
static bool isBlackOrWhite( color_4U check ) {
	return check == black || check == white;
}

static void ReadGlyphAt ( uint32_t x, uint32_t y, Image_4U &buffer, std::vector< glyph > &glyphList ) {
	// find the footprint of the glyph
	glyph g;

	// because of how we iterate across the image we know that x,y is the top left corner
	uint32_t xcur = x, ycur = y;

	// determine x extent
	while ( 1 ) {
		if ( isBlackOrWhite( buffer.GetAtXY( xcur, y ) ) ) {
			xcur++;
		} else {
			xcur--;
			break;
		}
	}

	// determine y extent
	while ( 1 ) {
		if ( isBlackOrWhite( buffer.GetAtXY( x, ycur ) ) ) {
			ycur++;
		} else {
			ycur--;
			break;
		}
	}

	// size the arrays
	const int xdim = xcur - x + 1;
	const int ydim = ycur - y + 1;
	g.glyphData.resize( ydim );
	for ( unsigned int i = 0; i < g.glyphData.size(); i++ ) { g.glyphData[ i ].resize( xdim ); }

	// read in the footprint of the glyph, store it in the glyph object
	for ( uint32_t yy = y; yy <= ycur; yy++ ) {
		for ( uint32_t xx = x; xx <= xcur; xx++ ) {

			color_4U read = buffer.GetAtXY( xx, yy );
			if ( read == black ) {
				g.glyphData[ yy - y ][ xx - x ] = 0;
			} else if ( read == white ) {
				g.glyphData[ yy - y ][ xx - x ] = 1;
			} // else ... no else - shouldn't hit this

			// zero out the footprint of the glyph, so it is not recounted on subsequent rows
			buffer.SetAtXY( xx, yy, clear );
		}
	}

	// get the index from the colored drop shadow
	color_4U dexx = buffer.GetAtXY( xcur + 1, ycur + 1 );
	g.index = dexx[ blue ] * 255 * 255 + dexx[ green ] * 255 + dexx[ red ];

	glyphList.push_back( g );
}

static void LoadGlyphs ( std::vector< glyph > &glyphList ) {
	Image_4U glyphRecord( "../src/data/bitfontCore2.png" );
	// iterate through all the pixels in the image
	const uint32_t height = glyphRecord.Height();
	const uint32_t width = glyphRecord.Width();
	for ( uint32_t y = 0; y < height; y++ ) {
		for ( uint32_t x = 0; x < width; x++ ) {
			if ( isBlackOrWhite( glyphRecord.GetAtXY( x, y ) ) ) {
				ReadGlyphAt( x, y, glyphRecord, glyphList );
				x += glyphList[ glyphList.size() - 1 ].glyphData[ 0 ].size(); // less of an optimization than hoped, ~1% speedup
			}
		}
	}

	// the above is quite slow - I tried a couple things, and saw about a 40% speedup for
	// writing out a set of offsets for each glyph, parsing that at runtime to accelerate
	// searching - I think that I can do better than that, however - that had the overhead
	// of parsing that stuff encoded as json - and didn't see much speedup by removing the
	// writeback of the clear color. I scrapped it because I wasn't getting everything
	// through faithfully. I think for now, it's fine, but 1.4-2.0 seconds at startup does
	// seem like it could use some attention in the future. We'll see how much it matters.
}

#endif // GLYPH_H