#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H

#include "../includes.h"

//===== Helper Functions ==============================================================================================
inline size_t bytesPerPixel ( GLint type ) {
	// over time we'll accumulate all the ones that I use, this currently is sufficient
	switch ( type ) {
	// one channel formats
	case GL_R8UI:				return 1; break;
	case GL_DEPTH_COMPONENT16:	return 1 * 2; break;

	case GL_R32I:
	case GL_R32UI:
	case GL_R32F:
	case GL_DEPTH_COMPONENT32:	return 1 * 4; break;

	// two channel formats
	case GL_RG32UI:				return 2 * 4; break;

	// three channel formats
	case GL_RGB8:				return 3 * 1; break;
	case GL_RGB32F:				return 3 * 4; break;

	// four channel formats
	case GL_RGBA8:
	case GL_RGBA8UI:			return 4 * 1; break;

	case GL_RGBA16F:			return 4 * 2; break;

	case GL_RGBA32UI:
	case GL_RGBA32F:			return 4 * 4; break;

	default:
		cout << "unknown type texture created" << endl;
		return 0;
		break;
	}
}

inline GLenum getFormat ( GLint internalFormat ) {
	switch ( internalFormat ) { // hitting the commonly used formats

	// depth formats
	case GL_DEPTH_COMPONENT:
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32:
		return GL_DEPTH_COMPONENT;
		break;

	// one-channel formats
	case GL_R8:
	case GL_R16:
	case GL_R16F:
	case GL_R32F:
		return GL_RED;
		break;

	case GL_R8I:
	case GL_R8UI:
	case GL_R16I:
	case GL_R16UI:
	case GL_R32I:
	case GL_R32UI:
		return GL_RED_INTEGER;
		break;

	// two-channel formats
	case GL_RG8:
	case GL_RG16:
	case GL_RG16F:
	case GL_RG32F:
		return GL_RG;
		break;

	case GL_RG8I:
	case GL_RG8UI:
	case GL_RG16I:
	case GL_RG16UI:
	case GL_RG32I:
	case GL_RG32UI:
		return GL_RG_INTEGER;
		break;

	// three-channel formats
	case GL_RGB4:
	case GL_RGB5:
	case GL_RGB8:
	case GL_RGB12:
	case GL_SRGB8:
	case GL_RGB16F:
	case GL_RGB32F:
	case GL_R11F_G11F_B10F:
	case GL_RGB9_E5:
		return GL_RGB;
		break;

	case GL_RGB8I:
	case GL_RGB8UI:
	case GL_RGB16I:
	case GL_RGB16UI:
	case GL_RGB32I:
	case GL_RGB32UI:
		return GL_RGB_INTEGER;
		break;

	// four-channel formats
	case GL_RGBA2:
	case GL_RGBA4:
	case GL_RGBA8:
	case GL_RGBA12:
	case GL_RGBA16:
	case GL_RGBA16F:
	case GL_RGBA32F:
		return GL_RGBA;
		break;

	case GL_RGBA8I:
	case GL_RGBA8UI:
	case GL_RGBA16I:
	case GL_RGBA16UI:
	case GL_RGBA32I:
	case GL_RGBA32UI:
		return GL_RGBA_INTEGER;
		break;

	default:
		return 0;
		break;

	}
}

