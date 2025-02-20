struct line {
	vec4 p0, p1;	// width in .a?
	vec4 color0, color1;
};

struct LineSpamConfig_t {
	ivec2 dimensions = ivec2( 1920, 1080 );

	textureManager_t * textureManager;

	GLuint opaqueLineSSBO;
	GLuint transparentLineSSBO;

	GLuint opaquePreZShader;
	GLuint opaqueDrawShader;
	GLuint transparentDrawShader;
	GLuint compositeShader;
	GLuint clearShader;

	void CreateTextures();
	void CompileShaders();
	void PrepLineBuffers();

	void UpdateTransform( inputHandler_t &input );
	void ClearPass();
	void OpaquePass();
	void TransparentPass();
	void CompositePass();

	bool dirty = true;
	mat4 transform = mat4( 1.0f );
	vector< line > lines;
	vector< line > opaqueLines;
	vector< line > transparentLines;
	void AddLine( line l ) { dirty = true; lines.push_back( l ); }

	float depthRange = 2.0f;
};

void LineSpamConfig_t::CreateTextures() {
	textureOptions_t opts;
	opts.dataType		= GL_R32UI;
	opts.width			= dimensions.x;
	opts.height			= dimensions.y;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;

	// depth buffer, uint
	textureManager->Add( "Depth Buffer", opts );

	// id buffer, uint
	textureManager->Add( "ID Buffer", opts );

	// r, g, b, count tallies, uint
	textureManager->Add( "Red Tally Buffer", opts );
	textureManager->Add( "Green Tally Buffer", opts );
	textureManager->Add( "Blue Tally Buffer", opts );
	textureManager->Add( "Sample Tally Buffer", opts );

	// color buffer, RGB color
	opts.dataType		= GL_RGBA16F;
	opts.magFilter		= GL_LINEAR;
	textureManager->Add( "Composite Target", opts );
}

void LineSpamConfig_t::CompileShaders() {
	const string basePath = string( "../src/projects/LineSpam/shaders/" );
	opaquePreZShader		= computeShader( basePath + "opaquePreZ.cs.glsl" ).shaderHandle;
	opaqueDrawShader		= computeShader( basePath + "opaqueDraw.cs.glsl" ).shaderHandle;
	transparentDrawShader	= computeShader( basePath + "transparentDraw.cs.glsl" ).shaderHandle;
	compositeShader			= computeShader( basePath + "composite.cs.glsl" ).shaderHandle;
	clearShader				= computeShader( basePath + "clear.cs.glsl" ).shaderHandle;
}

void LineSpamConfig_t::PrepLineBuffers() {
	// unset flag
	dirty = false;

	// sorting the list of lines into opaque and transparent
	opaqueLines.clear();
	// opaqueLines.reserve( lines.size() );
	transparentLines.clear();
	// transparentLines.reserve( lines.size() );
	for ( auto& l : lines ) {
		if ( l.color1.a == 1.0f ) {
			opaqueLines.push_back( l );
		} else {
			transparentLines.push_back( l );
		}
	}

	// create the buffers
	glGenBuffers( 1, &opaqueLineSSBO );
	glGenBuffers( 1, &transparentLineSSBO );

	glBindBuffer( GL_SHADER_STORAGE_BUFFER, opaqueLineSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 4 * sizeof( vec4 ) * opaqueLines.size(), ( void * ) opaqueLines.data(), GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, opaqueLineSSBO );
	glObjectLabel( GL_BUFFER, opaqueLineSSBO, -1, string( "Opaque Line Data" ).c_str() );

	glBindBuffer( GL_SHADER_STORAGE_BUFFER, transparentLineSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 4 * sizeof( vec4 ) * transparentLines.size(), ( void * ) transparentLines.data(), GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, transparentLineSSBO );
	glObjectLabel( GL_BUFFER, transparentLineSSBO, -1, string( "Transparent Line Data" ).c_str() );
}

