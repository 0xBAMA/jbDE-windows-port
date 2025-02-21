#include "daedalus.h"

void Daedalus::ResetAccumulators() {
	ZoneScoped;
	textureManager.ZeroTexture2D( "Color Accumulator" );
	textureManager.ZeroTexture2D( "Depth/Normals Accumulator" );
	textureManager.ZeroTexture2D( "Tonemapped" );
	daedalusConfig.tiles.RegenerateTileList(); // timer / sample count reset
}

void Daedalus::ResizeAccumulators( uint32_t x, uint32_t y ) {
	ZoneScoped;
	// destroy the existing textures
	textureManager.Remove( "Depth/Normals Accumulator" );
	textureManager.Remove( "Color Accumulator Scratch" );
	textureManager.Remove( "Color Accumulator" );
	textureManager.Remove( "Tonemapped" );

	// create the new ones
	textureOptions_t opts;
	opts.dataType		= GL_RGBA32F;
	opts.width			= x;
	opts.height			= y;
	opts.minFilter		= GL_LINEAR;
	opts.magFilter		= GL_LINEAR;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	textureManager.Add( "Depth/Normals Accumulator", opts );
	textureManager.Add( "Color Accumulator Scratch", opts );
	textureManager.Add( "Color Accumulator", opts );
	textureManager.Add( "Tonemapped", opts );

	// regen the tile list, reset timer, etc
	daedalusConfig.targetWidth = x;
	daedalusConfig.targetHeight = y;
	daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize, x, y );

	// recenter the view on the middle of the image
	daedalusConfig.view.outputOffset = daedalusConfig.view.outputZoom * ( vec2( daedalusConfig.targetWidth, daedalusConfig.targetHeight ) - vec2( config.width, config.height ) ) / 2.0f;
}

void Daedalus::Screenshot( string label, bool srgbConvert, bool fullDepth ) {
	const GLuint tex = textureManager.Get( label );
	uvec2 dims = textureManager.GetDimensions( label );
	std::vector< float > imageBytesToSave;
	imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
	glBindTexture( GL_TEXTURE_2D, tex );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
	Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
	if ( srgbConvert == true ) {
		screenshot.RGBtoSRGB();
	}
	const string filename = string( "Daedalus-" ) + timeDateString() + string( fullDepth ? ".exr" : ".png" );
	screenshot.Save( filename, fullDepth ? Image_4F::backend::TINYEXR : Image_4F::backend::LODEPNG );
}