inline string getTypeString ( GLint internalFormat ) {
	string value;
	switch ( internalFormat ) { // hitting the commonly used formats

	case GL_SRGB8:
		value = "norm sRGB"; break;

	case GL_RGB5:
	case GL_R11F_G11F_B10F:
	case GL_RGB9_E5:
	case GL_RGBA2:
	case GL_RGB4:
	case GL_RGBA4:
	case GL_R8:
	case GL_RG8:
	case GL_RGB8:
	case GL_RGBA8:
	case GL_RGB12:
	case GL_RGBA12:
	case GL_R16:
	case GL_RG16:
	case GL_RGBA16:
		value = "norm"; break;

	case GL_R16F:
	case GL_RG16F:
	case GL_RGB16F:
	case GL_RGBA16F:
	case GL_R32F:
	case GL_RG32F:
	case GL_RGB32F:
	case GL_RGBA32F:
		value = "float"; break;

	case GL_DEPTH_COMPONENT:
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32:
		value = "depth"; break;

	case GL_R8I:
	case GL_RG8I:
	case GL_RGB8I:
	case GL_RGBA8I:
	case GL_R16I:
	case GL_RG16I:
	case GL_RGB16I:
	case GL_RGBA16I:
	case GL_R32I:
	case GL_RG32I:
	case GL_RGB32I:
	case GL_RGBA32I:
		value = "int"; break;

	case GL_R8UI:
	case GL_RG8UI:
	case GL_RGB8UI:
	case GL_RGBA8UI:
	case GL_R16UI:
	case GL_RG16UI:
	case GL_RGB16UI:
	case GL_RGBA16UI:
	case GL_R32UI:
	case GL_RG32UI:
	case GL_RGB32UI:
	case GL_RGBA32UI:
		value = "uint"; break;

	default: break;
	}
	return value;
}

inline int getBitCount ( GLint internalFormat ) {
	int value;
	switch ( internalFormat ) { // hitting the commonly used formats

	// three-channel formats
	case GL_RGB5:
		value = 5; break;
	case GL_SRGB8:
		value = 8; break;

		// these are... yeah
	case GL_R11F_G11F_B10F:
		value = 11; break;
	case GL_RGB9_E5:
		value = 9; break;

	// four-channel formats
	case GL_RGBA2:
		value = 2; break;
	case GL_RGB4:
	case GL_RGBA4:
		value = 4; break;
	case GL_R8:
	case GL_RG8:
	case GL_RGB8:
	case GL_RGBA8:
	case GL_R8I:
	case GL_RG8I:
	case GL_RGB8I:
	case GL_RGBA8I:
	case GL_R8UI:
	case GL_RG8UI:
	case GL_RGB8UI:
	case GL_RGBA8UI:
		value = 8; break;
	case GL_RGB12:
	case GL_RGBA12:
		value = 12; break;
	case GL_R16:
	case GL_RG16:
	case GL_RGBA16:
	case GL_R16F:
	case GL_RG16F:
	case GL_RGB16F:
	case GL_RGBA16F:
	case GL_DEPTH_COMPONENT:
	case GL_DEPTH_COMPONENT16:
	case GL_R16I:
	case GL_RG16I:
	case GL_RGB16I:
	case GL_RGBA16I:
	case GL_R16UI:
	case GL_RG16UI:
	case GL_RGB16UI:
	case GL_RGBA16UI:
		value = 16; break;

	case GL_DEPTH_COMPONENT24:
		value = 24; break;
	case GL_R32F:
	case GL_RG32F:
	case GL_RGB32F:
	case GL_RGBA32F:
	case GL_DEPTH_COMPONENT32:
	case GL_R32I:
	case GL_RG32I:
	case GL_RGB32I:
	case GL_RGBA32I:
	case GL_R32UI:
	case GL_RG32UI:
	case GL_RGB32UI:
	case GL_RGBA32UI:
		value = 32; break;

	default: break;
	}
	return value;
}



//===== Texture Options ===============================================================================================
// data required to create a texture
struct textureOptions_t {
	// data type
	GLint dataType;

	// texture type
	GLenum textureType;

// dimensions
	// x, y, z, layers
	GLsizei width = 0;
	GLsizei height = 0;
	GLsizei depth = 1;	// will become relevant with 3d textures
	GLsizei layers = 1;	// not technically correctly handled right now, ignores value... 2D array texture is using depth, not layers...

// filtering - default to nearest filtering
	GLint minFilter = GL_NEAREST;
	GLint magFilter = GL_NEAREST;

// wrap mode - they let you specifiy it different for different axes, but I don't ever use that
	GLint wrap = GL_CLAMP_TO_BORDER;
	vec4 borderColor = { 0.0f, 0.0f, 0.0f, 1.0f };

