#include "voraldo.h"

//==============================================================================
void Voraldo13::CompileShaders () {
	const string base( "../src/projects/Voraldo13/shaders/" );

	// do we need to recompile everything, every time? it takes a nontrivial amount of time, for this many
	shaders[ "Draw" ] = computeShader( base + "draw.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Draw" ], -1, string( "Draw" ).c_str() );

	// shape operations
	shaders[ "AABB" ] = computeShader( base + "operations/AABB.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "AABB" ], -1, string( "AABB" ).c_str() );

	shaders[ "Cylinder" ] = computeShader( base + "operations/cylinder.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Cylinder" ], -1, string( "Cylinder" ).c_str() );

	shaders[ "Data Mask" ] = computeShader( base + "operations/dataMask.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Data Mask" ], -1, string( "Data Mask" ).c_str() );

	shaders[ "Ellipsoid" ] = computeShader( base + "operations/ellipsoid.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Ellipsoid" ], -1, string( "Ellipsoid" ).c_str() );

	shaders[ "Heightmap" ] = computeShader( base + "operations/heightmap.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Heightmap" ], -1, string( "Heightmap" ).c_str() );

	shaders[ "Grid" ] = computeShader( base + "operations/grid.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Grid" ], -1, string( "Grid" ).c_str() );

	shaders[ "Sphere" ] = computeShader( base + "operations/sphere.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Sphere" ], -1, string( "Sphere" ).c_str() );

	shaders[ "Triangle" ] = computeShader( base + "operations/triangle.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Triangle" ], -1, string( "Triangle" ).c_str() );

	shaders[ "XOR" ] = computeShader( base + "operations/xor.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "XOR" ], -1, string( "XOR" ).c_str() );

	// utility operations
	shaders[ "Box Blur" ] = computeShader( base + "operations/boxBlur.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Box Blur" ], -1, string( "Box Blur" ).c_str() );

	shaders[ "Gaussian Blur" ] = computeShader( base + "operations/gaussBlur.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Gaussian Blur" ], -1, string( "Gaussian Blur" ).c_str() );

	shaders[ "Clear" ] = computeShader( base + "operations/clear.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Clear" ], -1, string( "Clear" ).c_str() );

	shaders[ "Load" ] = computeShader( base + "operations/load.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Load" ], -1, string( "Load" ).c_str() );

	shaders[ "Mask Invert" ] = computeShader( base + "operations/maskInvert.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Mask Invert" ], -1, string( "Mask Invert" ).c_str() );

	shaders[ "Mask Clear" ] = computeShader( base + "operations/maskClear.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Mask Clear" ], -1, string( "Mask Clear" ).c_str() );

	shaders[ "Shift" ] = computeShader( base + "operations/shift.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Shift" ], -1, string( "Shift" ).c_str() );

	// lighting operations
	shaders[ "Light Clear" ] = computeShader( base + "lighting/clear.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Light Clear" ], -1, string( "Light Clear" ).c_str() );

	shaders[ "Light Mash" ] = computeShader( base + "lighting/mash.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Light Mash" ], -1, string( "Light Mash" ).c_str() );

	shaders[ "Fake GI" ] = computeShader( base + "lighting/fakeGI.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Fake GI" ], -1, string( "Fake GI" ).c_str() );

	shaders[ "Directional Light" ] = computeShader( base + "lighting/directional.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Directional Light" ], -1, string( "Directional Light" ).c_str() );

	shaders[ "Point Light" ] = computeShader( base + "lighting/point.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Point Light" ], -1, string( "Point Light" ).c_str() );

	shaders[ "Cone Light" ] = computeShader( base + "lighting/cone.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Cone Light" ], -1, string( "Cone Light" ).c_str() );

	shaders[ "Ambient Occlusion" ] = computeShader( base + "lighting/ambientOcclusion.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Ambient Occlusion" ], -1, string( "Ambient Occlusion" ).c_str() );

	// color adjustments - uses custom tonemap shader
	shaders[ "Tonemap" ] = computeShader( base + "tonemap.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Tonemap" ], -1, string( "Tonemap" ).c_str() );

	shaders[ "Dither Quantize" ] = computeShader( base + "ditherQuantize.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Dither Quantize" ], -1, string( "Dither Quantize" ).c_str() );

	shaders[ "Dither Palette" ] = computeShader( base + "ditherPalette.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Dither Palette" ], -1, string( "Dither Palette" ).c_str() );

	// shaders[ "Dither Palette LUT Precompute" ] = computeShader( base + "ditherLUTPrecompute.cs.glsl" ).shaderHandle;
	// glObjectLabel( GL_PROGRAM, shaders[ "Dither Palette LUT Precompute" ], -1, string( "Dither Palette LUT Precompute" ).c_str() );

	// renderers
	shaders[ "Image3D Raymarch" ] = computeShader( base + "renderers/raymarch.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Image3D Raymarch" ], -1, string( "Image3D Raymarch" ).c_str() );

	shaders[ "Sampler Raymarch" ] = computeShader( base + "renderers/raymarchSampler.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, shaders[ "Sampler Raymarch" ], -1, string( "Sampler Raymarch" ).c_str() );

	shaders[ "Renderer" ] = shaders[ "Image3D Raymarch" ]; // default renderer
}

