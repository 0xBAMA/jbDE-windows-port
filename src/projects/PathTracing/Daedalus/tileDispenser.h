#include "../../../engine/includes.h"

#pragma once
#ifndef TILEDISPENSER_H
#define TILEDISPENSER_H

class tileDispenser {
public:
	// environment
	uint32_t tileSize;
	uint32_t imageWidth;
	uint32_t imageHeight;

	// tiles themselves
	std::vector< ivec2 > tiles;
	uint32_t tileListOffset;
	uint32_t tileListPasses;
	uint32_t tilesBetweenQueries;
	float tileTimeLimitMS;

	// time since the last reset
	std::chrono::time_point< std::chrono::steady_clock > tLastReset;

	tileDispenser() {} // default, unused
	tileDispenser( uint32_t t, uint32_t w, uint32_t h ) :
		tileSize( t ), imageWidth( w ), imageHeight( h ) {
		tilesBetweenQueries = 5;
		tileTimeLimitMS = 15.0f;
		RegenerateTileList();
	}

	// regenerate list
	void RegenerateTileList() {
		tiles.resize( 0 );
		tileListOffset = 0;
		tileListPasses = 0;
		for ( uint32_t x = 0; x < imageWidth; x += tileSize ) {
			for ( uint32_t y = 0; y < imageHeight; y += tileSize ) {
				tiles.push_back( ivec2( x, y ) );
			}
		}
		// shuffle the generated array
		std::random_device rd; std::mt19937 rngen( rd() );
		std::shuffle( tiles.begin(), tiles.end(), rngen );
		tLastReset = std::chrono::steady_clock::now();
	}

	// reset state
	void Reset( uint32_t t, uint32_t w, uint32_t h ) {
		if ( t < 16 || t > 2048 ) return; // reject invalid tilesizes
		tileSize = t;
		imageWidth = w;
		imageHeight = h;
		RegenerateTileList();
	}

	// how many tiles in the list
	uint32_t Count() {
		return tiles.size();
	}

	// how many samples have run
	uint32_t SampleCount() {
		return tileListPasses;
	}

	// how long has this been running since the last time we reset the thing
	float SecondsSinceLastReset() {
		return std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - tLastReset ).count() / 1000.0f;
	}

	// get a tile from the list
	ivec2 GetTile() {
		// catch the case where we are completed a fullscreen pass
		if ( ++tileListOffset == tiles.size() ) {
			tileListOffset = 0;
			tileListPasses++;
		}
		return tiles[ tileListOffset ];
	}
};

#endif