	// initial image data, for loading images
	void * initialData = nullptr;

	// data passed type, if relevant
	GLenum pixelDataType = GL_UNSIGNED_BYTE;
};


//===== Texture Record ================================================================================================
struct texture_t {
	string label;			// identifier for the texture

	// keep the information used to create the texture
	textureOptions_t creationOptions;

	GLuint textureHandle;	// from glGenTextures()
	size_t textureSize;		// number of bytes on disk
};

//===== Texture Manager ===============================================================================================
class textureManager_t {
public:
	// prevent the use of 4.5 features
	const bool compatibilityMode = false;

	// report some OpenGL constants' values
	const bool statsReport = false;

	void Init () {

		if ( statsReport ) {

			//report some platform details:
			//	number of available texture units
			//	maximum dimensions of texture
			//	...

			GLint val;
			glGetIntegerv( GL_MAX_TEXTURE_SIZE, &val );
			cout << endl << endl << "\t\tMax Texture Size Reports: " << val << endl;

			cout << "\t\t  So Max Texture MIP Level Should Be " << log2( float( val ) ) << endl;

			glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &val );
			cout << "\t\tMax 3D Texture Size Reports: " << val << endl;

			glGetIntegerv( GL_MAX_DRAW_BUFFERS, &val );
			cout << "\t\tMax Draw Buffers: " << val << endl;

			glGetIntegerv( GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, &val );
			cout << "\t\tMax Compute Texture Image Units Reports: " << val << endl;

			glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &val );
			cout << "\t\tMax Combined Texture Image Units: " << val << endl;

			glGetIntegerv( GL_MAX_VIEWPORT_DIMS, &val );
			cout << "\t\tMax Viewport Dims: " << val << endl << endl;