//==============================================================================
void Voraldo13::CreateTextures () {
	static float lastSSFactor = 0.0f;
	if ( SSFactor != lastSSFactor ) {
		// delete the accumulator texture - this will hook the first time, and again any time it changes
		textureManager.Remove( "Accumulator" );

		textureOptions_t opts;
		opts.width = config.width * SSFactor;
		opts.height = config.height * SSFactor;
		opts.dataType = GL_RGBA16F;
		opts.minFilter = GL_LINEAR;
		opts.magFilter = GL_LINEAR;
		opts.textureType = GL_TEXTURE_2D;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = nullptr;
		textureManager.Add( "Accumulator", opts );

		// and update the last seen value
		lastSSFactor = SSFactor;
	}

	static uvec3 lastBlockDim = uvec3( 0u );
	if ( blockDim != lastBlockDim ) {

		if ( lastBlockDim != uvec3( 0u ) ) {
			// delete all textures, because we will have previously created them
			textureManager.Remove( "Color Block 0" );
			textureManager.Remove( "Color Block 1" );
			textureManager.Remove( "Mask Block 1" );
			textureManager.Remove( "Mask Block 1" );
			textureManager.Remove( "Lighting Block" );
			textureManager.Remove( "LoadBuffer" );
			textureManager.Remove( "Heightmap" );
		}

	// same as above, but we're creating several 3D textures...
		textureOptions_t opts;

		// initial XOR data for color block
		size_t numBytesBlock = blockDim.x * blockDim.y * blockDim.z * 4;
		std::vector< uint8_t > zeroes;
		zeroes.resize( numBytesBlock, 0 );
		std::vector< float > ones;
		ones.resize( numBytesBlock, 1.0f );
		std::vector< uint8_t > initialXOR;
		initialXOR.reserve( numBytesBlock );
		for ( uint32_t x = 0; x < blockDim.x; x++ ) {
			for ( uint32_t y = 0; y < blockDim.y; y++ ) {
				for ( uint32_t z = 0; z < blockDim.z; z++ ) {
					for ( int c = 0; c < 4; c++ ) {
						initialXOR.push_back( x ^ y ^ z );
					}
				}
			}
		}

		// Color Blocks (2x)
		opts.dataType = GL_RGBA8;
		opts.width = blockDim.x;
		opts.height = blockDim.y;
		opts.depth = blockDim.z;
		opts.layers = 1;
		opts.minFilter = GL_LINEAR_MIPMAP_LINEAR;
		opts.magFilter = GL_LINEAR;
		opts.wrap = GL_CLAMP_TO_EDGE;
		opts.textureType = GL_TEXTURE_3D;
		opts.initialData = &initialXOR.data()[ 0 ];
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		textureManager.Add( "Color Block 0", opts );

		opts.initialData = &zeroes.data()[ 0 ];
		textureManager.Add( "Color Block 1", opts );

		// Mask Blocks (2x)
		opts.dataType = GL_R8UI;
		opts.magFilter = GL_NEAREST;
		opts.minFilter = GL_NEAREST;
		textureManager.Add( "Mask Block 0", opts );
		textureManager.Add( "Mask Block 1", opts );

		// Lighting Block
		opts.dataType = GL_RGBA16F;
		opts.minFilter = GL_LINEAR_MIPMAP_LINEAR;
		opts.magFilter = GL_LINEAR;
		opts.initialData = &ones.data()[ 0 ];
		opts.pixelDataType = GL_FLOAT;
		textureManager.Add( "Lighting Block", opts );

		// Loadbuffer Block
		opts.dataType = GL_RGBA8;
		opts.magFilter = GL_NEAREST;
		opts.minFilter = GL_NEAREST;
		opts.initialData = &zeroes.data()[ 0 ];
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		textureManager.Add( "LoadBuffer", opts );

		// Heightmap texture
		opts.depth = 1;
		opts.dataType = GL_RGBA16F;
		opts.minFilter = GL_LINEAR;
		opts.magFilter = GL_LINEAR;
		opts.textureType = GL_TEXTURE_2D;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = nullptr;
		textureManager.Add( "Heightmap", opts );

		// and update the last seen value
		lastBlockDim = blockDim;
	}

	// palette textures will be the same, regardless...
	bool paletteTexturesInitialized = false;
	if ( !paletteTexturesInitialized ) {
		textureOptions_t opts;

		// 2D texture to hold the palette data...
		opts.width = 256;
		opts.height = 1;
		opts.depth = 1;
		opts.dataType = GL_RGBA32F;
		textureManager.Add( "Palette Color Data", opts );

		// 3D texture to hold the LUT
		opts.width = 256;
		opts.height = 256;
		opts.depth = 256;
		opts.dataType = GL_R32UI; // only need 16 bits (2x 8-bit indices), but whatever
		opts.textureType = GL_TEXTURE_3D;
		textureManager.Add( "Palette LUT", opts );
	
		// and then indicate that we don't have to do this work again
		paletteTexturesInitialized = true;
	}
}

