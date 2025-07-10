#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;
uniform ivec2 cursor;
uniform vec4 colors[ 10 ];

#include "glyphs.h"
#include "mathUtils.h"

#define STATEMASK 0x3u
#define SOLVEMASK 0xF00u
#define NOTESMASK 0x3F000u

// each uint has 32 bits... I think I can do it with 16
// 0000 0000 00 / 0000 / 00
// notes flags / solve / state
// which are:
    // notes flags: 10 bit flags indicating the current state of the notes for unsolved cells (really only need 9 bits)
    // solve: the solved value. Ignored for unsolved cells. Need 1-9, 9 values. 4 bits
    // state: notes, solved, incorrect. we need to encode 3 states, 2 bits. Determines what data is referenced when drawing the cell, notes or solve

uniform int state[ 81 ];
uniform int notes[ 81 ];
ivec2 getBoardDataEncoded( ivec2 p ) {
    return ivec2( state[ p.x + 9 * p.y ], notes[ p.x + 9 * p.y ] );
}

vec3 getCellPixel( ivec2 cell, vec2 uv ) {
    vec3 color = vec3( 0.0f );

    // solved, draw the corresponding pixel for a solved int
    // incorrect, show the same as solved, but with a red radial gradient behind the glyph to signal it is incorrect
    // notes, subdivide and draw the glyphs indicating the notes flags

    // accessing the board data now
    ivec2 boardData = getBoardDataEncoded( cell );
    if ( boardData.x == 0 ) { // show the notes

        vec2 notespacePoint = 3.0f * uv;
        vec2 uv = fract( notespacePoint );
        uvec2 noteIndex = uvec2( floor( notespacePoint ) );
        uint myValue = noteIndex.x + 3 * noteIndex.y + 1;

        bool glyphMask = ( fontRef( myValue + 48, ivec2( 8 * uv.x, 16 * uv.y ) ) != 0 );

        if ( ( ( 1 << ( myValue ) ) & ( boardData.y ) ) != 0 ) {
            // highlight color, notes true
            if ( glyphMask ) {
                color = colors[ myValue - 1 ].rgb;
            }
        } else {
            // dim gray, notes false
            if ( glyphMask ) {
                color = vec3( 0.01618f );
            }
        }

    } else { // show the solved or incorrect cell

        // get the glyph for the cell
        bool glyphMask = !( fontRef( abs( boardData.x ) + 48, ivec2( 8 * uv.x - 0.5, 16 * uv.y ) ) == 0 );
        if ( ( 8 * uv.x - 0.5f ) < 0 ) {
            glyphMask = false;
        }

        if ( boardData.x < 0 ) { // you blew it, pal... incorrect
            color = vec3( 0.1618f, 0.0f, 0.0f );
        }

        // overwrite with the glyph
        if ( glyphMask ) {
            color = colors[ abs( boardData.x ) - 1 ].rgb;
        }

    }

    return color;
}

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
    vec3 col = vec3( 0.0f );

    // centered uv
    vec2 uv = ( vec2( writeLoc ) + vec2( 0.5f ) ) / imageSize( accumulatorTexture );
    uv = uv - vec2( 0.5f );
    uv.x *= ( imageSize( accumulatorTexture ).x / float( imageSize( accumulatorTexture ).y ) );
    uv *= 10.0f;

// solve for which cell we are in
    if ( abs( uv.x ) < 4.5f && abs( uv.y ) < 4.5f ) {
        col = vec3( 0.1618f );
    }

    // I'd like a little window on the side to show the current input mode and maybe a selected number, we're going to have to see how the interface develops
    if ( uv.x < 6.0f && uv.x > 5.0f && uv.y < 3.5f && uv.y > 1.0f ) {
        // we'll be showing modal data here
        col = getCellPixel( cursor, vec2( uv.x - 5.0f, RangeRemapValue( uv.y, 1.0f, 3.5f, 0.0f, 1.0f ) ) );
    }

    vec2 gridspacePoint = uv + vec2( 4.5f );
    ivec2 gridIndex = ivec2( floor( gridspacePoint ) );

    // if it's on the board, show the notes
    if ( all( lessThan( gridIndex, ivec2( 9 ) ) ) && all( greaterThanEqual( gridIndex, ivec2( 0 ) ) ) ) {
        col = getCellPixel( gridIndex, fract( gridspacePoint ) );
    }

    // higlight at the cursor
    if ( gridIndex.x == cursor.x && gridIndex.y == cursor.y  ) {
        col += 0.1618f;
    }

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}