void LineSpamConfig_t::UpdateTransform( inputHandler_t &input ) {
	// transform = glm::mat4_cast( glm::angleAxis( 0.01f * sin( SDL_GetTicks() / 160.0f ), normalize( vec3( 8.0f, 1.0f, 7.0f ) ) ) * glm::angleAxis( 0.4f * sin( SDL_GetTicks() / 16000.0f ), normalize( vec3( 1.0f, 2.0f, 3.0f ) ) ) );

	// transform = glm::rotate( SDL_GetTicks() / 16000.0f, normalize( vec3( 0.0f, 1.0f, 0.0f ) ) )
	// 			* glm::rotate( sin( SDL_GetTicks() / 12000.0f ), normalize( vec3( 0.0f, 0.0f, 1.0f ) ) )
	// 			* glm::rotate( cos( SDL_GetTicks() / 1000.0f ), normalize( vec3( 1.0f, 0.0f, 0.0f ) ) );

	const bool shift = input.getState( KEY_LEFT_SHIFT ) || input.getState( KEY_RIGHT_SHIFT );
	const bool control = input.getState( KEY_LEFT_CTRL ) || input.getState( KEY_RIGHT_CTRL );

	const float scalar = shift ? 0.1f : ( control ? 0.0005f : 0.02f );

	if ( input.getState( KEY_W ) )			transform = glm::rotate( scalar, vec3( 1.0f, 0.0f, 0.0f ) ) * transform;
	if ( input.getState( KEY_S ) )			transform = glm::rotate( -scalar, vec3( 1.0f, 0.0f, 0.0f ) ) * transform;
	if ( input.getState( KEY_A ) )			transform = glm::rotate( -scalar, vec3( 0.0f, 1.0f, 0.0f ) ) * transform;
	if ( input.getState( KEY_D ) )			transform = glm::rotate( scalar, vec3( 0.0f, 1.0f, 0.0f ) ) * transform;
	if ( input.getState( KEY_Q ) )			transform = glm::rotate( scalar, vec3( 0.0f, 0.0f, 1.0f ) ) * transform;
	if ( input.getState( KEY_E ) )			transform = glm::rotate( -scalar, vec3( 0.0f, 0.0f, 1.0f ) ) * transform;
	if ( input.getState( KEY_UP ) )		transform = glm::translate( scalar * vec3( 0.0f, 0.0f, 1.0f ) ) * transform;
	if ( input.getState( KEY_DOWN ) )		transform = glm::translate( scalar * vec3( 0.0f, 0.0f, -1.0f ) ) * transform;
	if ( input.getState( KEY_RIGHT ) )		transform = glm::translate( scalar * vec3( 1.0f, 0.0f, 0.0f ) ) * transform;
	if ( input.getState( KEY_LEFT ) )		transform = glm::translate( scalar * vec3( -1.0f, 0.0f, 0.0f ) ) * transform;
	if ( input.getState( KEY_PAGEDOWN ) )	transform = glm::translate( scalar * vec3( 0.0f, 1.0f, 0.0f ) ) * transform;
	if ( input.getState( KEY_PAGEUP ) )	transform = glm::translate( scalar * vec3( 0.0f, -1.0f, 0.0f ) ) * transform;

	if ( input.getState4( KEY_F ) == KEYSTATE_RISING ) transform = mat4( 1.0f );

	if ( input.getState( KEY_EQUALS ) )	transform = glm::scale( vec3( 0.99f ) ) * transform; // zoom in
	if ( input.getState( KEY_MINUS ) )		transform = glm::scale( vec3( 1.0f / 0.99f ) ) * transform; // zoom out
}