void Voraldo13::newHeightmapPerlin() {
	// might add more parameters at some point
	std::vector<unsigned char> data;
	const uint32_t dim = max( max( blockDim.x, blockDim.y ), blockDim.z );
	PerlinNoise p;
	float xscale = 0.014f;
	float yscale = 0.04f;
	static float offset = 0;
	data.resize( dim * dim * 4 );
	for ( unsigned int x = 0; x < dim; x++ ) {
		for ( unsigned int y = 0; y < dim; y++ ) {
			data.push_back( ( unsigned char ) ( p.noise( x * xscale, y * yscale, offset ) * 255 ) );
			data.push_back( ( unsigned char ) ( p.noise( x * xscale, y * yscale, offset ) * 255 ) );
			data.push_back( ( unsigned char ) ( p.noise( x * xscale, y * yscale, offset ) * 255 ) );
			data.push_back( 255 );
		}
	}
	offset += 0.5f; // so it varies between updates ... ehh
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Heightmap" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, dim, dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[ 0 ] );
}

void Voraldo13::newHeightmapDiamondSquare() {
	long unsigned int seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine engine{ seed };
	std::uniform_real_distribution<float> distribution{ 0, 1 };
	uint32_t size = max( max( blockDim.x, blockDim.y ), blockDim.z ) + 1;
	uint32_t edge = size - 1;
	
	std::vector< std::vector< uint8_t > > map;
	map.resize( size );
	for ( uint32_t i = 0; i < size; i++ ) {
		map[ i ].resize( size );
	}

	map[ 0 ][ 0 ] = map[ edge ][ 0 ] = map[ 0 ][ edge ] = map[ edge ][ edge ] = 128;

	heightfield::diamond_square_no_wrap( size,
		[&engine, &distribution]( float range ) { // rng
			return distribution( engine ) * range;
		},
		[]( int level ) -> float { // variance
			return 64.0f * std::pow( 0.5f, level );
		},
		[&map]( int x, int y ) -> uint8_t& { // at
			return map[ y ][ x ];
		} );

	std::vector<unsigned char> data;
	for ( int x = 0; x < size; x++ ) {
		for ( int y = 0; y < size; y++ ) {
			data.push_back( map[ x ][ y ] );
			data.push_back( map[ x ][ y ] );
			data.push_back( map[ x ][ y ] );
			data.push_back( 255 );
		}
	}
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Heightmap" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[ 0 ] );
}

