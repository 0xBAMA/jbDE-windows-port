#ifndef SPACESHIP
#define SPACESHIP

#include <algorithm>
#include <random>
#include <unordered_map>
#include "../engine/includes.h"

struct bbox {
	glm::ivec3 mins;
	glm::ivec3 maxs;
};

// custom specialization of std::hash can be injected in namespace std
namespace std { // this is required for the use of std::unordered_map
	template<> struct hash< glm::ivec3 > {
		std::size_t operator()( glm::ivec3 const& s ) const noexcept {
			std::size_t h1 = std::hash< int >{}( s.x );
			std::size_t h2 = std::hash< int >{}( s.y );
			std::size_t h3 = std::hash< int >{}( s.z );
			return h1 ^ ( h2 << 4 ) ^ ( h3 << 8 );
		}
	};
}

class spaceshipGenerator {
public:
	spaceshipGenerator () {}

	// letterSelector l;
	std::unordered_map< glm::ivec3, glm::vec4 > model;

	int paletteIndex = 0;
	rng alphaGen = rng( 0.0f, 1.0f );
	rng paletteGen = rng( 0.0f, 1.0f );

	float spread;
	int minXYScale;
	int maxXYScale;
	int minZScale;
	int maxZScale;

	glm::vec4 getColorFromPalette( float t );
	bbox getModelBBox();
	void stampRandomGlyph( std::vector< glyph >& glyphList );
	void shaveBlock();
	void flipBlock();
	void mirrorBlock();
	void squareBlock();
	void genSpaceship( int numOps, float _spread, int _minXYScale, int _maxXYScale, int _minZScale, int _maxZScale, int _paletteIndex, float _paletteMin, float _paletteMax, float _alphaMin, float _alphaMax, std::vector< glyph > &glyphList );
	void getData( std::vector< uint8_t > &data, int dim );
};

inline glm::vec4 spaceshipGenerator::getColorFromPalette ( float t ) {
	// return a color for the palette
	palette::PaletteIndex = paletteIndex;
	return vec4( vec3( palette::paletteRef( paletteGen() ) ), alphaGen() );
}

inline bbox spaceshipGenerator::getModelBBox () {
	// iterate through the hashmap and get min and max values on each index
	bbox temp;
	temp.mins = glm::ivec3( 0 );
	temp.maxs = glm::ivec3( 0 );
	for( auto& [ p, m ] : model ) {
		temp.maxs.x = std::max( temp.maxs.x, p.x ); temp.mins.x = std::min( temp.mins.x, p.x );
		temp.maxs.y = std::max( temp.maxs.y, p.y ); temp.mins.y = std::min( temp.mins.y, p.y );
		temp.maxs.z = std::max( temp.maxs.z, p.z ); temp.mins.z = std::min( temp.mins.z, p.z );
	}
	return temp;
}

inline void spaceshipGenerator::stampRandomGlyph ( std::vector< glyph >& glyphList ) {
	// put a random glyph into the hashmap
	// pick a random glyph in the list
	rngi pick( 0, glyphList.size() - 1 );
	glyph _glyph = glyphList[ pick() ];

	// stamp parameters:
	// xy scale
	rngi xyScaleG( minXYScale, maxXYScale );
	int xyScale = xyScaleG();

	// z scale
	rngi zScaleG( minZScale, maxZScale );
	int zScale = zScaleG();

	// RGBA value
	rng tG( 0.0f, 1.0f );
	float t = tG();
	glm::vec4 c = getColorFromPalette( t );

	// orientation
	rngi orientG( 0, 2 );
	int orientation = orientG();

	// where to draw? base point will be somewhere in the current bounding box
	bbox b = getModelBBox();
	rng xG( ( b.mins.x + b.maxs.x ) / 2.0f, ( b.maxs.x - b.mins.x ) * spread );
	rng yG( ( b.mins.y + b.maxs.y ) / 2.0f, ( b.maxs.y - b.mins.y ) * spread );
	rng zG( ( b.mins.z + b.maxs.z ) / 2.0f, ( b.maxs.z - b.mins.z ) * spread );
	glm::ivec3 base = glm::ivec3( int( xG() ), int( yG() ), int( zG() ) );

	// stamp it (overwrite any existing contents)
	glm::ivec3 p;
	for ( unsigned int xx = 0; xx < _glyph.glyphData.size(); xx++ ) {
		for ( unsigned int yy = 0; yy < _glyph.glyphData[ 0 ].size(); yy++ ) {
			if ( _glyph.glyphData[ xx ][ yy ] == 1 ) {
				for ( int xs = 0; xs < xyScale; xs++ ) {
					for ( int ys = 0; ys < xyScale; ys++ ) {
						for ( int zs = 0; zs < zScale; zs++ ) {
							switch ( orientation ) {
							// the drawing will start at the specified base point - now centering on glyph center
							case 0:
								p = base + glm::ivec3( xs, ys, zs ) + glm::ivec3( xx * xyScale, yy * xyScale, 0 ) - glm::ivec3( _glyph.glyphData.size() / 2, _glyph.glyphData[ 0 ].size(), 0 );
								break;
							case 1:
								p = base + glm::ivec3( ys, xs, zs ) + glm::ivec3( yy * xyScale, xx * xyScale, 0 ) - glm::ivec3( _glyph.glyphData[ 0 ].size() / 2, _glyph.glyphData.size(), 0 );
								break;
							case 2:
								p = base + glm::ivec3( ys, zs, xs ) + glm::ivec3( yy * xyScale, 0, xx * xyScale ) - glm::ivec3( _glyph.glyphData[ 0 ].size() / 2, 0, _glyph.glyphData.size() );
								break;
							default:
								continue;
							}
							model[ p ] = c;
						}
					}
				}
			}
		}
	}
}

