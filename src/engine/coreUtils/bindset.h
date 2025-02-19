/*==============================================================================
need to handle the binding of images to the binding points for each shader
	- this will grow to include both image and texture usage
		*** texture manager will need to change to make use of this stuff, too

		- in the case where I need to get the shader program handle, I can do a
		glGet with GL_CURRENT_PROGRAM, which will give me the last thing that
		got set up with glUseProgram - this is an easy way to keep the interface
		the same, keeps from having to do anything to different to pass in the
		information
			- bindsets can be then program-agnostic, kind of, they might need
			to know that it's the same name shared both sides of the bus, or
			something like that
==============================================================================*/
inline GLuint GetCurrentProgram () {
	GLint val;
	glGetIntegerv( GL_CURRENT_PROGRAM, &val );
	return val;
}

struct binding {
	binding( int bp, GLuint te, GLenum ty ) : bindPoint( bp ), texture( te ), type( ty ) {}
	int bindPoint;
	GLuint texture;
	GLenum type;
};

struct bindSet {
	bindSet () {}
	bindSet ( std::vector< binding > bs ) : bindings( bs ) {}
	std::vector< binding > bindings;
	void apply () {
		for ( auto b : bindings ) {
			glBindImageTexture( b.bindPoint, b.texture, 0, GL_TRUE, 0, GL_READ_WRITE, b.type );
		}
	}
};