void Daedalus::ApplyFilter( int mode, int count ) {
	rngi pick = rngi( 0, 3 );
	GLuint shader;
	const uvec2 dims = textureManager.GetDimensions( "Color Accumulator" );
	for ( int i = 0; i < count; i++ ) {
		shader = shaders[ "Filter" ];
		glUseProgram( shader );
		glUniform1i( glGetUniformLocation( shader, "mode" ), mode == 4 ? pick() : mode );
		textureManager.BindImageForShader( "Color Accumulator", "sourceData", shader, 0 );
		textureManager.BindImageForShader( "Color Accumulator Scratch", "destData", shader, 1 );
		glDispatchCompute( ( dims.x + 15 ) / 16, ( dims.y + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		shader = shaders[ "Copy Back" ];
		glUseProgram( shader );
		glUniform1f( glGetUniformLocation( shader, "blendAmount" ), daedalusConfig.filterBlendAmount );
		textureManager.BindImageForShader( "Color Accumulator Scratch", "sourceData", shader, 0 );
		textureManager.BindImageForShader( "Color Accumulator", "destData", shader, 1 );
		glDispatchCompute( ( dims.x + 15 ) / 16, ( dims.y + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
}


void Daedalus::SendSkyCacheUniforms() {
	const GLuint shader = shaders[ "Sky Cache" ];
	glUseProgram( shader );

	glUniform1f( glGetUniformLocation( shader, "skyTime" ), daedalusConfig.render.scene.skyTime );
	glUniform1i( glGetUniformLocation( shader, "mode" ), daedalusConfig.render.scene.skyMode );
	glUniform3fv( glGetUniformLocation( shader, "color1" ), 1, glm::value_ptr( daedalusConfig.render.scene.skyConstantColor1 ) );
	glUniform3fv( glGetUniformLocation( shader, "color2" ), 1, glm::value_ptr( daedalusConfig.render.scene.skyConstantColor2 ) );
	glUniform1i( glGetUniformLocation( shader, "sunThresh" ), daedalusConfig.render.scene.sunThresh );

	textureManager.BindImageForShader( "Sky Cache", "skyCache", shader, 0 );
	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 1 );
}

void Daedalus::SendBasePathtraceUniforms() {
	const GLuint shader = shaders[ "Pathtrace" ];
	glUseProgram( shader );

	static uint32_t sampleCount = 0;
	static ivec2 blueNoiseOffset;
	if ( sampleCount != daedalusConfig.tiles.SampleCount() ) sampleCount = daedalusConfig.tiles.SampleCount(),
		blueNoiseOffset = ivec2( daedalusConfig.rng.blueNoiseOffset(), daedalusConfig.rng.blueNoiseOffset() );
	glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), blueNoiseOffset.x, blueNoiseOffset.y );
	glUniform1i( glGetUniformLocation( shader, "subpixelJitterMethod" ), daedalusConfig.render.subpixelJitterMethod );
	glUniform1i( glGetUniformLocation( shader, "sampleNumber" ), daedalusConfig.tiles.SampleCount() );
	glUniform1i( glGetUniformLocation( shader, "bokehMode" ), daedalusConfig.render.bokehMode );
	glUniform1i( glGetUniformLocation( shader, "cameraType" ), daedalusConfig.render.cameraType );
	glUniform1f( glGetUniformLocation( shader, "voraldoCameraScalar" ), daedalusConfig.render.voraldoCameraScalar );
	glUniform1i( glGetUniformLocation( shader, "cameraOriginJitter" ), daedalusConfig.render.cameraOriginJitter );
	glUniform1f( glGetUniformLocation( shader, "uvScalar" ), daedalusConfig.render.uvScalar ); // consider making this 2d
	glUniform1f( glGetUniformLocation( shader, "exposure" ), daedalusConfig.render.exposure );
	glUniform1f( glGetUniformLocation( shader, "FoV" ), daedalusConfig.render.FoV );
	glUniform1f( glGetUniformLocation( shader, "maxDistance" ), daedalusConfig.render.maxDistance );
	glUniform1f( glGetUniformLocation( shader, "epsilon" ), daedalusConfig.render.epsilon );
	glUniform1i( glGetUniformLocation( shader, "maxBounces" ), daedalusConfig.render.maxBounces );
	glUniform1f( glGetUniformLocation( shader, "skyClamp" ), daedalusConfig.render.scene.skyClamp );
	glUniform1i( glGetUniformLocation( shader, "skyInvert" ), daedalusConfig.render.scene.skyInvert );
	glUniform1f( glGetUniformLocation( shader, "skyBrightnessScalar" ), daedalusConfig.render.scene.skyBrightnessScalar );

	glUniform1i( glGetUniformLocation( shader, "raymarchEnable" ), daedalusConfig.render.scene.raymarchEnable );
	glUniform1i( glGetUniformLocation( shader, "raymarchMaxSteps" ), daedalusConfig.render.scene.raymarchMaxSteps );
	glUniform1f( glGetUniformLocation( shader, "raymarchUnderstep" ), daedalusConfig.render.scene.raymarchUnderstep );
	glUniform1f( glGetUniformLocation( shader, "raymarchMaxDistance" ), daedalusConfig.render.scene.raymarchMaxDistance );
	glUniform1f( glGetUniformLocation( shader, "marbleRadius" ), daedalusConfig.render.scene.marbleRadius );

	glUniform1i( glGetUniformLocation( shader, "ddaSpheresEnable" ), daedalusConfig.render.scene.ddaSpheresEnable );
	glUniform3fv( glGetUniformLocation( shader, "ddaSpheresBoundSize" ), 1, glm::value_ptr( daedalusConfig.render.scene.ddaSpheresBoundSize ) );
	glUniform1i( glGetUniformLocation( shader, "ddaSpheresResolution" ), daedalusConfig.render.scene.ddaSpheresResolution );

	glUniform1i( glGetUniformLocation( shader, "maskedPlaneEnable" ), daedalusConfig.render.scene.maskedPlaneEnable );

	glUniform1i( glGetUniformLocation( shader, "numExplicitPrimitives" ), daedalusConfig.render.scene.numExplicitPrimitives );
	glUniform1i( glGetUniformLocation( shader, "explicitListEnable" ), daedalusConfig.render.scene.explicitListEnable );

	glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( daedalusConfig.render.viewerPosition ) );
	glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( daedalusConfig.render.basisX ) );
	glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( daedalusConfig.render.basisY ) );
	glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( daedalusConfig.render.basisZ ) );
	glUniform1i( glGetUniformLocation( shader, "thinLensEnable" ), daedalusConfig.render.thinLensEnable );
	glUniform1f( glGetUniformLocation( shader, "thinLensFocusDistance" ), daedalusConfig.render.thinLensFocusDistance );
	glUniform1f( glGetUniformLocation( shader, "thinLensJitterRadiusInner" ), daedalusConfig.render.thinLensJitterRadiusInner );
	glUniform1f( glGetUniformLocation( shader, "thinLensJitterRadiusOuter" ), daedalusConfig.render.thinLensJitterRadiusOuter );

	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );
	textureManager.BindImageForShader( "Color Accumulator", "accumulatorColor", shader, 1 );
	textureManager.BindImageForShader( "Depth/Normals Accumulator", "accumulatorNormalsAndDepth", shader, 2 );
	textureManager.BindTexForShader( "Sky Cache", "skyCache", shader, 3 );
	textureManager.BindImageForShader( "TextBuffer", "textBuffer", shader, 4 );
	textureManager.BindImageForShader( "DDATex", "DDATex", shader, 5 );
	// textureManager.BindImageForShader( "HeightmapTex", "HeightmapTex", shader, 5 );
}

