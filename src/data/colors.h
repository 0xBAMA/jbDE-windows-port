
#pragma once
#ifndef COLORS_H
#define COLORS_H

#include <string>
#include <algorithm>
#include "./paletteLoader.h" // for the palettes from lospec

//***********************************************************
//
//  File:     colors.h
//
//  Author:   Matthew Beldyk
//  Email:    mb245002@ohiou.edu
//
//  Usage:    I created this file to house some named string
//            constants with escape codes for colors in them
//            this makes it much easier for me to do colors.
//            I can still use the codes if I want, but this
//            works too.  try the statement:
//            cout<<BLUE<<"I like cookies"<<endl;
//
//		  You may use this whereever you want, but if you
//		  make any large improvements or whatever, I am
//		  curious, so email 'em my way, please.
//
//***********************************************************
//
//  all credit given to Matthew Beldyk for writing this file
//  he gave me permission to try out in my programs
//  just wanted to use to make everything look nice
//
//***********************************************************
// some windows specific defines to set this, probably - msvc complains about unrecognized escape sequences
//#define USE_COMMAND_LINE_COLOR_SEQUENCES

// these work for linux
#ifdef USE_COMMAND_LINE_COLOR_SEQUENCES
	const std::string BLINK		= "\e[5m";
	const std::string BOLD		= "\e[1m";
	const std::string RESET		= "\e[0m";
	const std::string ERROR		= "\e[1;41;37m\a";
	const std::string MENU		= "\e[44;37m";

	// text colors
	const std::string T_BLACK	= "\e[30m";
	const std::string T_RED		= "\e[31m";
	const std::string T_GREEN	= "\e[32m";
	const std::string T_YELLOW	= "\e[33m";
	const std::string T_BLUE	= "\e[34m";
	const std::string T_MAGENTA	= "\e[35m";
	const std::string T_CYAN	= "\e[36m";
	const std::string T_WHITE	= "\e[37m";

	// background colors
	const std::string B_BLACK	= "\e[40m";
	const std::string B_RED		= "\e[41m";
	const std::string B_GREEN	= "\e[42m";
	const std::string B_YELLOW	= "\e[43m";
	const std::string B_BLUE	= "\e[44m";
	const std::string B_MAGENTA	= "\e[45m";
	const std::string B_CYAN	= "\e[46m";
	const std::string B_WHITE	= "\e[47m";
#else
	// windows doesn't recognize any of these - skip it
	const std::string BLINK		= "";
	const std::string BOLD		= "";
	const std::string RESET		= "";
	const std::string ERROR		= "";
	const std::string MENU		= "";
	const std::string T_BLACK	= "";
	const std::string T_RED		= "";
	const std::string T_GREEN	= "";
	const std::string T_YELLOW	= "";
	const std::string T_BLUE	= "";
	const std::string T_MAGENTA	= "";
	const std::string T_CYAN	= "";
	const std::string T_WHITE	= "";
	const std::string B_BLACK	= "";
	const std::string B_RED		= "";
	const std::string B_GREEN	= "";
	const std::string B_YELLOW	= "";
	const std::string B_BLUE	= "";
	const std::string B_MAGENTA	= "";
	const std::string B_CYAN	= "";
	const std::string B_WHITE	= "";
#endif


// palette stuff after this point


inline vec3 GetColorForTemperature ( float temperature ) {
// from the author:
	// Color temperature (sRGB) stuff
	// Copyright (C) 2014 by Benjamin 'BeRo' Rosseaux
	// Because the german law knows no public domain in the usual sense,
	// this code is licensed under the CC0 license http://creativecommons.org/publicdomain/zero/1.0/

// Valid from 1000 to 40000 K (and additionally 0 for pure full white)

	// Reference Values from:
	// http://blenderartists.org/forum/showthread.php?270332-OSL-Goodness&p=2268693&viewfull=1#post2268693
	mat3 m =
		( temperature <= 6500.0f )
			? mat3( vec3( 0.0f, -2902.1955373783176f, -8257.7997278925690f ),
					vec3( 0.0f, 1669.5803561666639f, 2575.2827530017594f ),
					vec3( 1.0f, 1.3302673723350029f, 1.8993753891711275f ) )
			: mat3( vec3( 1745.0425298314172f, 1216.6168361476490f, -8257.7997278925690f ),
					vec3( -2666.3474220535695f, -2173.1012343082230f, 2575.2827530017594f ),
					vec3( 0.55995389139931482f, 0.70381203140554553f, 1.8993753891711275f ) );

	return glm::mix( glm::clamp( vec3( m[ 0 ] / ( vec3( glm::clamp( temperature, 1000.0f, 40000.0f ) ) +
		m[ 1 ] ) + m[ 2 ] ), vec3( 0.0f ), vec3( 1.0f ) ), vec3( 1.0f ), glm::smoothstep( 1000.0f, 0.0f, temperature ) );
}