			// make up for the spacing issues
			cout << "\t............................................................. ";

		}

		// any additional init? not sure

	}

	void SetFilterMinMag ( string label, GLuint filterTypeMin, GLuint filterTypeMag ) {
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				glActiveTexture( 0 );
				glBindTexture( tex.creationOptions.textureType, tex.textureHandle );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterTypeMin );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterTypeMag );
				return;
			}
		}
		// if we finish the list without finding it
		cout << "Texture \"" << label << "\" Not Found" << endl;
		return;
	}

	GLuint Add ( string label, textureOptions_t &texOptsIn ) {

		texture_t tex;
		tex.label = label;
		tex.creationOptions = texOptsIn;
		tex.textureSize = texOptsIn.width * texOptsIn.height * texOptsIn.depth * texOptsIn.layers * bytesPerPixel( texOptsIn.dataType );

		const bool needsMipmap = ( // does this texture need to generate mipmaps
			texOptsIn.minFilter == GL_NEAREST_MIPMAP_NEAREST ||
			texOptsIn.minFilter == GL_LINEAR_MIPMAP_NEAREST ||
			texOptsIn.minFilter == GL_NEAREST_MIPMAP_LINEAR ||
			texOptsIn.minFilter == GL_LINEAR_MIPMAP_LINEAR
		);

		// generate the texture
		glActiveTexture( GL_TEXTURE0 );
		glGenTextures( 1, &tex.textureHandle );
		glBindTexture( texOptsIn.textureType, tex.textureHandle );
		switch ( texOptsIn.textureType ) {
		case GL_TEXTURE_2D:

			// { // temporary, trying to figure out if GL_CLAMP_TO_EDGE still exists - it does not
			// 	GLfloat color[ 4 ] = { 0.0f, 0.0f, 0.0f, 1.0f };
			// 	glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color );
			// }

			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texOptsIn.minFilter );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texOptsIn.magFilter );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texOptsIn.wrap );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texOptsIn.wrap );
			glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &texOptsIn.borderColor[ 0 ] );
			glTexImage2D( GL_TEXTURE_2D, 0, texOptsIn.dataType, texOptsIn.width, texOptsIn.height, 0, getFormat( texOptsIn.dataType ), texOptsIn.pixelDataType, texOptsIn.initialData );
			if ( needsMipmap == true ) {
				glGenerateMipmap( GL_TEXTURE_2D );
			}
			break;

		case GL_TEXTURE_2D_ARRAY:
			glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, texOptsIn.minFilter );
			glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, texOptsIn.magFilter );
			glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, texOptsIn.wrap );
			glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, texOptsIn.wrap );
			glTexParameterfv( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, &texOptsIn.borderColor[ 0 ] );
			glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, texOptsIn.dataType, texOptsIn.width, texOptsIn.height, texOptsIn.depth, 0, getFormat( texOptsIn.dataType ), texOptsIn.pixelDataType, texOptsIn.initialData );
			if ( needsMipmap == true ) {
				glGenerateMipmap( GL_TEXTURE_2D_ARRAY );
			}
			break;

		case GL_TEXTURE_3D:
			glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, texOptsIn.minFilter );
			glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, texOptsIn.magFilter );
			glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, texOptsIn.wrap );
			glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, texOptsIn.wrap );
			glTexParameterfv( GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, &texOptsIn.borderColor[ 0 ] );
			glTexImage3D( GL_TEXTURE_3D, 0, texOptsIn.dataType, texOptsIn.width, texOptsIn.height, texOptsIn.depth, 0, getFormat( texOptsIn.dataType ), texOptsIn.pixelDataType, texOptsIn.initialData );
			if ( needsMipmap == true ) {
				glGenerateMipmap( GL_TEXTURE_3D );
			}
			break;

		default:
			cout << "unknown texture type" << endl;
			break;
		}

		// add texture label
		glObjectLabel( GL_TEXTURE, tex.textureHandle, -1, label.c_str() );

		// data likely doesn't exist after initialization
		tex.creationOptions.initialData = nullptr;

		// store for later
		textures.push_back( tex );

		// return the texture handle
		return tex.textureHandle;
	}

	GLuint Get ( const string label ) { // if the array contains the key, return it, else some nonsense value
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				return tex.textureHandle;
			}
		}
		cout << "Texture \"" << label << "\" Missing" << endl;
		return std::numeric_limits< GLuint >::max();
	}

	GLint GetType ( const string label ) {
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				return tex.creationOptions.dataType; // this is the move
			}
		}
		return std::numeric_limits< GLint >::max();
	}

	uvec2 GetDimensions ( const string label ) {
		textureOptions_t opts;
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				opts = tex.creationOptions;
			}
		}
		return uvec2( opts.width, opts.height );
	}