void Daedalus::SendInnerLoopPathtraceUniforms() {
	const GLuint shader = shaders[ "Pathtrace" ];
	const ivec2 tileOffset = daedalusConfig.tiles.GetTile(); // send uniforms ( unique per loop iteration )
	glUniform2i( glGetUniformLocation( shader, "tileOffset" ),	tileOffset.x, tileOffset.y );
	glUniform1i( glGetUniformLocation( shader, "wangSeed" ),	daedalusConfig.rng.wangSeeder() );
}

void Daedalus::SendPrepareUniforms() {
	const GLuint shader = shaders[ "Prepare" ];
	glUseProgram( shader );

	// make sure we're sending the updated color temp every frame
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
			oms * weights.r + s,	oms * weights.r,		oms * weights.r,
			oms * weights.g,		oms * weights.g + s,	oms * weights.g,
			oms * weights.b,		oms * weights.b,		oms * weights.b + s
		);
	}

	glUniform3fv( glGetUniformLocation( shader, "colorTempAdjust" ), 1,			glm::value_ptr( temperatureColor ) );
	glUniform1i( glGetUniformLocation( shader, "tonemapMode" ),					tonemap.tonemapMode );
	glUniformMatrix3fv( glGetUniformLocation( shader, "saturation" ), 1, false,	glm::value_ptr( saturationMatrix ) );
	glUniform1f( glGetUniformLocation( shader, "gamma" ),						tonemap.gamma );
	glUniform1f( glGetUniformLocation( shader, "postExposure" ),				tonemap.postExposure );
	glUniform1i( glGetUniformLocation( shader, "enableVignette" ),				tonemap.enableVignette );
	glUniform1f( glGetUniformLocation( shader, "vignettePower" ),				tonemap.vignettePower );
	glUniform1i( glGetUniformLocation( shader, "updateHistogram" ),				daedalusConfig.render.grading.updateHistogram );
	glUniform1f( glGetUniformLocation( shader, "hueShift" ),					tonemap.hueShift );
	glUniform1i( glGetUniformLocation( shader, "enableLensDistort" ),			daedalusConfig.post.enableLensDistort );
	glUniform1i( glGetUniformLocation( shader, "lensDistortNormalize" ),		daedalusConfig.post.lensDistortNormalize );
	glUniform1i( glGetUniformLocation( shader, "lensDistortNumSamples" ),		daedalusConfig.post.lensDistortNumSamples );
	glUniform3fv( glGetUniformLocation( shader, "lensDistortParametersStart" ), 1, glm::value_ptr( daedalusConfig.post.lensDistortParametersStart ) );
	glUniform3fv( glGetUniformLocation( shader, "lensDistortParametersEnd" ), 1, glm::value_ptr( daedalusConfig.post.lensDistortParametersEnd ) );
	glUniform1i( glGetUniformLocation( shader, "lensDistortChromab" ),			daedalusConfig.post.lensDistortChromab );

	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );
	textureManager.BindImageForShader( "Color Accumulator", "accumulatorColor", shader, 1 );
	textureManager.BindTexForShader( "Color Accumulator", "accumulatorColorTex", shader, 1 );
	textureManager.BindImageForShader( "Depth/Normals Accumulator", "accumulatorNormalsAndDepth", shader, 2 );
	textureManager.BindImageForShader( "Tonemapped", "tonemappedResult", shader, 3 );
}

void Daedalus::SendPresentUniforms() {
	const GLuint shader = shaders[ "Present" ];
	glUseProgram( shader );

	glUniform1f( glGetUniformLocation( shader, "scale" ), daedalusConfig.view.outputZoom );
	glUniform2f( glGetUniformLocation( shader, "offset" ), daedalusConfig.view.outputOffset.x, daedalusConfig.view.outputOffset.y );
	glUniform1i( glGetUniformLocation( shader, "edgeLines" ), daedalusConfig.view.edgeLines );
	glUniform3fv( glGetUniformLocation( shader, "edgeLinesColor" ), 1, glm::value_ptr( daedalusConfig.view.edgeLinesColor ) );
	glUniform1i( glGetUniformLocation( shader, "centerLines" ), daedalusConfig.view.centerLines );
	glUniform3fv( glGetUniformLocation( shader, "centerLinesColor" ), 1, glm::value_ptr( daedalusConfig.view.centerLinesColor ) );
	glUniform1i( glGetUniformLocation( shader, "ROTLines" ), daedalusConfig.view.ROTLines );
	glUniform3fv( glGetUniformLocation( shader, "ROTLinesColor" ), 1, glm::value_ptr( daedalusConfig.view.ROTLinesColor ) );
	glUniform1i( glGetUniformLocation( shader, "goldenLines" ), daedalusConfig.view.goldenLines );
	glUniform3fv( glGetUniformLocation( shader, "goldenLinesColor" ), 1, glm::value_ptr( daedalusConfig.view.goldenLinesColor ) );
	glUniform1i( glGetUniformLocation( shader, "vignette" ), daedalusConfig.view.vignette );

	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );
	textureManager.BindTexForShader( "Tonemapped", "preppedImage", shader, 1 );
	textureManager.BindImageForShader( "Display Texture", "outputImage", shader, 2 );
}