// unknown origin, blue to red fade through green
inline vec3 HeatMapColorRamp ( float input ) {
	input *= pi / 2.0f;
	return vec3( sin( input ), sin( input * 2.0f ), cos( input ) );
}

inline vec3 hue ( float input ) {
	return ( vec3( 0.6f ) + vec3( 0.6f ) * cos( vec3( 6.3f ) * vec3( input ) + vec3( 0.0f, 23.0f, 21.0f ) ) );
}

// Jet Colormap
inline vec3 Jet ( float input ) {
	const vec4 kRedVec4 = vec4( 0.13572138f, 4.61539260f, -42.66032258f, 132.13108234f );
	const vec4 kGreenVec4 = vec4( 0.09140261f, 2.19418839f, 4.84296658f, -14.18503333f );
	const vec4 kBlueVec4 = vec4( 0.10667330f, 12.64194608f, -60.58204836f, 110.36276771f );
	const vec2 kRedVec2 = vec2( -152.94239396f, 59.28637943f );
	const vec2 kGreenVec2 = vec2( 4.27729857f, 2.82956604f );
	const vec2 kBlueVec2 = vec2( -89.90310912f, 27.34824973f );
	input = glm::fract( input );
	vec4 v4 = vec4( 1.0f, input, input * input, input * input * input );
	vec2 v2 = glm::vec2( v4[ 2 ], v4[ 3 ] ) * v4.z;
	return vec3(
		glm::dot( v4, kRedVec4 )   + glm::dot( v2, kRedVec2 ),
		glm::dot( v4, kGreenVec4 ) + glm::dot( v2, kGreenVec2 ),
		glm::dot( v4, kBlueVec4 )  + glm::dot( v2, kBlueVec2 )
	);
}

// Zucconi Helper func
inline vec3 bump3y ( vec3 x, vec3 yoffset ) {
	vec3 y = vec3( 1.0f ) - x * x;
	y = clamp( ( y - yoffset ), vec3( 0.0f ), vec3( 1.0f ) );
	return y;
}

inline vec3 SpectralZucconi ( float input ) {
	input = glm::fract( input );
	const vec3 cs = vec3( 3.54541723f, 2.86670055f, 2.29421995f );
	const vec3 xs = vec3( 0.69548916f, 0.49416934f, 0.28269708f );
	const vec3 ys = vec3( 0.02320775f, 0.15936245f, 0.53520021f );
	return bump3y( cs * ( input - xs ), ys );
}

inline vec3 SpectralZucconi6 ( float input ) {
	input = glm::fract( input );
	const vec3 c1 = vec3( 3.54585104f, 2.93225262f, 2.41593945f );
	const vec3 x1 = vec3( 0.69549072f, 0.49228336f, 0.27699880f );
	const vec3 y1 = vec3( 0.02312639f, 0.15225084f, 0.52607955f );
	const vec3 c2 = vec3( 3.90307140f, 3.21182957f, 3.96587128f );
	const vec3 x2 = vec3( 0.11748627f, 0.86755042f, 0.66077860f );
	const vec3 y2 = vec3( 0.84897130f, 0.88445281f, 0.73949448f );
	return bump3y( c1 * ( input - x1 ), y1 ) + bump3y( c2 * ( input - x2 ), y2 );
}

// TODO:
// - take a look at http://devmag.org.za/2012/07/29/how-to-choose-colours-procedurally-algorithms/
// - interpolation of colors in different colorspaces ( separate function )
	// - Global setting for interpolation colorspace in palette::
	// - This is used for both the interpolated palette and the linear gradient
	// - bring over the colorspace header, RGB<->whatever conversion funcs ( will need some edits to make work with C++ )

