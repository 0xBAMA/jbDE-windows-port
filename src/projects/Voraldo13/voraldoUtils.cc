#include "voraldo.h"

//==============================================================================
void Voraldo13::CompileShaders () {
	const string base( "../src/projects/Voraldo13/shaders/" );

	// do we need to recompile everything, every time? it takes a nontrivial amount of time, for this many
	shaders[ "Draw" ] = computeShader( base + "draw.cs.glsl" ).shaderHandle;

	// shape operations
	shaders[ "AABB" ] = computeShader( base + "operations/AABB.cs.glsl" ).shaderHandle;
	shaders[ "Cylinder" ] = computeShader( base + "operations/cylinder.cs.glsl" ).shaderHandle;
	shaders[ "Data Mask" ] = computeShader( base + "operations/dataMask.cs.glsl" ).shaderHandle;
	shaders[ "Ellipsoid" ] = computeShader( base + "operations/ellipsoid.cs.glsl" ).shaderHandle;
	shaders[ "Heightmap" ] = computeShader( base + "operations/heightmap.cs.glsl" ).shaderHandle;
	shaders[ "Grid" ] = computeShader( base + "operations/grid.cs.glsl" ).shaderHandle;
	shaders[ "Sphere" ] = computeShader( base + "operations/sphere.cs.glsl" ).shaderHandle;
	shaders[ "Triangle" ] = computeShader( base + "operations/triangle.cs.glsl" ).shaderHandle;
	shaders[ "XOR" ] = computeShader( base + "operations/xor.cs.glsl" ).shaderHandle;

	// utility operations
	shaders[ "Box Blur" ] = computeShader( base + "operations/boxBlur.cs.glsl" ).shaderHandle;
	shaders[ "Gaussian Blur" ] = computeShader( base + "operations/gaussBlur.cs.glsl" ).shaderHandle;
	shaders[ "Clear" ] = computeShader( base + "operations/clear.cs.glsl" ).shaderHandle;
	shaders[ "Load" ] = computeShader( base + "operations/load.cs.glsl" ).shaderHandle;
	shaders[ "Mask Invert" ] = computeShader( base + "operations/maskInvert.cs.glsl" ).shaderHandle;
	shaders[ "Mask Clear" ] = computeShader( base + "operations/maskClear.cs.glsl" ).shaderHandle;
	shaders[ "Shift" ] = computeShader( base + "operations/shift.cs.glsl" ).shaderHandle;

	// lighting operations
	shaders[ "Light Clear" ] = computeShader( base + "lighting/clear.cs.glsl" ).shaderHandle;
	shaders[ "Light Mash" ] = computeShader( base + "lighting/mash.cs.glsl" ).shaderHandle;
	shaders[ "Fake GI" ] = computeShader( base + "lighting/fakeGI.cs.glsl" ).shaderHandle;
	shaders[ "Directional Light" ] = computeShader( base + "lighting/directional.cs.glsl" ).shaderHandle;
	shaders[ "Point Light" ] = computeShader( base + "lighting/point.cs.glsl" ).shaderHandle;
	shaders[ "Cone Light" ] = computeShader( base + "lighting/cone.cs.glsl" ).shaderHandle;
	shaders[ "Ambient Occlusion" ] = computeShader( base + "lighting/ambientOcclusion.cs.glsl" ).shaderHandle;

	// color adjustments - uses custom tonemap shader
	// shaders[ "Tonemap" ] = computeShader( base + "tonemap.cs.glsl" ).shaderHandle;
	shaders[ "Dither Quantize" ] = computeShader( base + "ditherQuantize.cs.glsl" ).shaderHandle;
	shaders[ "Dither Palette" ] = computeShader( base + "ditherPalette.cs.glsl" ).shaderHandle;

	// renderers
	shaders[ "Image3D Raymarch" ] = computeShader( base + "renderers/raymarch.cs.glsl" ).shaderHandle;
	shaders[ "Sampler Raymarch" ] = computeShader( base + "renderers/raymarchSampler.cs.glsl" ).shaderHandle;
	shaders[ "Renderer" ] = shaders[ "Image3D Raymarch" ]; // default
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

	// todo

		// 2D texture to hold the palette data... why not just do all of the palettes, it's not a lot of memory and we can index with an int

		// 3D texture to hold the LUT
	
		// and then indicate that we don't have to do this work again
		paletteTexturesInitialized = true;
	}
}