void Daedalus::PrepSphereBufferRandom() {
	// clear it out
	daedalusConfig.render.scene.explicitPrimitiveData.clear();
	palette::PickRandomPalette( true );

	// simple implementation, randomizing all parameters
	rng c = rng(  0.3f, 1.0f );
	rngN o = rngN( 0.0f, 0.125f );
	rng r = rng( 0.1f, 0.15f );
	rng IoR = rng( 1.0f / 1.1f, 1.0f / 1.5f );
	rng Roughness = rng( 0.0f, 0.5f );

	for ( int idx = 0; idx < daedalusConfig.render.scene.numExplicitPrimitives; idx++ ) {
		vec3 color = palette::paletteRef( c() );
		daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( o(), o(), o(), r() * r() ) );			// position
		daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( color.x, color.y, color.z, c() < 0.5f ? 2.0f : 14.0f ) );	// color
		daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( IoR(), Roughness(), 0.0f, 0.0f ) );	// material properties
	}

	vec3 color = vec3( 1.0f );
	daedalusConfig.render.scene.explicitPrimitiveData.clear();
	daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( 0.0f, 0.0f, 0.0f, daedalusConfig.render.scene.marbleRadius ) );			// position
	daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( color.x, color.y, color.z, 14.0f ) );	// color
	daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( IoR(), 0.0f, 0.0f, 0.0f ) );	// material properties
}

void Daedalus::PrepSphereBufferPacking() {
	// TODO

	// // stochastic sphere packing, inside the volume
	// vec3 min = vec3( -8.0f, -1.5f, -8.0f );
	// vec3 max = vec3(  8.0f,  1.5f,  8.0f );
	// uint32_t maxIterations = 500;
	// float currentRadius = 1.3f;
	// rng paletteRefVal = rng( 0.0f, 1.0f );
	// int material = 6;
	// vec4 currentMaterial = vec4( palette::paletteRef( paletteRefVal() ), material );
	// while ( ( sirenConfig.sphereLocationsPlusColors.size() / 2 ) < sirenConfig.maxSpheres ) {
	// 	rng x = rng( min.x + currentRadius, max.x - currentRadius );
	// 	rng y = rng( min.y + currentRadius, max.y - currentRadius );
	// 	rng z = rng( min.z + currentRadius, max.z - currentRadius );
	// 	uint32_t iterations = maxIterations;
	// 	// redundant check, but I think it's the easiest way not to run over
	// 	while ( iterations-- && ( sirenConfig.sphereLocationsPlusColors.size() / 2 ) < sirenConfig.maxSpheres ) {
	// 		// generate point inside the parent cube
	// 		vec3 checkP = vec3( x(), y(), z() );
	// 		// check for intersection against all other spheres
	// 		bool foundIntersection = false;
	// 		for ( uint idx = 0; idx < sirenConfig.sphereLocationsPlusColors.size() / 2; idx++ ) {
	// 			vec4 otherSphere = sirenConfig.sphereLocationsPlusColors[ 2 * idx ];
	// 			if ( glm::distance( checkP, otherSphere.xyz() ) < ( currentRadius + otherSphere.a ) ) {
	// 				// cout << "intersection found in iteration " << iterations << endl;
	// 				foundIntersection = true;
	// 				break;
	// 			}
	// 		}
	// 		// if there are no intersections, add it to the list with the current material
	// 		if ( !foundIntersection ) {
	// 			// cout << "adding sphere, " << currentRadius << endl;
	// 			sirenConfig.sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
	// 			sirenConfig.sphereLocationsPlusColors.push_back( currentMaterial );
	// 			cout << "\r" << fixedWidthNumberString( sirenConfig.sphereLocationsPlusColors.size() / 2, 6, ' ' ) << " / " << sirenConfig.maxSpheres << " after " << Tock() / 1000.0f << "s                          ";
	// 		}
	// 	}
	// 	// if you've gone max iterations, time to halve the radius and double the max iteration count, get new material
	// 		currentMaterial = vec4( palette::paletteRef( paletteRefVal() ), material );
	// 		currentRadius /= 1.618f;
	// 		maxIterations *= 3;

	// 		// doing this makes it pack flat
	// 		// min.y /= 1.5f;
	// 		// max.y /= 1.5f;

	// 		// slowly shrink bounds to accentuate the earlier placed spheres
	// 		min *= 0.95f;
	// 		max *= 0.95f;
	// }

}

