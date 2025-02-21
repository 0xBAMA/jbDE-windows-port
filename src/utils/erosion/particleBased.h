#pragma once
#ifndef PARTICLE_EROSION
#define PARTICLE_EROSION

#include "../../engine/includes.h"

class particleEroder {
public:
	particleEroder () {}

	// functions
	void InitWithDiamondSquare () {
		long unsigned int seed = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine engine{ seed };
		std::uniform_real_distribution< float > distribution{ 0.0f, 1.0f };

		// todo: make this variable ( data array cannot be variable size in c++ )
		const uint32_t dim = 1024;
		// const uint32_t dim = 4096;

	// #define TILE
	#ifdef TILE
		const auto size = dim;
	#else
		const auto size = dim + 1; // for no_wrap
	#endif

		const auto edge = size - 1;
		// float data[ size ][ size ] = { { 0.0f } };
		std::vector< std::vector< float > > data;
		data.resize( size );
		for ( uint32_t i = 0; i < size; i++ ) {
			data[ i ].resize( size );
		}

		// data[ 0 ][ 0 ] = data[ edge ][ 0 ] = data[ 0 ][ edge ] = data[ edge ][ edge ] = 0.25f;

		rng pos = rng( 0.0f, 1.5f );
		data[ 0 ][ 0 ] = pos();
		data[ edge ][ 0 ] = pos();
		data[ 0 ][ edge ] = pos();
		data[ edge ][ edge ] = pos();

	#ifdef TILE
		heightfield::diamond_square_wrap
	#else
		heightfield::diamond_square_no_wrap
	#endif
		(size,
			// random
			[ &engine, &distribution ]( float range ) {
				return distribution( engine ) * range;
			},
			// variance
			[]( int level ) -> float {
				return std::pow( 0.5f, level );
				// return static_cast<float>( std::numeric_limits<float>::max() / 2 ) * std::pow(0.5f, level);
				// return static_cast<float>(std::numeric_limits<float>::max()/1.6) * std::pow(0.5f, level);
			},
			// at
			[ &data ]( int x, int y ) -> float& {
				return data[ x ][ y ];
			}
		);

		model = Image_1F( size, size );
		for ( uint32_t y = 0; y < size; y++ ) {
			for ( uint32_t x = 0; x < size; x++ ) {
				model.SetAtXY( x, y, color_1F( { data[ x ][ y ] } ) );
			}
		}

		// model.Save( "test.exr", Image_1F::backend::TINYEXR );
	}

	vec3 GetSurfaceNormal ( uint32_t x, uint32_t y ) {
		const float scale = 60.0f;

		const float cache00 = model.GetAtXY( x, y )[ red ];
		const float cachep0 = model.GetAtXY( x + 1, y )[ red ];
		const float cachen0 = model.GetAtXY( x - 1, y )[ red ];
		const float cache0p = model.GetAtXY( x, y + 1 )[ red ];
		const float cache0n = model.GetAtXY( x, y - 1 )[ red ];
		const float cachepp = model.GetAtXY( x + 1, y + 1 )[ red ];
		const float cachepn = model.GetAtXY( x + 1, y - 1 )[ red ];
		const float cachenp = model.GetAtXY( x - 1, y + 1 )[ red ];
		const float cachenn = model.GetAtXY( x - 1, y - 1 )[ red ];

		const float sqrt2 = sqrt( 2.0f );

		vec3 n = vec3( 0.15f ) * normalize(vec3( scale * ( cache00 - cachep0 ), 1.0f, 0.0f ) );  // Positive X
		n += vec3( 0.15f ) * normalize( vec3( scale * ( cachen0 - cache00 ), 1.0f, 0.0f ) );         // Negative X
		n += vec3( 0.15f ) * normalize( vec3( 0.0f, 1.0f, scale * ( cache00 - cache0p ) ) );        // Positive Y
		n += vec3( 0.15f ) * normalize( vec3( 0.0f, 1.0f, scale * ( cache0n - cache00 ) ) );       // Negative Y

		// diagonals
		n += vec3( 0.1f ) * normalize( vec3( scale * ( cache00 - cachepp ) / sqrt2, sqrt2, scale * ( cache00 - cachepp ) / sqrt2 ) );
		n += vec3( 0.1f ) * normalize( vec3( scale * ( cache00 - cachepn ) / sqrt2, sqrt2, scale * ( cache00 - cachepn ) / sqrt2 ) );
		n += vec3( 0.1f ) * normalize( vec3( scale * ( cache00 - cachenp ) / sqrt2, sqrt2, scale * ( cache00 - cachenp ) / sqrt2 ) );
		n += vec3( 0.1f ) * normalize( vec3( scale * ( cache00 - cachenn ) / sqrt2, sqrt2, scale * ( cache00 - cachenn ) / sqrt2 ) );

		return n;
	}

