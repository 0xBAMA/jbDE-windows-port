#include "../../engine/includes.h"

class DLAModelCPU {
public:
	std::vector< vec3 > unanchoredParticles;
	std::unordered_map< ivec3, std::vector< vec3 > > anchoredParticles; // on a quantized grid

	float anchorDistance = 0.8f;
	int threadIDX;

	void Respawn ( vec3 &particle ) {
		static rng placement( -100.0f, 100.0f );
		particle.x = placement();
		particle.y = placement();
		particle.z = placement();
	}

	void Update () {
		rng jitter( -5.0f, 5.0f );
		for ( auto& particle : unanchoredParticles ) {
			// jitter the particle in a random direction
			particle.x += jitter();
			particle.y += jitter();
			particle.z += jitter();

			// "wind"
			particle.y += 1.0f;

			// look at the nearby cells to see if there's anyone you might bond to
			ivec3 quantizedPosition = ivec3( particle );

			float closestDistance = 1000.0f;
			vec3 closestParticle = vec3( 0.0f );

			for ( int x = -1; x <= 1; x++ ) {
				for ( int y = -1; y <= 1; y++ ) {
					for ( int z = -1; z <= 1; z++ ) {
						for ( auto& otherParticle : anchoredParticles[ quantizedPosition + ivec3( x, y, z ) ] ) {
							float dCantidate = distance( particle, otherParticle );
							if ( dCantidate < closestDistance ) {
								closestDistance = dCantidate;
								closestParticle = otherParticle;
							}
						}
					}
				}
			}

			// if we found an anchored particle in bonding distance
			if ( closestDistance < anchorDistance ) {
				anchoredParticles[ quantizedPosition ].push_back( particle );
				Respawn( particle );
			}

			// bounds check and respawn if outside of reasonable volume
			if ( abs( particle.x ) > 150.0f || abs( particle.y ) > 150.0f || abs( particle.y ) > 150.0f ) {
				Respawn( particle );
			}
		}
	}

	void Init () {
		// initialize the list of unanchored particles
		unanchoredParticles.resize( 50000 );
		for ( auto& particle : unanchoredParticles ) {
			Respawn( particle );
		}

		// initial anchored particles
		rng loc( 0.0f, tau );
		rng jitter( -8, 8 );
		for ( int i = 0; i < 20000; i++ ) {
			float locS = loc();
			vec3 point = vec3( 10.0f * cos( locS ) + jitter() * loc(), 10.0f * sin( locS ) + jitter() * loc(), loc() + jitter() );
			anchoredParticles[ ivec3( point ) ].push_back( point );
		}

		// prune to a maximum of a single particle per cell
		for ( auto& [p, m] : anchoredParticles ) {
			if ( m.size() != 0 ) {
				m.resize( 1 );
			}
		}

		rng anchorD = rng( 1.0f, 1.9f );
		anchorDistance = anchorD();
	}

	int i = 0;
	void RunBatch ( int iterations ) {

		// run it for some number of iterations
		ivec3 lastMinTouched = ivec3( 0 );
		ivec3 lastMaxTouched = ivec3( 0 );
		int lastMaxCellCount = 0;
		int n = 0;
		for ( i = 0; i < iterations; i++ ) {

			// running the sim...
			Update();

			if ( i % 10 == 0 ) {
				int totalAnchored = 0;
				int uniqueCells = 0;
				int maxCellCount = 0;
				ivec3 minTouched = ivec3( 0 );
				ivec3 maxTouched = ivec3( 0 );
				for ( auto& [p, m] : anchoredParticles ) {
					if ( m.size() != 0 ) {
						totalAnchored += m.size();
						maxCellCount = max( int( m.size() ), maxCellCount );
						uniqueCells++;
						minTouched = min( p, minTouched );
						maxTouched = max( p, maxTouched );
					}
				}
				cout << 100.0f * float( i ) / iterations << "%... Anchored: " << totalAnchored << " Unique: " << uniqueCells << endl;
				cout << "Min " << minTouched.x << " " << minTouched.y << " " << minTouched.z << endl;
				cout << "Max " << maxTouched.x << " " << maxTouched.y << " " << maxTouched.z << endl;
				lastMinTouched = minTouched;
				lastMaxTouched = maxTouched;
				lastMaxCellCount = maxCellCount;
			}


			if ( i % 500 == 0 ) {
				// dump 256^3 block model for Voraldo
				Image_4F blockSave( 256, 256 * 256 );
				ivec3 centerPoint = ivec3( ( lastMinTouched + lastMaxTouched ) / 2 );

				for ( auto& [p, m] : anchoredParticles ) {
					if ( m.size() != 0 ) {
						ivec3 pA = p - centerPoint + ivec3( 127 );
						blockSave.SetAtXY( pA.x, pA.y + pA.z * 256, color_4F( { 0.618f, 0.618f, 0.618f, std::pow( float( m.size() ) / lastMaxCellCount, 0.3f ) } ) );
					}
				}

				blockSave.Save( "Run" + to_string( threadIDX ) + "_DLAStage" + to_string( n++ ) + "_" + to_string( anchorDistance ) + ".png" );
			}
		}

		/*
		// dump 256^3 block model for Voraldo
		Image_4F blockSave ( 256, 256 * 256 );
		ivec3 centerPoint = ivec3( ( lastMinTouched + lastMaxTouched ) / 2 );

		for ( auto& [ p, m ] : anchoredParticles ) {
			if ( m.size() != 0 ) {
				ivec3 pA = p - centerPoint + ivec3( 127 );
				blockSave.SetAtXY( pA.x, pA.y + pA.z * 256, color_4F( { 1.0f, 1.0f, 1.0f, 2.0f * float( m.size() ) / lastMaxCellCount } ) );
			}
		}

		blockSave.Save( "Test.png" );
		*/
	}