void Voraldo13::newHeightmapXOR() {
	static std::vector<unsigned char> data;
	static bool firstTime = true;
	const uint32_t dim = max( max( blockDim.x, blockDim.y ), blockDim.z );
	data.reserve( dim * dim * 4 );
	if ( firstTime ) {
		for ( unsigned int x = 0; x < dim; x++ ) {
			for ( unsigned int y = 0; y < dim; y++ ) {
				unsigned int val = x ^ y;
				data.push_back( val );
				data.push_back( val );
				data.push_back( val );
				data.push_back( 255 );
			}
		}
		firstTime = false;
	}
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Heightmap" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, dim, dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[ 0 ] );
}

void Voraldo13::newHeightmapAND() {
	static std::vector<unsigned char> data;
	static bool firstTime = true;
	// not sure how this is going to handle nonuniform block sizes...
	const uint32_t dim = max( max( blockDim.x, blockDim.y ), blockDim.z );
	data.reserve( dim * dim * 4 );
	if ( firstTime ) {
		for ( unsigned int x = 0; x < dim; x++ ) {
			for ( unsigned int y = 0; y < dim; y++ ) {
				unsigned int val = x & y;
				data.push_back( val );
				data.push_back( val );
				data.push_back( val );
				data.push_back( 255 );
			}
		}
		firstTime = false;
	}
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Heightmap" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, dim, dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[ 0 ] );
}

void Voraldo13::MenuPopulate() {
	YAML::Node config = YAML::LoadFile( "../src/projects/Voraldo13/menuConfig.yaml" );
	int numEntries = config[ "Entries" ].size();
	for ( int i = 0; i < numEntries; i++ ) {

		category_t entryCategoryType = category_t::none;
		string entryCategory = config[ "Entries" ][ i ][ "Category" ].as<string>();
		string entryLabel = config[ "Entries" ][ i ][ "Label" ].as<string>();

		if ( entryCategory == string( "Shapes" ) )
			entryCategoryType = category_t::shapes;
		else if ( entryCategory == string( "Utilities" ) )
			entryCategoryType = category_t::utilities;
		else if ( entryCategory == string( "Lighting" ) )
			entryCategoryType = category_t::lighting;
		else if ( entryCategory == string( "Settings" ) )
			entryCategoryType = category_t::settings;

		// add parameters...

		menu.entries.push_back( menuEntry( entryLabel, entryCategoryType ) );
	}
}

// used in load/save operation to check extension
bool Voraldo13::hasEnding( std::string fullString, std::string ending ) {
	if ( fullString.length() >= ending.length() ) {
		return ( 0 == fullString.compare( fullString.length() - ending.length(), ending.length(), ending ) );
	}
	else {
		return false;
	}
}

bool Voraldo13::hasPNG( std::string filename ) {
	return hasEnding( filename, std::string( ".png" ) );
}

void Voraldo13::updateSavesList() {
	struct pathLeafString {
		std::string operator()( const std::filesystem::directory_entry& entry ) const {
			return entry.path().string();
		}
	};
	savesList.clear();
	std::filesystem::path p( "../src/projects/Voraldo13/saves" );
	std::filesystem::directory_iterator start( p );
	std::filesystem::directory_iterator end;
	std::transform( start, end, std::back_inserter( savesList ), pathLeafString() );
	std::sort( savesList.begin(), savesList.end() ); // sort these alphabetically
}

