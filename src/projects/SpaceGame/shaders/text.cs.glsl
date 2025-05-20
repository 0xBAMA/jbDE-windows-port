#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
// layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 1, rgba8ui ) uniform uimage2D writeTarget;
layout( binding = 2, rgba8ui ) readonly uniform uimage2D fontAtlas;

// where to start drawing from (pixel location of bottom left corner of first glyph)
uniform ivec2 basePointOffset;

// can draw up to 128 chars, on a single line - probably makes sense to do colors
uniform uint numChars;
uniform uint text[ 128 ];

ivec2 getCurrentGlyphBase( int index ) {
	// 16x16 array of glyphs, each of which is 3x7 pixels
	ivec2 location;
	location.x = 3 * ( index % 16 );
	location.y = 105 - 7 * ( index / 16 );
	return location;
}

void main () {
	ivec2 writeLocation = ivec2( gl_GlobalInvocationID.xy );
	ivec2 adjusted = writeLocation - basePointOffset;

// glyphs are 3x7... pad by one pixel on x, for spacing
	// which glyph ID/character color to pull dataTexture
	ivec2 bin = ivec2( adjusted.x / 4, adjusted.y / 7 );
	// where to reference the fontAtlas' glyph ( uv ), for the given character ID
	ivec2 loc = ivec2( adjusted.x % 4, adjusted.y % 7 + 1 ); // +1 to fix off-by-one error

	if ( bin.x >= 0 && bin.x < numChars && bin.y == 0 && loc.x < 3 && loc.x >= 0 && loc.y < 7 && loc.y >=0 ) {
		// where to read from on the font atlas, based on the specified char
		ivec2 atlasReadLocation = getCurrentGlyphBase( int( text[ bin.x ] ) ) + loc;

		// sample the atlas texture to get the sample on the glyph for this pixel
		uvec4 color = imageLoad( fontAtlas, ivec2( mod( atlasReadLocation, ivec2( imageSize( fontAtlas ).xy ) ) ) );

		// if nonzero alpha, write to the write target
		if ( color.r != 0 ) {
			imageStore( writeTarget, writeLocation, uvec4( 178, 69, 14, 255 ) );
		}
	}
}