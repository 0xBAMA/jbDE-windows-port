#include "../../engine/includes.h"
#ifndef TRIDENT_H
#define TRIDENT_H

// I had always heard this referred to as a gizmo - but looking for reference
//   images for this implementation, I found some package that was on sale for
//   second life calling it an 'orient trident' and now I am absolutely calling
//   it that. This will also be tracking the state of the basis vectors used by
//   the renderer + providing the rotation functions used by the controls.

class orientTrident {
public:
	orientTrident () {
		InitBasis();
		show = true;
	}

	void toggleShow () {
		show = !show;
	}

	void PassInShaders ( GLuint generate, GLuint copy ) {
		generateShader = generate;
		copyShader = copy;
	}

	void PassInImage ( GLuint textureHandle ) {
		tridentImage = textureHandle;
	}

	void Update ( GLuint writeTarget ) {

		// bind reqiured textures
		glBindImageTexture( 0, tridentImage, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );
		glBindImageTexture( 1, writeTarget, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI );

		static int previousModeSelect = modeSelect;
		if ( previousModeSelect != modeSelect ) {
			needsRedraw = true;
			previousModeSelect = modeSelect;
		}

		if ( needsRedraw ) { // do the raymarch
			// while relatively cheap, this is still the most expensive part of this
			//   operation, so the idea is to write the result to a texture and copy
			//   that until the state changes and that is no longer good data
			glUseProgram( generateShader );
			glUniform3fv( glGetUniformLocation( generateShader, "basisX" ), 1, glm::value_ptr( basisX ) );
			glUniform3fv( glGetUniformLocation( generateShader, "basisY" ), 1, glm::value_ptr( basisY ) );
			glUniform3fv( glGetUniformLocation( generateShader, "basisZ" ), 1, glm::value_ptr( basisZ ) );
			glUniform1i( glGetUniformLocation( generateShader, "mode" ), modeSelect );
			glDispatchCompute( blockDimensions.x, blockDimensions.y, 1 );
			needsRedraw = false;
		}

		glUseProgram( copyShader );
		glUniform2iv( glGetUniformLocation( copyShader, "basePt" ), 1, glm::value_ptr( basePt ) );
		glDispatchCompute( blockDimensions.x, blockDimensions.y, 1 );
	}

	glm::vec3 basisX;
	glm::vec3 basisY;
	glm::vec3 basisZ;

	// want something to set this from the application settings
	int modeSelect = 1; // 0 is the menger sponge, 1 is the ball

	glm::ivec2 basePt;

	// size in terms of font glyphs - sizes the dispatches
	glm::ivec2 blockDimensions{ 20, 7 };

	void InitBasis () {
		basisX = glm::vec3( 1.0f, 0.0f, 0.0f );
		basisY = glm::vec3( 0.0f, 1.0f, 0.0f );
		basisZ = glm::vec3( 0.0f, 0.0f, 1.0f );
		needsRedraw = true;
	}

	// rotate functions X,Y,Z with an amount to rotate by ( pos/neg )
		// each of these will set needsRedraw to true, since the state changes

	void RotateX ( float amnt ) {
		basisX = glm::rotate( basisX, amnt, glm::vec3( 1.0f, 0.0f, 0.0f ) );
		basisY = glm::rotate( basisY, amnt, glm::vec3( 1.0f, 0.0f, 0.0f ) );
		basisZ = glm::rotate( basisZ, amnt, glm::vec3( 1.0f, 0.0f, 0.0f ) );
		needsRedraw = true;
	}

	void RotateY ( float amnt ) {
		basisX = glm::rotate( basisX, amnt, glm::vec3( 0.0f, 1.0f, 0.0f ) );
		basisY = glm::rotate( basisY, amnt, glm::vec3( 0.0f, 1.0f, 0.0f ) );
		basisZ = glm::rotate( basisZ, amnt, glm::vec3( 0.0f, 1.0f, 0.0f ) );
		needsRedraw = true;
	}

	void RotateZ ( float amnt ) {
		basisX = glm::rotate( basisX, amnt, glm::vec3( 0.0f, 0.0f, 1.0f ) );
		basisY = glm::rotate( basisY, amnt, glm::vec3( 0.0f, 0.0f, 1.0f ) );
		basisZ = glm::rotate( basisZ, amnt, glm::vec3( 0.0f, 0.0f, 1.0f ) );
		needsRedraw = true;
	}

	void setDirty () {
		needsRedraw = true;
	}

	void SetViewFront () {
		InitBasis();
	}

	void SetViewBack () {
		InitBasis();
		RotateY( jbDE::pi );
	}

	void SetViewRight () {
		InitBasis();
		RotateY( jbDE::pi / 2.0 );
	}

	void SetViewLeft () {
		InitBasis();
		RotateY( -jbDE::pi / 2.0 );
	}

	void SetViewUp () {
		InitBasis();
		RotateX( -jbDE::pi / 2.0 );
	}

	void SetViewDown () {
		InitBasis();
		RotateX( jbDE::pi / 2.0 );
	}

	bool show;

	bool Dirty () {
		return needsRedraw;
	}

	void ClearDirty () {
		needsRedraw = false;
	}

private:
	GLuint tridentImage;
	GLuint generateShader;
	GLuint copyShader;
	bool needsRedraw; // generate shader needs to run again with new values
};

#endif