void Voraldo13::AddBool( json& j, string label, bool value ) {
	j[ label.c_str() ][ "type" ] = "bool";
	j[ label.c_str() ][ "x" ] = value;
}

void Voraldo13::AddInt( json& j, string label, int value ) {
	j[ label.c_str() ][ "type" ] = "int";
	j[ label.c_str() ][ "x" ] = value;
}

void Voraldo13::AddFloat( json& j, string label, float value ) {
	j[ label.c_str() ][ "type" ] = "float";
	j[ label.c_str() ][ "x" ] = value;
}

void Voraldo13::AddIvec3( json& j, string label, glm::ivec3 value ) {
	j[ label.c_str() ][ "type" ] = "ivec3";
	j[ label.c_str() ][ "x" ] = value.x;
	j[ label.c_str() ][ "y" ] = value.y;
	j[ label.c_str() ][ "z" ] = value.z;
}

void Voraldo13::AddVec3( json& j, string label, glm::vec3 value ) {
	j[ label.c_str() ][ "type" ] = "vec3";
	j[ label.c_str() ][ "x" ] = value.x;
	j[ label.c_str() ][ "y" ] = value.y;
	j[ label.c_str() ][ "z" ] = value.z;
}

void Voraldo13::AddVec4( json& j, string label, glm::vec4 value ) {
	j[ label.c_str() ][ "type" ] = "vec4";
	j[ label.c_str() ][ "x" ] = value.x;
	j[ label.c_str() ][ "y" ] = value.y;
	j[ label.c_str() ][ "z" ] = value.z;
	j[ label.c_str() ][ "w" ] = value.w;
}

void Voraldo13::SendUniforms( json j ) {
	ZoneScoped;

	// prepare to send
	GLuint shader = shaders[ j[ "shader" ] ];
	glUseProgram( shader );

	// iterate through the entries
	for ( auto& element : j.items() ) {

		// name of the operation, or name of the shader
		string label( element.key() );

		// the type of the uniform - "null" is a special value for:
			// shader label
			// bindset
			// user shader text
			// dispatch type label
		bool ignore = ( label == "shader" || label == "bindset" || label == "text" || label == "dispatchType" );
		string type( ignore ? "null" : element.value()[ "type" ] );

		// shortens references
		json val = element.value();

		if ( type == "null" ) {
			continue;
		}
		else if ( type == "bool" ) {
			glUniform1i( glGetUniformLocation( shader, label.c_str() ), val[ "x" ].get<bool>() );
		}
		else if ( type == "int" ) {
			glUniform1i( glGetUniformLocation( shader, label.c_str() ), val[ "x" ] );
		}
		else if ( type == "float" ) {
			glUniform1f( glGetUniformLocation( shader, label.c_str() ), val[ "x" ] );
		}
		else if ( type == "ivec3" ) {
			glUniform3i( glGetUniformLocation( shader, label.c_str() ), val[ "x" ], val[ "y" ], val[ "z" ] );
		}
		else if ( type == "vec3" ) {
			glUniform3f( glGetUniformLocation( shader, label.c_str() ), val[ "x" ], val[ "y" ], val[ "z" ] );
		}
		else if ( type == "vec4" ) {
			glUniform4f( glGetUniformLocation( shader, label.c_str() ), val[ "x" ], val[ "y" ], val[ "z" ], val[ "w" ] );
		}
	}
}

void Voraldo13::setColorMipmapFlag() {
	mipmapFlagColor = true;
}

void Voraldo13::setLightMipmapFlag() {
	mipmapFlagLight = true;
}

void Voraldo13::genColorMipmap() {
	if ( mipmapFlagColor && shaders[ "Renderer" ] == shaders[ "Sampler Raymarch" ] ) {
		glBindTexture( GL_TEXTURE_3D, textureManager.Get( "Color Block Front" ) );
		glGenerateMipmap( GL_TEXTURE_3D );
	}
}