namespace palette {

	inline std::vector< paletteEntry > paletteListLocal;

	inline void PopulateLocalList ( std::vector< paletteEntry > &engineList ) {
		paletteListLocal.reserve( engineList.size() );
		for ( size_t i = 0; i < engineList.size(); i++ ) {
			paletteListLocal.push_back( engineList[ i ] );
		}
	}

	// I think the best way to do this is to have any state set as a variable in this namespace
		// e.g. which palette index ( or find the palette by string label ), IQ palette control points, simple gradient endpoints

	enum class type {
//==== lospec / matplotlib indexed set ========================================
		// map input 0-1 to nearest color
		paletteIndexed,

		// take the floor of the input, mod size of the palette
		paletteIndexed_modInt,

		// blend nearest two colors
		paletteIndexed_interpolated,


//==== Analytic Palettes ======================================================
	//==== Hue ( kinda HSV type affair ) ======================================
		paletteHue,

	//==== Jet ================================================================
		paletteJet,

	//==== Zucconi Spectral Variants ==========================================
		paletteZucconiSpectral,
		paletteZucconiSpectral6,

	//==== from GetColorForTemperature() above ================================
		paletteTemperature,
		paletteTemperature_normalized,

	//==== from HeatMapColorRamp() above ======================================
		paletteHeatmapRamp,

	//==== Inigo Quilez-style sinusoidal palettes =============================
		paletteIQSinusoid,

	//==== Simple gradient, interpolating between two colors ==================
		paletteSimpleGradient

	};


	inline vec3 IQControlPoints[ 4 ] = { vec3( 0.0f ) };
	inline void SetIQPaletteDefaultSet ( int index ) {
		const vec3 values[ 10 ][ 4 ] = { // to generate more: https://www.shadertoy.com/view/tlSBDw - looks like this set of 7 covers most of the ones of any value
			{ vec3( 0.5f ), vec3( 0.5f ), vec3( 1.0f ), vec3( 0.00f, 0.10f, 0.20f ) },												// steel heat colors
			{ vec3( 0.55f, 0.4f, 0.3f ), vec3( 0.8f ), vec3( 0.29f ), vec3( 0.54f, 0.59f, 0.69f ) },								// phreax cooler
			{ vec3( 0.5f ), vec3( 0.55f ), vec3( 0.45f ), vec3( 0.47f, 0.57f, 0.67f ) },											// phreax warmer
			{ vec3( 0.55f, 0.4f, 0.3f ), vec3( 0.60f, 0.61f, 0.45f ), vec3( 0.8f, 0.75f, 0.8f ), vec3( 0.285f, 0.54f, 0.88f ) },	// phreax rainbow
			{ vec3( 0.5f ), vec3( 0.5f ), vec3( 1.0f ), vec3( 0.00f, 0.33f, 0.67f ) },												// rainbow
			{ vec3( 0.5f ), vec3( 0.5f ), vec3( 1.0f ), vec3( 0.30f, 0.20f, 0.20f ) },												// blue-red
			{ vec3( 0.5f ), vec3( 0.5f ), vec3( 1.0f, 1.0f, 0.5f ), vec3( 0.80f, 0.90f, 0.30f ) },									// yellow-green-blue
			{ vec3( 0.5f ), vec3( 0.5f ), vec3( 1.0f, 0.7f, 0.4f ), vec3( 0.00f, 0.15f, 0.20f ) },									// purple-blue-yellow
			{ vec3( 0.5f ), vec3( 0.5f ), vec3( 2.0f, 1.0f, 0.0f ), vec3( 0.50f, 0.20f, 0.25f ) },									// pink-green-yellow
			{ vec3( 0.8f, 0.5f, 0.4f ), vec3( 0.2f, 0.4f, 0.2f ), vec3( 2.0f, 1.0f, 1.0f ), vec3( 0.00f, 0.25f, 0.25f ) }			// watermelon
		};
		index = std::clamp( index, 0, 9 );
		for ( uint8_t pt{ 0 }; pt < 4; pt++ ) {
			IQControlPoints[ pt ] = values[ index ][ pt ];
		}
	}