// I think this is the way we're going to use this now...
	// additionally, we can drop the glUniform1i if using layout qualifiers in the shader code

	// I'd like to have something here that's just like, Bind( string label, int binding ), with some default arguments for glBindImageTexture(), like what the bindsets were doing
	void Bind ( string label, int location, int level = 0, GLuint access = GL_READ_WRITE ) { // is not equivalent for textures, not sure what's going on with that
		glBindImageTexture( location, Get( label ), level, GL_TRUE, 0, access, GetType( label ) );
	}

	// so an example call is textureManager.BindTexForShader( "Display Texture", "current", shaders[ "Display" ], 0 );
	void BindTexForShader ( string label, const string shaderSampler, const GLuint shader, int location ) {
		// if 4.5 feature set is not supported
		if ( compatibilityMode == true ) {
			glActiveTexture( GL_TEXTURE0 + location );
			glBindTexture( GL_TEXTURE_2D, Get( label ) );
		} else {
			glBindTextureUnit( location, Get( label ) );
		}
		glUniform1i( glGetUniformLocation( shader, shaderSampler.c_str() ), location );
	}

	// so an example call is textureManager.BindImageForShader( "Display Texture", "current", shaders[ "Display" ], 0 );
	void BindImageForShader ( string label, const string shaderSampler, const GLuint shader, int location, int level = 0 ) {
		glBindImageTexture( location, Get( label ), level, GL_TRUE, 0, GL_READ_WRITE, GetType( label ) );
		glUniform1i( glGetUniformLocation( shader, shaderSampler.c_str() ), location );
	}

	void ZeroTexture2D ( string label ) {
		GLuint handle, dataType, format;
		uint32_t w, h;
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				w = tex.creationOptions.width;
				h = tex.creationOptions.height;
				handle = tex.textureHandle;
				dataType = tex.creationOptions.dataType;
				format = getFormat( dataType );
			}
		}
		// this is going to be too slow for per-frame usage, allocating a big buffer like this over and over
		Image_4U zeroes( w, h ); // consider static declaration, resizing only if the current buffer is not large enough
		void * data = ( void * ) zeroes.GetImageDataBasePtr();
		glBindTexture( GL_TEXTURE_2D, handle );
		glTexImage2D( GL_TEXTURE_2D, 0, dataType, w, h, 0, format, GL_UNSIGNED_BYTE, data );
	}

	void ZeroTexture3D ( string label ) {
		GLuint handle, dataType, format;
		uint32_t w, h, d;
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				w = tex.creationOptions.width;
				h = tex.creationOptions.height;
				d = tex.creationOptions.depth;
				handle = tex.textureHandle;
				dataType = tex.creationOptions.dataType;
				format = getFormat( dataType );
			}
		}
		// this is going to be too slow for per-frame usage, allocating a big buffer like this over and over
		Image_4U zeroes( w, h * d );
		void * data = ( void * ) zeroes.GetImageDataBasePtr();
		glBindTexture( GL_TEXTURE_3D, handle );
		glTexImage3D( GL_TEXTURE_3D, 0, dataType, w, h, d, 0, format, GL_UNSIGNED_BYTE, data );
	}

	void Update ( string label /*, ... */ ) {
		// pass in new data for the texture... tbd
	}

	size_t Count () { // how many textures are in the current set?
		return textures.size(); // this will support add / remove in the future
	}

	void EnumerateTextures () {

		int maxWidth = 0; // maximum width of the bytes value, across all textures
		for ( auto& tex : textures ) {
			maxWidth = std::max( maxWidth, int( GetWithThousandsSeparator( tex.textureSize ).length() ) );
		}

	// I want more info here:
		// dimensions
		// number of channels
		// data type + number of bits

		const int reportWidth = 48;
		cout << "  Textures :" << endl;
		for ( auto& tex : textures ) {
			std::stringstream s;
			s << "    " << std::setw( 3 ) << std::setfill( '0' ) << tex.textureHandle << " : " << tex.label << " ";
			cout << s.str();
			for ( unsigned int i = 0; i < reportWidth - s.str().size(); i++ ) {
				cout << ".";
			}
			cout << " ( " << std::setw( maxWidth ) << std::setfill( ' ' ) << GetWithThousandsSeparator( tex.textureSize ) << " bytes )" << endl;
		}
		cout << endl;
	}

	// total size in bytes
	size_t TotalSize () {
		size_t total = 0;
		for ( auto& tex : textures )
			total += tex.textureSize;
		return total;
	}

	void Remove ( string label ) {
		// delete texture
		GLuint tex = Get( label );

		// just early out
		if ( tex == std::numeric_limits< GLuint >::max() ) { return; }

		glDeleteTextures( 1, &tex );

		// do the removal
		bool removed = false;
		std::vector< texture_t >::iterator itr = textures.begin();
		while ( itr != textures.end() ) {
			if ( itr->label == label ) {
				textures.erase( itr );
				removed = true;
			} else {
				++itr;
			}
		}

		// if we get through the list without finding, the texture was not found
		if ( !removed ) {
			cout << "texture not found" << endl;
		}
	}

	// TODO
	void Shutdown () {
		// delete all textures
	}

	std::vector< texture_t > textures; // keep as an ordered set
};

// move bindsets to here
	// bindsets get a new field to tell texture/image bind type

#endif // TEXTURE_H