	struct particle {
		glm::vec2 position;
		glm::vec2 speed = glm::vec2( 0.0f );
		float volume = 1.0f;
		float sedimentFraction = 0.0f;
	};

	void Erode ( uint32_t numIterations ) {
		std::default_random_engine gen;

		const uint32_t w = model.Width();
		const uint32_t h = model.Height();

		std::uniform_int_distribution< uint32_t > distX ( 0, w );
		std::uniform_int_distribution< uint32_t > distY ( 0, h );

		// run the simulation for the specified number of steps
		for ( unsigned int i = 0; i < numIterations; i++ ) {

			// cout << "\r" << i << "/" << numIterations << "                     ";
			//spawn a new particle at a random position
			particle p;
			p.position = glm::vec2( distX( gen ), distY( gen ) );

			while ( p.volume > minVolume ) { // while the droplet exists (drop volume > 0)
				const glm::uvec2 initialPosition = p.position; // cache the initial position
				const vec3 normal = GetSurfaceNormal( initialPosition.x, initialPosition.y );

				// newton's second law to calculate acceleration
				p.speed += timeStep * glm::vec2( normal.x, normal.z ) / ( p.volume * density ); // F = MA, A = F/M
				p.position += timeStep * p.speed; // update position based on new speed
				p.speed *= ( 1.0f - timeStep * friction ); // friction factor to attenuate speed

				// // wrap if out of bounds (mod logic)
				// particle_wrap(p);
				// if(glm::any(glm::isnan(p.position)))
				//     break;

				// thought I was clever, just discard if out of bounds
				if ( !glm::all( glm::greaterThanEqual( p.position, glm::vec2( 0.0f ) ) ) ||
					!glm::all( glm::lessThan( p.position, glm::vec2( w, h ) ) ) ) break;

				// sediment capacity
				glm::ivec2 refPoint = glm::ivec2( p.position.x, p.position.y );
				float maxSediment = p.volume * glm::length( p.speed ) * ( model.GetAtXY( initialPosition.x, initialPosition.y )[ red ] - model.GetAtXY( refPoint.x, refPoint.y )[ red ] );
				maxSediment = std::max( maxSediment, 0.0f ); // don't want negative values here
				float sedimentDifference = maxSediment - p.sedimentFraction;

				// update sediment content, deposit on the heightmap
				p.sedimentFraction += timeStep * depositionRate * sedimentDifference;
				float oldValue = model.GetAtXY( std::clamp( initialPosition.x, 0u, w - 1 ), std::clamp( initialPosition.y, 0u, h - 1 ) )[ red ];
				model.SetAtXY( std::clamp( initialPosition.x, 0u, w - 1 ), std::clamp( initialPosition.y, 0u, h - 1 ), color_1F( { oldValue - ( timeStep * p.volume * depositionRate * sedimentDifference ) } ) );

				// evaporate the droplet
				p.volume *= ( 1.0f - timeStep * evaporationRate );
			}
		}
	}

	void Save ( string filename ) {
		model.Save( filename, Image_1F::backend::TINYEXR );
	}

	// simulation field
	Image_1F model;

	// simulation parameters
	float timeStep = 1.2f;
	float density = 1.0f; // to determine intertia
	float evaporationRate = 0.001f;
	float depositionRate = 0.1f;
	float minVolume = 0.01f;
	float friction = 0.05f;
};

#endif // PARTICLE_EROSION