void Daedalus::RelaxSphereList() {
	// this only applies to spheres... whatever
	rng o = rng( -0.1f, 0.1f );
	int iterations = 0;
	const int maxIterations = 100;
	while ( 1 ) { // relaxation step
		// walk the list, repulstion force between pairs
		for ( int i = 0; i < daedalusConfig.render.scene.numExplicitPrimitives; i++ ) {
			for ( int j = i + 1; j < daedalusConfig.render.scene.numExplicitPrimitives; j++ ) {
				// sphere i and sphere j move slightly away from one another if intersecting
				vec4 sphereI = daedalusConfig.render.scene.explicitPrimitiveData[ 3 * i ];
				vec4 sphereJ = daedalusConfig.render.scene.explicitPrimitiveData[ 3 * j ];
				float combinedRadius = sphereI.w + sphereJ.w;
				vec3 displacement = sphereI.xyz() - sphereJ.xyz();
				float lengthDisplacement = glm::length( displacement );
				if ( lengthDisplacement < combinedRadius ) {
					const float offset = combinedRadius - lengthDisplacement;
					daedalusConfig.render.scene.explicitPrimitiveData[ 3 * i ] = glm::vec4( sphereI.xyz() + ( offset / 2.0f + o() ) * glm::normalize( displacement ), sphereI.w );
					daedalusConfig.render.scene.explicitPrimitiveData[ 3 * j ] = glm::vec4( sphereJ.xyz() - ( offset / 2.0f + o() ) * glm::normalize( displacement ), sphereJ.w );
				}
			}
		}
		// break out when there are no intersections
		bool existsIntersections = false;
		for ( int i = 0; i < daedalusConfig.render.scene.numExplicitPrimitives; i++ ) {
			for ( int j = i + 1; j < daedalusConfig.render.scene.numExplicitPrimitives; j++ ) {
				vec4 sphereI = daedalusConfig.render.scene.explicitPrimitiveData[ 3 * i ];
				vec4 sphereJ = daedalusConfig.render.scene.explicitPrimitiveData[ 3 * j ];
				float combinedRadius = sphereI.w + sphereJ.w;
				vec3 displacement = sphereI.xyz() - sphereJ.xyz();
				if ( glm::length( displacement ) < combinedRadius ) {
					existsIntersections = true;
				}
			}
		}
		// we have made sure nobody intersects, or something is hanging
		if ( !existsIntersections || ( iterations++ == maxIterations ) ) break;
	}
}

void Daedalus::SendExplicitPrimitiveSSBO() {
	// send the data from the vector to the GPU
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, daedalusConfig.render.scene.explicitPrimitiveSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 4 * 3 * daedalusConfig.render.scene.numExplicitPrimitives,
		( GLvoid * ) &daedalusConfig.render.scene.explicitPrimitiveData[ 0 ], GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, daedalusConfig.render.scene.explicitPrimitiveSSBO );
}

void Daedalus::ClearColorGradingHistogramBuffer() {
	// data is 255 histogram bins, plus a maximum across the field
	constexpr uint32_t bufferContents[ ( 1024 + 4 ) ] = { 0 };
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, daedalusConfig.render.grading.colorHistograms );
	glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLuint ) * ( 1024 + 4 ), ( GLvoid * ) &bufferContents, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, daedalusConfig.render.grading.colorHistograms );
}

void Daedalus::SendWaveformPrepareUniforms() {
	const GLuint shader = shaders[ "Waveform Prepare" ];

	textureManager.BindImageForShader( "Tonemapped", "tonemappedSourceData", shader, 0 );
	textureManager.BindImageForShader( "Waveform Red", "redImage", shader, 1 );
	textureManager.BindImageForShader( "Waveform Green", "greenImage", shader, 2 );
	textureManager.BindImageForShader( "Waveform Blue", "blueImage", shader, 3 );
	textureManager.BindImageForShader( "Waveform Luma", "lumaImage", shader, 4 );
}

void Daedalus::SendWaveformCompositeUniforms( int mode ) {
	GLuint shader;
	if ( mode == 0 ) {
		shader = shaders[ "Parade Composite" ];
		textureManager.BindImageForShader( "Parade Composite", "compositedResult", shader, 5 );
	} else {
		shader = shaders[ "Waveform Composite" ];
		textureManager.BindImageForShader( "Waveform Composite", "compositedResult", shader, 5 );
	}
	textureManager.BindImageForShader( "Waveform Red", "redImage", shader, 1 );
	textureManager.BindImageForShader( "Waveform Green", "greenImage", shader, 2 );
	textureManager.BindImageForShader( "Waveform Blue", "blueImage", shader, 3 );
	textureManager.BindImageForShader( "Waveform Luma", "lumaImage", shader, 4 );
}