	~DLAModelCPU() {};
	DLAModelCPU () {};
};

class DLAModelGPU {
public:
	GLuint particleBuffer;
	GLuint DLASimShader;
	GLuint DLADrawShader;

	textureManager_t * textureManager;
	int32_t numParticles = ( 1 << 22 );

	int32_t blockDim = 1000;

	void ReloadShaders () {
		DLADrawShader = computeShader( "../src/projects/DLA/shaders/draw.cs.glsl" ).shaderHandle;
		DLASimShader = computeShader( "../src/projects/DLA/shaders/dla.cs.glsl" ).shaderHandle;
	}

	void ResetParticles () {
		rngi texelPick( 0, blockDim - 1 );
		vector< vec4 > particlePositions;
		particlePositions.reserve( numParticles );
		for ( int i = 0; i < numParticles; i++ ) {
			particlePositions.push_back( vec4( texelPick(), texelPick(), texelPick(), texelPick() ) );
		}
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, particleBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * numParticles, ( GLvoid* ) &particlePositions.data()[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, particleBuffer );
	}

	void ResetField () {
		// 3D uint texture for DLA deposition
		Image_4U DLABuffer( blockDim, blockDim * blockDim );
		rng pick( 0, tau );
		for ( int i = 0; i < 500; i++ ) {
			// make sure that there's at least a couple seed texels
			// DLABuffer.SetAtXY( texelPick(), texelPick() + texelPick() * blockDim, color_4U( { 1, 0, 0, 0 } ) );


			/*
			// ivec3 loc = ivec3( texelPick(), texelPick(), texelPick() );
			ivec3 loc = ivec3( blockDim ) / 8;
			rngi jitter( -8, 8 );
			for ( int j = 0; j < 64; j++ ) {
				DLABuffer.SetAtXY( loc.x + jitter(), loc.y + jitter() + ( loc.z + jitter() ) * blockDim, color_4U( { 1, 0, 0, 0 } ) );
			}
			*/


			float value = pick();
			ivec3 loc = ivec3(
				( blockDim / 12.0f ) * sin( value ) + blockDim / 8,
				( blockDim / 12.0f ) * cos( value ) + blockDim / 8,
				int( pick() ) + blockDim / 8
			);
			cout << "writing at " << loc.x << " " << loc.y << " " << loc.z << endl;
			DLABuffer.SetAtXY( loc.x, loc.y + loc.z * blockDim, color_4U( { 1, 0, 0, 0 } ) );
		}

		{
			textureManager->Remove( "DLA Texture" );
			textureOptions_t opts;
			opts.width = blockDim;
			opts.height = blockDim;
			opts.depth = blockDim;
			opts.textureType = GL_TEXTURE_3D;
			opts.dataType = GL_R32UI;
			opts.minFilter = GL_NEAREST;
			opts.magFilter = GL_NEAREST;
			opts.wrap = GL_CLAMP_TO_BORDER;
			opts.initialData = ( void* ) DLABuffer.GetImageDataBasePtr();
			textureManager->Add( "DLA Texture", opts );
		}
	}

	void Init () {

		rngi texelPick( 0, blockDim - 1 );

		// buffer for particle locations
		glGenBuffers( 1, &particleBuffer );
		ResetParticles();

		// clear the buffer
		ResetField();

		// shader for updating the DLA sim
		ReloadShaders();
	}

	void Run ( int iterations ) {

	// run N iterations on the existing model (will be able to repeat this)
		glUseProgram( DLASimShader );

		// bind particle buffer
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, particleBuffer );

		// bind 3D texture
		textureManager->BindImageForShader( "DLA Texture", "DLATexture", DLASimShader, 0 );

		// seeding the RNG
		static rngi rngSeeder( 0, 10000000 );
		glUniform1i( glGetUniformLocation( DLASimShader, "randomSeed" ), rngSeeder() );

		// run the shader
		const uint32_t workgroupsRoundedUp = ( numParticles + 63 ) / 64;
		glDispatchCompute( 64, workgroupsRoundedUp / 64, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

	}

	// something to visualize realtime update?
};