void LineSpamConfig_t::ClearPass() {
	glUseProgram( clearShader );
	textureManager->BindImageForShader( "Red Tally Buffer", "redTally", clearShader, 0 );
	textureManager->BindImageForShader( "Green Tally Buffer", "greenTally", clearShader, 1 );
	textureManager->BindImageForShader( "Blue Tally Buffer", "blueTally", clearShader, 2 );
	textureManager->BindImageForShader( "Sample Tally Buffer", "sampleTally", clearShader, 3 );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", clearShader, 4 );
	textureManager->BindImageForShader( "ID Buffer", "idBuffer", clearShader, 5 );
	glDispatchCompute( ( dimensions.x + 15 ) / 16, ( dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void LineSpamConfig_t::OpaquePass() {
	// opaque pre z draw, establish closest depths per pixel
	glUseProgram( opaquePreZShader );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", opaquePreZShader, 0 );
	glUniformMatrix4fv( glGetUniformLocation( opaquePreZShader, "transform" ), 1, false, glm::value_ptr( transform ) );
	glUniform1ui( glGetUniformLocation( opaquePreZShader, "numOpaque" ), opaqueLines.size() );
	glUniform1f( glGetUniformLocation( opaquePreZShader, "depthRange" ), depthRange );
	const int workgroupsRoundedUp = ( opaqueLines.size() + 63 ) / 64;
	glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

	// opaque draw, depth equals, setting ID values for composite
	glUseProgram( opaqueDrawShader );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", opaqueDrawShader, 0 );
	textureManager->BindImageForShader( "ID Buffer", "idBuffer", opaqueDrawShader, 1 );
	glUniformMatrix4fv( glGetUniformLocation( opaqueDrawShader, "transform" ), 1, false, glm::value_ptr( transform ) );
	glUniform1ui( glGetUniformLocation( opaqueDrawShader, "numOpaque" ), opaqueLines.size() );
	glUniform1f( glGetUniformLocation( opaqueDrawShader, "depthRange" ), depthRange );
	glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void LineSpamConfig_t::TransparentPass() {
	// transparent draw process
	glUseProgram( transparentDrawShader );
	textureManager->BindImageForShader( "Red Tally Buffer", "redTally", transparentDrawShader, 0 );
	textureManager->BindImageForShader( "Green Tally Buffer", "greenTally", transparentDrawShader, 1 );
	textureManager->BindImageForShader( "Blue Tally Buffer", "blueTally", transparentDrawShader, 2 );
	textureManager->BindImageForShader( "Sample Tally Buffer", "sampleTally", transparentDrawShader, 3 );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", transparentDrawShader, 4 );
	glUniformMatrix4fv( glGetUniformLocation( transparentDrawShader, "transform" ), 1, false, glm::value_ptr( transform ) );
	glUniform1ui( glGetUniformLocation( transparentDrawShader, "numTransparent" ), transparentLines.size() );
	glUniform1f( glGetUniformLocation( transparentDrawShader, "depthRange" ), depthRange );
	const int workgroupsRoundedUp = ( transparentLines.size() + 63 ) / 64;
	glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void LineSpamConfig_t::CompositePass() {
	// this will also do the buffer clears, since it's the only consumer of the other buffer info
	glUseProgram( compositeShader );
	textureManager->BindImageForShader( "Red Tally Buffer", "redTally", compositeShader, 0 );
	textureManager->BindImageForShader( "Green Tally Buffer", "greenTally", compositeShader, 1 );
	textureManager->BindImageForShader( "Blue Tally Buffer", "blueTally", compositeShader, 2 );
	textureManager->BindImageForShader( "Sample Tally Buffer", "sampleTally", compositeShader, 3 );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", compositeShader, 4 );
	textureManager->BindImageForShader( "ID Buffer", "idBuffer", compositeShader, 5 );
	textureManager->BindImageForShader( "Composite Target", "compositedResult", compositeShader, 6 );
	glUniform1f( glGetUniformLocation( compositeShader, "depthRange" ), depthRange );
	static rngi wangSeed = rngi( 0, 1000000 );
	glUniform1i( glGetUniformLocation( compositeShader, "seedOffset" ), wangSeed() );
	glDispatchCompute( ( dimensions.x + 15 ) / 16, ( dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}