void Daedalus::ClearColorGradingWaveformBuffer() {
	// clear out the buffers, images
	const uint32_t numColumns = daedalusConfig.targetWidth;

	static std::vector< uint32_t > minData;
	static std::vector< uint32_t > maxData;
	static std::vector< uint32_t > waveformClear;

	if ( minData.size() != ( numColumns * 4u ) ) {
		minData.resize( numColumns * 4, 255 );
		maxData.resize( numColumns * 4, 0 );
		waveformClear.resize( numColumns * 256, 0 );
	}

	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Waveform Red" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, numColumns, 256, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, waveformClear.data() );
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Waveform Green" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, numColumns, 256, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, waveformClear.data() );
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Waveform Blue" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, numColumns, 256, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, waveformClear.data() );
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Waveform Luma" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, numColumns, 256, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, waveformClear.data() );

	const int numBytes = sizeof( GLuint ) * 4 * numColumns;
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, daedalusConfig.render.grading.waveformMins );
	glBufferData( GL_SHADER_STORAGE_BUFFER, numBytes, ( GLvoid * ) minData.data(), GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, daedalusConfig.render.grading.waveformMins );

	glBindBuffer( GL_SHADER_STORAGE_BUFFER, daedalusConfig.render.grading.waveformMaxs );
	glBufferData( GL_SHADER_STORAGE_BUFFER, numBytes, ( GLvoid * ) maxData.data(), GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, daedalusConfig.render.grading.waveformMaxs );

	glBindBuffer( GL_SHADER_STORAGE_BUFFER, daedalusConfig.render.grading.waveformGlobalMax );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 16, ( GLvoid * ) maxData.data(), GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, daedalusConfig.render.grading.waveformGlobalMax );
}

void Daedalus::SendVectorscopePrepareUniforms() {
	const GLuint shader = shaders[ "Vectorscope Prepare" ];

	textureManager.BindImageForShader( "Tonemapped", "tonemappedSourceData", shader, 0 );
	textureManager.BindImageForShader( "Vectorscope Data", "vectorscopeImage", shader, 1 );
}


void Daedalus::SendVectorscopeCompositeUniforms() {
	const GLuint shader = shaders[ "Vectorscope Composite" ];

	textureManager.BindImageForShader( "Vectorscope Data", "vectorscopeImage", shader, 1 );
	textureManager.BindImageForShader( "Vectorscope Composite", "compositedResult", shader, 2 );
}

void Daedalus::ClearColorGradingVectorscopeBuffer() {
	constexpr uint32_t vectorscopeData[ 512 * 512 ] = { 0 };
	glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Vectorscope Data" ) );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, 512, 512, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, vectorscopeData );

	constexpr uint32_t countValue = 0;
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, daedalusConfig.render.grading.vectorscopeMax );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 5, daedalusConfig.render.grading.vectorscopeMax );
}

