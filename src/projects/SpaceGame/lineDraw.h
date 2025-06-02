#include "engine.h"

#ifndef LINEDRAW_H
#define LINEDRAW_H

// LineDrawer class handles the line drawing pipeline, managing SSBO updates,
// drawing batches of lines, and carrying out color passes for compositing.

struct Line {
	ivec2 start; // Start point
	ivec2 end;   // End point

	Line ( ivec2 s, ivec2 e ) : start( s ), end( e ) {}
};

class LineDrawer {
private:
	GLuint ssbo = 0; // SSBO for line data
	struct LineBatch {
		vector< Line > lines;   // Line collection
		vec3 color = vec3( 1.0f ); // Default color
		size_t offset = 0;         // Offset of this batch in SSBO
	};
	array< LineBatch, 8 > passes; // Up to 8 independent color passes
	bool ssboDirty = false; // Track when data in the SSBO needs updating
	size_t totalLines = 0;  // Total number of lines across all passes
	GLuint lineShader = 0;  // Shader for line rendering
	GLuint compositeShader = 0; // Composite rendering shader
	GLuint clearShader = 0; // Shader for clearing intermediate buffer
	textureManager_t* textureManager = nullptr;

	void UpdateSSBO () {
		if ( !ssboDirty || totalLines == 0 ) { // Skip if no changes
			return;
		}

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, totalLines * sizeof( ivec4 ), nullptr, GL_DYNAMIC_DRAW );
		ivec4* mappedBuffer = ( ivec4* ) glMapBufferRange( GL_SHADER_STORAGE_BUFFER, 0, totalLines * sizeof( ivec4 ), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );

		size_t offset = 0;
		for ( LineBatch& batch : passes ) {
			batch.offset = offset;
			for ( Line& line : batch.lines ) {
				mappedBuffer[ offset++ ] = ivec4( line.start, line.end ); // Add lines to SSBO
			}
		}

		glUnmapBuffer( GL_SHADER_STORAGE_BUFFER );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );

		ssboDirty = false;
	}

	void ClearIntermediateBuffer () {
		// Add code here to clear the intermediate buffer if necessary
		glUseProgram( clearShader );
		glDispatchCompute( ( width + 15 ) / 16, ( height + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}

	int width = 0, height = 0;

	void ExecutePass ( const LineBatch& pass ) {
		if ( pass.lines.empty() ) return;

		// Bind SSBO for shader processing
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, ssbo );

		// Pass rendering with line shader
		glUseProgram( lineShader );
		textureManager->BindImageForShader( "Line Draw Buffer", "lineIntermediateBuffer", lineShader, 0 );
		glUniform1i( glGetUniformLocation( lineShader, "offset" ), pass.offset );
		// two rounding operations... could do as a single, 4096, whatever
		glDispatchCompute( 64, std::max( ( uint32_t( pass.lines.size() + 63 ) / 64 + 63 ) / 64, 1U ), 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// Composite pass
		glUseProgram( compositeShader );
		glUniform3f( glGetUniformLocation( compositeShader, "color" ), pass.color.r, pass.color.g, pass.color.b );
		textureManager->BindImageForShader( "Line Draw Buffer", "lineIntermediateBuffer", compositeShader, 0 );
		textureManager->BindTexForShader( "Accumulator", "accumulatorTexture", compositeShader, 1 );
		glDispatchCompute( ( width + 15 ) / 16, ( height + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// Clear buffer after use
		ClearIntermediateBuffer();
	}

public:
	LineDrawer () = default;

	~LineDrawer () {
		if ( ssbo ) {
			glDeleteBuffers( 1, &ssbo );
		}
	}

	void Init ( GLuint lineShaderProgram, GLuint compositeShaderProgram, GLuint clearShaderProgram, textureManager_t& manager, int w, int h ) {
		this->lineShader = lineShaderProgram;
		this->compositeShader = compositeShaderProgram;
		this->clearShader = clearShaderProgram;
		this->textureManager = &manager;
		width = w;
		height = h;

		// Create the SSBO
		if ( ssbo == 0 ) {
			glCreateBuffers( 1, &ssbo );
			glObjectLabel( GL_BUFFER, ssbo, -1, "Line Drawer SSBO" );
		}
	}

	void AddLine ( int passIndex, ivec2 start, ivec2 end ) {
		if ( passIndex < 0 || passIndex >= passes.size() ) return;
		passes[ passIndex ].lines.emplace_back( start, end );
		ssboDirty = true;
		totalLines++;
	}

	void SetPassColor ( int passIndex, vec3 color ) {
		if ( passIndex < 0 || passIndex >= passes.size() ) return;
		passes[ passIndex ].color = color;
	}

	void Update () {
		UpdateSSBO(); // Update SSBO if needed

		for ( const LineBatch& pass : passes ) {
			ExecutePass( pass );
		}

		ClearAllPasses(); // Clear lines after rendering
	}

	void ClearAllPasses () {
		for ( LineBatch& batch : passes ) {
			batch.lines.clear();
			batch.offset = 0;
		}
		ssboDirty = true;
		totalLines = 0;
	}
};


#endif //LINEDRAW_H