void Voraldo13::newHeightmapPerlin() {
	// might add more parameters at some point
	std::vector<unsigned char> data;
	PerlinNoise p;
	float xscale = 0.014f;
	float yscale = 0.04f;
	static float offset = 0;
	// TODO: this is fucked
	/*
	for ( unsigned int x = 0; x < BLOCKDIM; x++ ) {
		for ( unsigned int y = 0; y < BLOCKDIM; y++ ) {
			data.push_back( ( unsigned char ) ( p.noise( x * xscale, y * yscale, offset ) * 255 ) );
			data.push_back( ( unsigned char ) ( p.noise( x * xscale, y * yscale, offset ) * 255 ) );
			data.push_back( ( unsigned char ) ( p.noise( x * xscale, y * yscale, offset ) * 255 ) );
			data.push_back( 255 );
		}
	}
	*/
	offset += 0.5; // so it varies between updates ... ehh
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Heightmap" ) );
	// glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, BLOCKDIM, BLOCKDIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[ 0 ] );
}

void Voraldo13::newHeightmapDiamondSquare() {
/*
	long unsigned int seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine engine{ seed };
	std::uniform_real_distribution<float> distribution{ 0, 1 };
	auto size = max( max( blockDim.x, blockDim.y ), blockDim.z ) + 1;
	auto edge = size - 1;
	// TODO: need to fix this, constant sizing issue
	 uint8_t map[ size ][ size ] = { { 0 } };
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

	ImGui::Text( "TODO: Very high likelyhood this is fucked" );
	
	std::vector<unsigned char> data;
	for ( int x = 0; x < BLOCKDIM; x++ ) {
		for ( int y = 0; y < BLOCKDIM; y++ ) {
			data.push_back( map[ x ][ y ] );
			data.push_back( map[ x ][ y ] );
			data.push_back( map[ x ][ y ] );
			data.push_back( 255 );
		}
	}
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Heightmap" ) );
	// glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, BLOCKDIM, BLOCKDIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[ 0 ] );
*/
}

void Voraldo13::newHeightmapXOR() {
	static std::vector<unsigned char> data;
	static bool firstTime = true;
	if ( firstTime ) {
	/*
		for ( unsigned int x = 0; x < BLOCKDIM; x++ ) {
			for ( unsigned int y = 0; y < BLOCKDIM; y++ ) {
				unsigned int val = x ^ y;
				data.push_back( val );
				data.push_back( val );
				data.push_back( val );
				data.push_back( 255 );
			}
		}
		firstTime = false;
	*/
	}
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Heightmap" ) );
	// glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, BLOCKDIM, BLOCKDIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[ 0 ] );
}

void Voraldo13::newHeightmapAND() {
	static std::vector<unsigned char> data;
	static bool firstTime = true;
	if ( firstTime ) {
	/*
		for ( unsigned int x = 0; x < BLOCKDIM; x++ ) {
			for ( unsigned int y = 0; y < BLOCKDIM; y++ ) {
				unsigned int val = x & y;
				data.push_back( val );
				data.push_back( val );
				data.push_back( val );
				data.push_back( 255 );
			}
		}
		firstTime = false;
	*/
	}
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Heightmap" ) );
	// glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, BLOCKDIM, BLOCKDIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[ 0 ] );
}

void Voraldo13::MenuPopulate() {
	std::ifstream i( "../src/projects/Voraldo13/menuConfig.json" );
	json j; i >> j;
	for ( auto& element : j[ "Entries" ] ) {
		// construct each menu entry and add
		string entryLabel = element[ "Label" ];
		category_t entryCategory = category_t::none;
		if ( element[ "Category" ] == string( "Shapes" ) )
			entryCategory = category_t::shapes;
		else if ( element[ "Category" ] == string( "Utilities" ) )
			entryCategory = category_t::utilities;
		else if ( element[ "Category" ] == string( "Lighting" ) )
			entryCategory = category_t::lighting;
		else if ( element[ "Category" ] == string( "Settings" ) )
			entryCategory = category_t::settings;
		menu.entries.push_back( menuEntry( entryLabel, entryCategory ) );
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
	std::filesystem::path p( "data/saves" );
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

	flipColorBlocks = !flipColorBlocks;
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

/*
void Voraldo13::SendTonemappingParameters() {
	ZoneScoped;
	const GLuint shader = shaders[ "Tonemap" ];
	glUniform3fv( glGetUniformLocation( shader, "colorTempAdjust" ), 1, glm::value_ptr( GetColorForTemperature( tonemap.colorTemp ) ) );
	glUniform1i( glGetUniformLocation( shader, "tonemapMode" ), tonemap.tonemapMode );
	glUniform1f( glGetUniformLocation( shader, "gamma" ), tonemap.gamma );
}
*/