void Voraldo13::genLightMipmap() {
	if ( mipmapFlagLight && shaders[ "Renderer" ] == shaders[ "Sampler Raymarch" ] ) {
		glBindTexture( GL_TEXTURE_3D, textureManager.Get( "Lighting Block" ) );
		glGenerateMipmap( GL_TEXTURE_3D );
	}
}

string Voraldo13::processAddEscapeSequences( string input ) {
	// parse string - change tab to two spaces, newline to \n, etc
	std::string output;
	for ( const auto& c : input ) {
		if ( c == '\t' ) {
			output += std::string( "  " );
		}
		else if ( c == '\n' ) {
			output += std::string( "\\n" );
		}
		else {
			output += c;
		}
	}
	return output;
}

void Voraldo13::AddToLog( json j ) {
	// add the operation record to the log
	log.push_back( j );
}

void Voraldo13::DumpLog() {
	for ( auto& j : log ) {
		cout << j << newline;
	}
}

void Voraldo13::BlockDispatch() {
	ZoneScoped;
	glDispatchCompute( ( blockDim.x + 7 ) / 8, ( blockDim.y + 7 ) / 8, ( blockDim.z + 7 ) / 8 );
	glMemoryBarrier( GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	render.framesSinceLastInput = 0;
}

void Voraldo13::SwapBlocks() {
	// this makes it so that you can just use e.g. the Rendering bindset for the
	// renderer, abstracts away the need for logic around a buffer toggle - just
	// call SwapBlocks() after operations which change which blocks play the
	// role of front/back to make sure the state is correct

	render.flipColorBlocks = !render.flipColorBlocks;
	render.framesSinceLastInput = 0;
}

void Voraldo13::SendRaymarchParameters() {
	ZoneScoped;
	const GLuint shader = shaders[ "Renderer" ];
	const glm::mat3 inverseBasisMat = inverse( glm::mat3( -trident.basisX, -trident.basisY, -trident.basisZ ) );
	glUniformMatrix3fv( glGetUniformLocation( shader, "invBasis" ), 1, false, glm::value_ptr( inverseBasisMat ) );
	glUniform1f( glGetUniformLocation( shader, "scale" ), -render.scaleFactor );
	glUniform1f( glGetUniformLocation( shader, "blendFactor" ), render.blendFactor );
	glUniform1f( glGetUniformLocation( shader, "perspectiveFactor" ), render.perspective );
	glUniform4fv( glGetUniformLocation( shader, "clearColor" ), 1, glm::value_ptr( render.clearColor ) );
	glUniform2f( glGetUniformLocation( shader, "renderOffset" ), render.renderOffset.x, render.renderOffset.y );
	glUniform1f( glGetUniformLocation( shader, "alphaPower" ), render.alphaCorrectionPower );
	glUniform1i( glGetUniformLocation( shader, "numSteps" ), render.volumeSteps );
	glUniform1f( glGetUniformLocation( shader, "jitterFactor" ), render.jitterAmount );
	glUniform1i( glGetUniformLocation( shader, "useThinLens" ), render.useThinLens );
	glUniform1f( glGetUniformLocation( shader, "thinLensFocusDist" ), render.thinLensFocusDist );
}

void Voraldo13::SendTonemappingParameters() {
	ZoneScoped;

	static float prevColorTemperature = 0.0f;
	static vec3 temperatureColor;
	if ( tonemap.colorTemp != prevColorTemperature ) {
		prevColorTemperature = tonemap.colorTemp;
		temperatureColor = GetColorForTemperature( tonemap.colorTemp );
	}

	// precompute the 3x3 matrix for the saturation adjustment
	static float prevSaturationValue = -1.0f;
	static mat3 saturationMatrix;
	if ( tonemap.saturation != prevSaturationValue ) {
		// https://www.graficaobscura.com/matrix/index.html
		const float s = tonemap.saturation;
		const float oms = 1.0f - s;

		vec3 weights = tonemap.saturationImprovedWeights ?
			vec3( 0.3086f, 0.6094f, 0.0820f ) :	// "improved" luminance vector
			vec3( 0.2990f, 0.5870f, 0.1140f );	// NTSC weights

		saturationMatrix = mat3(
			oms * weights.r + s, oms * weights.r, oms * weights.r,
			oms * weights.g, oms * weights.g + s, oms * weights.g,
			oms * weights.b, oms * weights.b, oms * weights.b + s
		);
	}

	const GLuint shader = shaders[ "Tonemap" ];
	static rngi blueNoiseOffset = rngi( 0, 512 );
	glUniform2i( glGetUniformLocation( shader, "blueNoiseOffset" ), blueNoiseOffset(), blueNoiseOffset() );
	glUniform3fv( glGetUniformLocation( shader, "colorTempAdjust" ), 1, glm::value_ptr( temperatureColor ) );
	glUniform1i( glGetUniformLocation( shader, "tonemapMode" ), tonemap.tonemapMode );
	glUniform1f( glGetUniformLocation( shader, "gamma" ), tonemap.gamma );
	glUniform1f( glGetUniformLocation( shader, "postExposure" ), tonemap.postExposure );
	glUniformMatrix3fv( glGetUniformLocation( shader, "saturation" ), 1, false, glm::value_ptr( saturationMatrix ) );
	glUniform1i( glGetUniformLocation( shader, "enableVignette" ), tonemap.enableVignette );
	glUniform1f( glGetUniformLocation( shader, "vignettePower" ), tonemap.vignettePower );

	// problematic if missing
	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 2 );
}