void Daedalus::PrepGlyphBuffer() {
	// block dimensions
	uint32_t texW = 128;
	uint32_t texH = 96;
	uint32_t texD = 64;

	Image_4U data( texW, texH * texD );

	// create the voxel model inside the flat image
		// update the data
	const uint32_t numOps = 30000;
	rngi opPick = rngi( 0, 34 );
	rngi xPick = rngi( 0, texW - 1 ); rngi xDPick = rngi( 4, 20 );
	rngi yPick = rngi( 0, texH - 1 ); rngi yDPick = rngi( 4, 12 );
	rngi zPick = rngi( 0, texD - 1 ); rngi zDPick = rngi( 4, 10 );
	rngi glyphPick = rngi( 176, 223 );
	rngi thinPick = rngi( 0, 4 );

	// look at a random, narrow slice of a random palette
	palette::PickRandomPalette( true );
	rng ppPick = rng( 0.1f, 0.9f );
	rng pwPick = rng( 0.01f, 0.3f );
	rngN pPick = rngN( ppPick(), pwPick() );

	color_4U col = color_4U( { 0u, 0u, 0u, 1u } );
	uint32_t length = 0;

	// do N randomly selected
	for ( uint32_t op = 0; op < numOps; op++ ) {
		vec3 c = palette::paletteRef( pPick() ) * 255.0f;
		switch ( opPick() ) {
		case 0: // AABB
		case 1:
		{
			// col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );

			uvec3 base = uvec3( xPick(),  yPick(),  zPick() );
			uvec3 dims = uvec3( xDPick(), yDPick(), zDPick() );
			for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
			for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
			for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
				data.SetAtXY( x, y + z * texH, col );
			break;
		}

		case 2: // lines
		case 3:
		case 4:
		case 5:
		case 6:
		{
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			uvec3 base = uvec3( xPick(), yPick(), zPick() );
			length = xDPick();
			for ( uint32_t x = base.x; x < base.x + length; x++ )
				data.SetAtXY( x, base.y + base.z * texH, col );
			break;
		}

		case 7: // other lines
		case 8:
		case 9:
		case 10:
		case 11:
		{
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			uvec3 base = uvec3( xPick(), yPick(), zPick() );
			length = yDPick();
			for ( uint32_t y = base.y; y < base.y + length; y++ )
				data.SetAtXY( base.x, y + base.z * texH, col );
			break;
		}

		case 12: // more other-er lines
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		{
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			uvec3 base = uvec3( xPick(), yPick(), zPick() );
			length = zDPick();
			for ( uint32_t z = base.z; z < base.z + length; z++ )
				data.SetAtXY( base.x, base.y + z * texH, col );
			break;
		}

		case 18: // should come out emissive
		case 20:
		case 21:
		{
			col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
			// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );

			uvec3 base = uvec3( xPick(), yPick(), zPick() );
			uvec3 dims = uvec3( thinPick() + 1, thinPick() + 1, thinPick() + 1 );
			for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
			for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
			for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
				data.SetAtXY( x, y + z * texH, col );
			break;
		}

		case 22: // string text
		case 23:
		case 24:
		case 25:
		case 26:
		{
			static rngi wordPick = rngi( 0, colorWords.size() - 1 );
			string word = colorWords[ wordPick() ];
			uvec3 basePt = uvec3( xPick(), yPick(), zPick() );
			for ( size_t i = 0; i < word.length(); i++ ) {
				col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) word[ i ] } );
				data.SetAtXY( basePt.x + i, basePt.y + basePt.z * texH, col );
			}
			break;
		}

		case 27:
		case 28:
		case 29:
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		{	// clear out an AABB
			col = color_4U( { 0u, 0u, 0u, 0u } );
			uvec3 base = uvec3( xPick(),  yPick(),  zPick() );
			uvec3 dims = uvec3( xDPick(), yDPick(), zDPick() );
			for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
			for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
			for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
				data.SetAtXY( x, y + z * texH, col );
			break;
		}

		case 35:
		{
			col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
			ivec3 base = ivec3( xPick(),  yPick(),  zPick() );
			data.SetAtXY( base.x, base.y + base.z * texH, col );
			break;
		}

		default:
			break;
		}
	}

	// texture is ready to be used in the pathtrace
		// create a texture, and send the data
	static bool firstRun = true;
	if ( firstRun ) {
		firstRun = false;
		textureOptions_t opts;
		opts.width			= texW;
		opts.height			= texH;
		opts.depth			= texD;
		opts.dataType		= GL_RGBA8UI;
		opts.minFilter		= GL_NEAREST;
		opts.magFilter		= GL_NEAREST;
		opts.textureType	= GL_TEXTURE_3D;
		opts.wrap			= GL_CLAMP_TO_BORDER;
		opts.initialData = ( void * ) data.GetImageDataBasePtr();
		textureManager.Add( "TextBuffer", opts );
	} else {
		// pass the new generated texture data to the existing texture
		glBindTexture( GL_TEXTURE_3D, textureManager.Get( "TextBuffer" ) );
		glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA8UI, texW, texH, texD, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, ( void * ) data.GetImageDataBasePtr() );
	}
}

void Daedalus::GoLTex() {
	// initialize the texture
	std::vector< uint8_t > data;
	data.resize( 512 * 512 * 512 * 4, 255 );

	static bool firstRun = true;
	if ( !firstRun ) {
		textureManager.Remove( "DDATex" );
	}
	firstRun = false;

	textureOptions_t opts;
	opts.width			= 512;
	opts.height			= 512;
	opts.depth			= 512;
	opts.dataType		= GL_RGBA8UI;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_3D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	opts.initialData	= ( void * ) &data[ 0 ];
	textureManager.Add( "DDATex", opts );

	palette::PickRandomPalette( true );
	// vec3 color = palette::paletteRef( 0.5f );

	// overwrite the first slice with noise
	const GLuint tex = textureManager.Get( "DDATex" );
	rng init = rng( 0.0f, 1.0f );
	data.clear();
	data.resize( 512 * 512 * 4, 0 );
	const int www = 1;
	for ( int x = 0; x < 512; x++ ) {
		for ( int y = 0; y < 512; y++ ) {
		// for ( int y = 256 - www; y < 256 + www; y++ ) {

			vec3 color = palette::paletteRef( init() );
			data[ ( x + y * 512 ) * 4 + 0 ] = color.x * 255;
			data[ ( x + y * 512 ) * 4 + 1 ] = color.y * 255;
			data[ ( x + y * 512 ) * 4 + 2 ] = color.z * 255;

			const float d = glm::distance( vec2( float( x ), float( y ) ), vec2( 256.0f ) );
			if ( d < 100.0f && d > 50.0f ) {
				data[ ( x + y * 512 ) * 4 + 3 ] = init() < 0.5f ? 0 : 255;
				// data[ ( x + y * 512 ) * 4 + 3 ] = 255;
			}
		}
	}

	glBindTexture( GL_TEXTURE_3D, tex );
	glTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, 0, 512, 512, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, ( void * ) &data[ 0 ] );

	const GLuint shader = shaders[ "GoL" ];
	glUseProgram( shader );
	textureManager.BindImageForShader( "DDATex", "ddatex", shader, 1 );

	// then evaluate the update, slices up the z axis
	for ( int i = 0; i < 512; i++ ) {
		// uniform var, i, indicating current generation's slice
		glUniform1i( glGetUniformLocation( shader, "previousSlice" ), i );

		// dispatch the compute shader to update the slice
		glDispatchCompute( 512 / 16, 512 / 16, 1 );

		// memory barrier
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
}