inline void spaceshipGenerator::shaveBlock () {
	// pick an axis - take some data off which is greater than the max minus some random distance
	// remove voxels which have a coordinate on [axis] which is greater than the max minus slice distance
	bbox b = getModelBBox();
	rngi axisPick( 0, 2 );
	const int axis = axisPick();
	int min;
	int max;
	switch ( axis ) {
	case 0: min = b.maxs.x - 5; max = b.maxs.x; break;
	case 1: min = b.maxs.y - 5; max = b.maxs.y; break;
	case 2: min = b.maxs.z - 5; max = b.maxs.z; break;
	}
	rngi amtPick( min, max );
	int amt = amtPick();
	amt = amtPick();
	amt = amtPick();
	std::unordered_map< glm::ivec3, glm::vec4 > newmodel;
	for ( auto& [ p, m ] : model ) {
		glm::ivec3 pn = p;
		switch ( axis ) {
		case 0: if ( pn.x < amt ) newmodel[ pn ] = m; break;
		case 1: if ( pn.y < amt ) newmodel[ pn ] = m; break;
		case 2: if ( pn.z < amt ) newmodel[ pn ] = m; break;
		}
	}
	model.clear();
	model = newmodel;
}

inline void spaceshipGenerator::flipBlock () {
	// pick an axis, subtract the bbox axis max from all points
	squareBlock();
	rngi axisPick( 0, 2 );
	const int axis = axisPick();
	const bbox b = getModelBBox();
	std::unordered_map< glm::ivec3, glm::vec4 > newmodel;
	for( auto& [ p, m ] : model ) {
		glm::ivec3 pn = p, base = b.maxs;
		switch ( axis ) {
		case 0: pn.x = base.x - p.x; break;
		case 1: pn.y = base.y - p.y; break;
		case 2: pn.z = base.z - p.z; break;
		}
		newmodel[ pn ] = m;
	}
	model.clear();
	model = newmodel;
}

inline void spaceshipGenerator::mirrorBlock () {
	// pick an axis, then create a copy of all data, mirrored about 0
	squareBlock();
	rngi axisPick( 0, 2 );
	const int axis = axisPick();
	std::unordered_map< glm::ivec3, glm::vec4 > newmodel;
	for ( auto& [ p, m ] : model ) {
		glm::ivec3 np = p;
		switch ( axis ) {
		case 0: np.x *= -1; break;
		case 1: np.y *= -1; break;
		case 2: np.z *= -1; break;
		}
		newmodel[ p ] = m;
		newmodel[ np ] = m;
	}
	model.clear();
	model = newmodel;
}

inline void spaceshipGenerator::squareBlock () {
	// make sure all the data is in positive indices - all positive offsets from the negative faces
	const bbox b = getModelBBox();
	std::unordered_map< glm::ivec3, glm::vec4 > newmodel;
	for ( auto& [ p, m ] : model ) {
		newmodel[ p - b.mins ] = m;
	}
	model.clear();
	model = newmodel;
}

inline void spaceshipGenerator::genSpaceship ( int numOps, float _spread, int _minXYScale, int _maxXYScale, int _minZScale, int _maxZScale, int _paletteIndex, float _paletteMin, float _paletteMax, float _alphaMin, float _alphaMax, std::vector< glyph > &glyphList ) {
	spread = _spread;
	minXYScale = _minXYScale;
	maxXYScale = _maxXYScale;
	minZScale = _minZScale;
	maxZScale = _maxZScale;
	paletteIndex = _paletteIndex;
	alphaGen = rng( _alphaMin, _alphaMax );
	paletteGen = rng( _paletteMin, _paletteMax );

	// zero out the model, then run some number of operations
	model.clear();
	rngi opPick( 0, 100 );
	for ( uint8_t j = 0; j < 22; j++ ) {
		stampRandomGlyph( glyphList );
	}
	for ( int i = 0; i < numOps; i++ ) {
		switch( opPick() % 5 ) {
		case 0:
		case 1:
			for ( uint8_t j = 0; j < 22; j++ ) {
				stampRandomGlyph( glyphList );
			}
			break;
		case 2: mirrorBlock(); flipBlock(); mirrorBlock(); break;
		case 3: mirrorBlock(); break;
		case 4: shaveBlock(); break;
		default: break;
		}
	}
}

inline void spaceshipGenerator::getData ( std::vector< uint8_t > &data, int dim ) {
	// take the data from the model and put it in the block
	squareBlock();
	bbox b = getModelBBox();
	glm::ivec3 offset = b.maxs / 2;

	std::unordered_map< glm::ivec3, glm::vec4 > outputModel;
	for ( auto& [ p, m ] : model ) {
		glm::ivec3 check = p - offset + glm::ivec3( dim / 2 );
		if ( check.x >= 0 && check.x < dim && check.y >= 0 && check.y < dim && check.z >= 0 && check.z < dim ) {
			outputModel[ check ] = m;
		}
	}

	for ( auto& [ p, m ] : outputModel ) {
		unsigned int index = 4 * ( p.x + p.y * dim + p.z * dim * dim );
		if ( index > data.size() ) continue;
		data[ index + 0 ] = m.x * 255;
		data[ index + 1 ] = m.y * 255;
		data[ index + 2 ] = m.z * 255;
		data[ index + 3 ] = m.w * 255;
	}
}

#endif