	inline vec3 iqPaletteRef ( float t, vec3 a, vec3 b, vec3 c, vec3 d ) {
		return a + b * cos( 6.28318f * ( c * t + d ) );
	}

// managing lospec / matplotlib palettes
	inline int PaletteIndex = 0;
	inline void PickPaletteByLabel ( string label ) {
		PaletteIndex = 0; // default, if not found
		for ( unsigned int i = 0; i < paletteListLocal.size(); i++ ) {
			if ( paletteListLocal[ i ].label == label ) {
				PaletteIndex = i;
			}
		}
	}

	inline void PickRandomPalette ( bool reportName = false ) {
		rngi pick( int( 0 ), int( paletteListLocal.size() - 1 ) );
		PaletteIndex = pick();
		if ( reportName ) {
			cout << "picked random palette " << PaletteIndex << ": " << paletteListLocal[ PaletteIndex ].label << newline;
		}
	}

	inline string GetCurrentPaletteName () {
		return paletteListLocal[ PaletteIndex ].label;
	}

	// for linear gradient between two colors
	inline vec3 GradientEndpoints[ 2 ] = { vec3( 0.0f ) };

	inline vec3 paletteRef ( float input, type inputType = type::paletteIndexed_interpolated ) {

		vec3 value = vec3( 0.0f );
		switch ( inputType ) {

			// indexing into the currently selected palette / palette size ( nearest neighbor )
			case type::paletteIndexed:
				value = vec3( paletteListLocal[ PaletteIndex ].colors[ int( input * ( paletteListLocal[ PaletteIndex ].colors.size() ) ) ] ) / 255.0f;
				break;

			// indexing into the currently selected palette, with the floor of the input, mod by the size of the palette
			case type::paletteIndexed_modInt:
				value = vec3( paletteListLocal[ PaletteIndex ].colors[ int( input ) % paletteListLocal[ PaletteIndex ].colors.size() ] ) / 255.0f;
				break;

			// same as paletteIndexed, but interpolate between nearest values ( clamp on the ends )
			case type::paletteIndexed_interpolated:
				{
					const float paletteSize = paletteListLocal[ PaletteIndex ].colors.size();
					float index = float( input * ( paletteSize - 1 ) );

					uint32_t indexLow = std::clamp( uint32_t( index ), 0u, uint32_t( paletteSize - 1 ) );
					uint32_t indexHigh = std::clamp( ( uint32_t( index ) + 1 ) < paletteSize ? indexLow + 1 : 0u, 0u, uint32_t( paletteSize - 1 ) );

					// interpolating in other colorspaces... RGB is not ideal
					value = glm::mix(
						vec3( paletteListLocal[ PaletteIndex ].colors[ indexLow ] ) / 255.0f,
						vec3( paletteListLocal[ PaletteIndex ].colors[ indexHigh ] ) / 255.0f,
						index - float( indexLow )
					);
				}
				break;

			case type::paletteHue:
				value = hue( input );
				break;

			case type::paletteJet:
				value = Jet( input );
				break;

			case type::paletteZucconiSpectral:
				value = SpectralZucconi( input );
				break;

			case type::paletteZucconiSpectral6:
				value = SpectralZucconi6( input );
				break;

			// use input value raw
			case type::paletteTemperature:
				value = GetColorForTemperature( input );
				break;

			// remap 0-1 to 1k-40k
			case type::paletteTemperature_normalized:
				value = GetColorForTemperature( input * 40000.0f + 1000.0f );
				break;

			// reference to the sinusoidal
			case type::paletteHeatmapRamp:
				value = HeatMapColorRamp( input );
				break;

			case type::paletteIQSinusoid: // using currently set control points, IQControlPoints[]
				value = iqPaletteRef( input, IQControlPoints[ 0 ], IQControlPoints[ 1 ], IQControlPoints[ 2 ], IQControlPoints[ 3 ] );
				break;

			case type::paletteSimpleGradient:
				value = glm::mix( GradientEndpoints[ 0 ], GradientEndpoints[ 1 ], input );
				break;

			default: // shouldn't be able to hit this
				break;

		}
	
		return glm::clamp( value, vec3( 0.0f ), vec3( 1.0f ) );
	}
};

#endif //COLORS_H