void Daedalus::DDAVATTex() {
	palette::PickRandomPalette( true );
	// static glm::vec4 color0 = glm::vec4( palette::paletteRef( 0.2f ), 0.0f );
	// static glm::vec4 color1 = glm::vec4( palette::paletteRef( 0.5f ), 1.0f );
	// static glm::vec4 color2 = glm::vec4( palette::paletteRef( 0.8f ), 1.0f );

	rng jitter = rng( -0.1f, 0.1f );
	rng alphaOffset = rng( 0.0f, 0.5f );

	// would be nice to provide some of these functions
	static float lambda = 0.35f;
	static float beta = 0.5f;
	static float mag = 0.0f;
	static int initMode = 3;
	static float flip = 0.1;
	static char inputString[ 256 ] = "";
	static bool plusX = false, plusY = false, plusZ = false;
	static bool minusX = false, minusY = true, minusZ = false;

	#define BLOCKDIM 512
	voxelAutomataTerrain vR( 9, flip, string( "r" ), initMode, lambda, beta, mag, glm::bvec3( minusX, minusY, minusZ ), glm::bvec3( plusX, plusY, plusZ ) );
	strcpy( inputString, vR.getShortRule().c_str() );
	std::vector< uint8_t > loaded;
	loaded.reserve( BLOCKDIM * BLOCKDIM * BLOCKDIM * 4 );
	for ( int x = 0; x < BLOCKDIM; x++ ) {
		for ( int y = 0; y < BLOCKDIM; y++ ) {
			for ( int z = 0; z < BLOCKDIM; z++ ) {
				glm::vec4 color;
				switch ( vR.state[ x ][ y ][ z ] ) {
					// case 0: color = color0; break;
					// case 1: color = color1; break;
					// case 2: color = color2; break;
					// default: color = color0; break;
					case 0: color = glm::vec4( palette::paletteRef( 0.2f + jitter() ), 0.0f ); break;
					case 1: color = glm::vec4( palette::paletteRef( 0.5f + jitter() ), 0.1f ); break;
					case 2: color = glm::vec4( palette::paletteRef( 0.8f + jitter() ), 0.2f + alphaOffset() ); break;
					// default: color = color0; break;
				}
				loaded.push_back( static_cast<uint8_t>( color.x * 255.0 ) );
				loaded.push_back( static_cast<uint8_t>( color.y * 255.0 ) );
				loaded.push_back( static_cast<uint8_t>( color.z * 255.0 ) );
				loaded.push_back( static_cast<uint8_t>( color.w * 255.0 ) );
			}
		}
	}
	static bool firstRun = true;
	if ( !firstRun ) {
		textureManager.Remove( "DDATex" );
	}
	firstRun = false;

	textureOptions_t opts;
	opts.width			= BLOCKDIM;
	opts.height			= BLOCKDIM;
	opts.depth			= BLOCKDIM;
	opts.dataType		= GL_RGBA8UI;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_3D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	opts.initialData	= ( void * ) &loaded[ 0 ];
	textureManager.Add( "DDATex", opts );

}

void Daedalus::HeightmapTex() {
	particleEroder p;
	p.InitWithDiamondSquare();
	// Image_1F::rangeRemapInputs_t remap;
	// p.model.RangeRemap( &remap );

	int numSteps = 100;
	for ( int i = 0; i <= numSteps; i++ ) {
		cout << "\rRunning eroder: " << i << " / " << numSteps << std::flush;
		p.Erode( 1000 );
	}

	cout << endl;
	// p.Save( "test.png" );

	static bool firstRun = true;
	if ( !firstRun ) {
		textureManager.Remove( "HeightmapTex" );
	}

	firstRun = false;
	textureOptions_t opts;
	opts.width			= p.model.Width();
	opts.height			= p.model.Height();
	opts.dataType		= GL_R32F;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	opts.pixelDataType	= GL_FLOAT;
	opts.initialData	= ( void * ) &p.model.GetImageDataBasePtr()[ 0 ];
	textureManager.Add( "HeightmapTex", opts );
}

void Daedalus::LoadSkyBoxEXRFromString( const string label ) {
	textureManager.Remove( "Sky Cache" );

	Image_4F loadedImage( label, Image_4F::backend::TINYEXR );

	textureOptions_t opts;
	opts.width			= loadedImage.Width();
	opts.height			= loadedImage.Height();
	opts.dataType		= GL_RGBA32F;
	opts.minFilter		= GL_LINEAR;
	opts.magFilter		= GL_LINEAR;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	opts.pixelDataType	= GL_FLOAT;
	opts.initialData	= ( void * ) loadedImage.GetImageDataBasePtr();
	textureManager.Add( "Sky Cache", opts );
}