void Voraldo13::SendDitherParametersQ() {
	const GLuint shader = shaders[ "Dither Quantize" ];
	glUniform1i( glGetUniformLocation( shader, "numBits" ), render.ditherNumBits );
	glUniform1i( glGetUniformLocation( shader, "colorspacePick" ), render.ditherSpaceSelect );
	glUniform1i( glGetUniformLocation( shader, "patternSelector" ), render.ditherPattern );
	glUniform1i( glGetUniformLocation( shader, "frameNumber" ), render.framesSinceStartup );

	textureManager.Bind( "Blue Noise", 0 );
	glBindImageTexture( 1, textureManager.Get( "Display Texture" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8UI ); // if it's stupid and works...
}

void Voraldo13::SendDitherParametersP() {
	const GLuint shader = shaders[ "Dither Palette" ];
	glUniform1i( glGetUniformLocation( shader, "colorspacePick" ), render.ditherSpaceSelect );
	glUniform1i( glGetUniformLocation( shader, "patternSelector" ), render.ditherPattern );
	glUniform1i( glGetUniformLocation( shader, "frameNumber" ), render.framesSinceStartup );

	textureManager.Bind( "Blue Noise", 0 );
	glBindImageTexture( 1, textureManager.Get( "Display Texture" ), 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8UI );
	textureManager.Bind( "Palette Color Data", 2 );
}

void Voraldo13::SendSelectedPalette() {
	// update color data for the palette
	if ( paletteResendFlag ) {

	// I don't know if this is worth trying
		// there's some interesting opportunities here... using the interpolated color palette, I could expand any range into 256 colors...
		// in the absence of any reason not to, I think I would prefer to do it that way - may need to revisit, tbd
		const int paletteSize = 256;
		static vec4 colors[ paletteSize ];
		palette::PaletteIndex = render.ditherPaletteIndex;
		for ( int i = 0; i < paletteSize; i++ ) {
			colors[ i ] = vec4( palette::paletteRef( RemapRange( float( i ), 0.0f, 256.0f, render.ditherPaletteMin, render.ditherPaletteMax ) ), 1.0f );
		}

	// else, we'll just use the colors out of the selected palette
	//	const int paletteSize = palette::paletteListLocal[ render.ditherPaletteIndex ].colors.size();
	//	vec4 colors[ 256 ];
	//	for ( int i = 0; i < paletteSize; i++ ) {
	//		colors[ i ] = vec4( vec3( palette::paletteListLocal[ render.ditherPaletteIndex ].colors[ i ] ) / 255.0f, 1.0f );
	//	}

		glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Palette Color Data" ) );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, paletteSize, 1, 0, GL_RGBA, GL_FLOAT, &colors[ 0 ] );

		// run compute shader to precompute LUT
		// glUseProgram( shaders[ "Dither Palette LUT Precompute" ] );
		// glDispatchCompute( 64, 64, 64 ); // hardcoded for 8-bit color space

		paletteResendFlag = false;
	}
}

void Voraldo13::CapturePostprocessScreenshot() {
	wantCapturePostprocessScreenshot = false;
	std::vector< float > imageBytesToSaveP;
	imageBytesToSaveP.resize( config.width * config.height * 4 );
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Display Texture" ) );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSaveP.data()[ 0 ] );
	Image_4F screenshotP( config.width, config.height, &imageBytesToSaveP.data()[ 0 ] );
	screenshotP.FlipVertical();
	screenshotP.RGBtoSRGB();
	if ( postprocessScreenshotScaleFactor != 1.0f ) {
		screenshotP.Resize( postprocessScreenshotScaleFactor );
	}
	screenshotP.Save( string( "Voraldo13_P-" ) + timeDateString() + string( ".png" ) );
}

void Voraldo13::RenderBindings() {
	textureManager.Bind( "Blue Noise", 0 );
	textureManager.Bind( "Accumulator", 1 );
	textureManager.Bind( render.flipColorBlocks ? "Color Block 1" : "Color Block 0", 2 );
	textureManager.Bind( "Lighting Block", 3 );
}

void Voraldo13::BasicOperationBindings () {
	textureManager.Bind( render.flipColorBlocks ? "Color Block 1" : "Color Block 0", 0 );
	textureManager.Bind( render.flipColorBlocks ? "Color Block 0" : "Color Block 1", 1 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 1" : "Mask Block 0", 2 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 0" : "Mask Block 1", 3 );
	textureManager.Bind( "Blue Noise", 4 );
}

void Voraldo13::HeightmapOperationBindings() {
	textureManager.Bind( render.flipColorBlocks ? "Color Block 1" : "Color Block 0", 0 );
	textureManager.Bind( render.flipColorBlocks ? "Color Block 0" : "Color Block 1", 1 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 1" : "Mask Block 0", 2 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 0" : "Mask Block 1", 3 );
	textureManager.Bind( "Heightmap", 4 );
}

void Voraldo13::LoadBufferOperationBindings() {
	textureManager.Bind( render.flipColorBlocks ? "Color Block 1" : "Color Block 0", 0 );
	textureManager.Bind( render.flipColorBlocks ? "Color Block 0" : "Color Block 1", 1 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 1" : "Mask Block 0", 2 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 0" : "Mask Block 1", 3 );
	textureManager.Bind( "LoadBuffer", 4 );
}

void Voraldo13::BasicOperationWithLightingBindings() {
	textureManager.Bind( render.flipColorBlocks ? "Color Block 1" : "Color Block 0", 0 );
	textureManager.Bind( render.flipColorBlocks ? "Color Block 0" : "Color Block 1", 1 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 1" : "Mask Block 0", 2 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 0" : "Mask Block 1", 3 );
	textureManager.Bind( "Lighting Block", 4 );
}

void Voraldo13::LightingOperationBindings() {
	// inverted wrt normal bindings
	textureManager.Bind( render.flipColorBlocks ? "Color Block 0" : "Color Block 1", 0 );
	textureManager.Bind( render.flipColorBlocks ? "Color Block 1" : "Color Block 0", 1 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 0" : "Mask Block 1", 2 );
	textureManager.Bind( render.flipColorBlocks ? "Mask Block 1" : "Mask Block 0", 3 );
	textureManager.Bind( "Lighting Block", 4 );
	textureManager.Bind( "Blue Noise", 5 );
}
