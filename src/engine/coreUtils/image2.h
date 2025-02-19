#pragma once
#ifndef IMAGE2_H
#define IMAGE2_H

//===== LodePNG =======================================================================================================
// Lode Vandevenne's LodePNG PNG Load/Save lib
#include <imageLibs/LodePNG/lodepng.h>		// https://github.com/lvandeve/lodepng

//===== STB ===========================================================================================================
// Sean Barrett's public domain load, save, resize libs - need corresponding define in the ./stb/impl.cc file,
	// before their inclusion, which is done by the time compilation hits this point - they can be straight
	// included, here, as follows:
#include <imageLibs/stb/stb_image.h> // https://github.com/nothings/stb/blob/master/stb_image.h
#include <imageLibs/stb/stb_image_write.h>	// https://github.com/nothings/stb/blob/master/stb_image_write.h
#include <imageLibs/stb/stb_image_resize.h>	// https://github.com/nothings/stb/blob/master/stb_image_resize.h

//===== tinyEXR =======================================================================================================
// TinyEXR is for loading and saving of high bit depth images - 16, 32 bits, etc
#include <imageLibs/tinyEXR/tinyexr.h>		// https://github.com/syoyo/tinyexr

//===== STL ===========================================================================================================
#include <array>
#include <vector>
#include <random>
#include <string>
#include <iostream>
#include <type_traits> // for std::is_same - https://en.cppreference.com/w/cpp/types/is_same

//===== Image2 ========================================================================================================

// TODO:
	// CPU side dithering
	// CPU side tonemapping - float type only, most likely
	// higher quality sampling stuff, tbd
	// more types of lens distorts - https://www.tangramvision.com/blog/camera-modeling-exploring-distortion-and-distortion-models-part-ii
		// there's more to the Brown-Conrady model https://arxiv.org/pdf/1804.03584v1.pdf - cubic contribution, too, for assymetric warps
		// Kannala-Brandt might be worth taking a look at http://close-range.com/docs/A_GENERIC_CAMERA_MODEL_AND_CALIBRATION_METHOD_Kannala-Brandt_pdf697.pdf


enum channel {
	red = 0,
	green = 1,
	blue = 2,
	alpha = 3
};

template < typename imageType, int numChannels > class Image2 {
public:

	struct color {
		color () {}
		color ( std::array< imageType, numChannels > init ) : data( init ) {}

		std::array< imageType, numChannels > data { 0 };

		imageType operator [] ( int c ) const { return data[ c ]; }	// for val = color[ c ]
		imageType & operator [] ( int c ) { return data[ c ]; }		// for color[ c ] = val

		friend bool operator == ( const color& l, const color& r ) {
			for ( int i = 0; i < numChannels; i++ ) {
				if ( l.data[ i ] != r.data[ i ] ) {
					return false;
				}
			}
			return true;
		}

		// operators needed for blending - sum + division by float normalization factor
			// adding some others, just in case I need them at some point
		color operator + ( const color &other ) const {
			color temp;
			for ( int c { 0 }; c < numChannels; c++ ) {
				temp.data[ c ] = this->data[ c ] + other.data[ c ];
			}
			return temp;
		}

		color operator - ( const color &other ) const {
			color temp;
			for ( int c { 0 }; c < numChannels; c++ ) {
				temp.data[ c ] = this->data[ c ] - other.data[ c ];
			}
			return temp;
		}

		color operator / ( const float divisor ) const {
			color temp;
			for ( int c { 0 }; c < numChannels; c++ ) {
				temp.data[ c ] = this->data[ c ] / divisor;
			}
			return temp;
		}

		color operator * ( const float scalar ) const {
			color temp;
			for ( int c { 0 }; c < numChannels; c++ ) {
				temp.data[ c ] = this->data[ c ] * scalar;
			}
			return temp;
		}

		color operator / ( const color &divisor ) const {
			color temp;
			for ( int c { 0 }; c < numChannels; c++ ) {
				temp.data[ c ] = this->data[ c ] / divisor.data[ c ];
			}
			return temp;
		}

		color operator * ( const color &scalar ) const {
			color temp;
			for ( int c { 0 }; c < numChannels; c++ ) {
				temp.data[ c ] = this->data[ c ] * scalar.data[ c ];
			}
			return temp;
		}

		float GetLuma () const {
		// we're going to basically bake in the assumption that it has 3 color channels
			// because this luma calculation is basically just valid for the RGB color situation
			const bool isUint = std::is_same< uint8_t, imageType >::value;
			// NTSC saturation weights
			const float scaleFactors[] = { 0.299f, 0.587f, 0.114f };
			float sum = 0.0f;
			for ( int c { 0 }; c < numChannels && c < 3; c++ ) {
				sum += ( isUint ? data[ c ] / 255.0f : data[ c ] ) * ( isUint ? data[ c ] / 255.0f : data[ c ] ) * scaleFactors[ c ];
			}
			return sqrt( sum );
		}

		// add swizzle? or is that too redundant? would probably make more sense on the color than on the image

	};

//===== Constructors ==================================================================================================

	// default
	Image2 () : width( 0 ), height( 0 ) {}

	// init emtpy image with given dimensions
	Image2 ( uint32_t x, uint32_t y ) : width( x ), height( y ) {
		data.resize( width * height * numChannels, 0 );
	}

	// load image from path
	enum class backend { STB_IMG, LODEPNG, TINYEXR };
	Image2 ( string path, backend loader = backend::LODEPNG ) {
		if ( !Load( path, loader ) ) {
			cout << "image load failed with path " << path << newline << flush;
		}
	}

	// load from e.g. GPU memory, also used for copying from another image with GetImageDataBasePtr()
	Image2 ( uint32_t x, uint32_t y, const imageType* contents ) : width( x ), height( y ) {
		const size_t numBers = width * height * numChannels;
		data.reserve( numBers );
		for ( size_t idx = 0; idx < numBers; idx++ ) {
			data.push_back( contents[ idx ] );
		}
	}

	// construct from another image of the same type
	Image2< imageType, numChannels > ( const Image2< imageType, numChannels > &source ) {
		width = source.Width();
		height = source.Height();
		const size_t numBers = width * height * numChannels;
		data.reserve( numBers );
		for ( size_t idx = 0; idx < numBers; idx++ ) {
			data.push_back( source.GetImageDataBasePtr()[ idx ] );
		}
	}

//===== Functions =====================================================================================================
//======= Basic =======================================================================================================

	bool Load ( string path, backend loader = backend::LODEPNG ) {
		switch ( loader ) {
			case backend::STB_IMG: return LoadSTB_img( path ); break;
			case backend::LODEPNG: return LoadLodePNG( path ); break;
			case backend::TINYEXR: return LoadTinyEXR( path ); break;
			default: return false;
		}
	}

	bool Save ( string path, backend loader = backend::LODEPNG ) const {
		switch ( loader ) {
			case backend::STB_IMG: return SaveSTB_img( path ); break;
			case backend::LODEPNG: return SaveLodePNG( path ); break;
			case backend::TINYEXR: return SaveTinyEXR( path ); break;
			default: return false;
		}
	}

	void FlipHorizontal () {
		// back up the existing state of the image
		std::vector< imageType > oldData;
		oldData.reserve( width * height * numChannels );
		for ( auto c : data ) oldData.push_back( c );
		data.resize( 0 );
		data.reserve( width * height * 4 );
		for ( uint32_t y = 0; y < height; y++ ) {
			for ( uint32_t x = 0; x < width; x++ ) {
				for ( uint8_t c = 0; c < numChannels; c++ ) {
					data.push_back( oldData[ ( ( width - x - 1 ) + y * width ) * numChannels + c ] );
				}
			}
		}
	}

	void FlipVertical () {
		// back up the existing state of the image
		std::vector< imageType > oldData;
		oldData.reserve( width * height * numChannels );
		for ( auto c : data ) oldData.push_back( c );
		data.resize( 0 );
		data.reserve( width * height * numChannels );
		for ( uint32_t y = 0; y < height; y++ ) {
			for ( uint32_t x = 0; x < width; x++ ) {
				for ( uint8_t c = 0; c < numChannels; c++ ) {
					data.push_back( oldData[ ( x + ( height - y - 1 ) * width ) * numChannels + c ] );
				}
			}
		}
	}

	void Resize ( float factor ) {
		// scale, uniform on both axes
		Resize( factor, factor );
	}

	void Resize ( float XFactor, float YFactor ) {
		// scale factor does not need to be the same on x and y
		int newX = std::floor( XFactor * float( width ) );
		int newY = std::floor( YFactor * float( height ) );

		// create a copy of the existing data
		const uint32_t oldSize = width * height * numChannels;
		imageType* oldData = ( imageType* ) malloc( oldSize * sizeof( imageType ) );
		for ( uint32_t i = 0; i < oldSize; i++ ) {
			oldData[ i ] = data[ i ];
		}

		// buffer needs to be allocated before the resize function runs
		imageType* newData = ( imageType* ) malloc( newX * newY * numChannels * sizeof( imageType ) );

		// compiler is complaining without the casts, I have no idea why that would be neccesary - it has the correct type from imageType
			// I guess the is_same<> does not evaluate during template instantiation? hard to say, I don't really have any insight here
		if ( std::is_same< uint8_t, imageType >::value ) { // uint type
			stbir_resize_uint8( ( const uint8_t* ) oldData, width, height, width * numChannels * sizeof( imageType ),
				( uint8_t* ) newData, newX, newY, newX * numChannels * sizeof( imageType ), numChannels );
		} else { // float type
			stbir_resize_float( ( const float* ) oldData, width, height, width * numChannels * sizeof( imageType ),
				( float* ) newData, newX, newY, newX * numChannels * sizeof( imageType ), numChannels );
		}

		// image dimensions are now scaled up by the input scale factor
		width = newX;
		height = newY;

		// copy new data back to the data vector
		data.resize( 0 );
		const uint32_t newSize = width * height * numChannels;
		data.reserve( newSize );
		for ( uint32_t i = 0; i < newSize; i++ ) {
			data.push_back( newData[ i ] );
		}
	}

	void ClearTo ( color c ) {
		for ( uint32_t y { 0 }; y < height; y++ ){
			for ( uint32_t x { 0 }; x < width; x++ ) {
				SetAtXY( x, y, c );
			}
		}
	}

	void ClearEveryOtherColumnTo ( color c ) {
		for ( uint32_t y { 0 }; y < height; y++ ){
			for ( uint32_t x { 0 }; x < width; x+=2 ) {
				SetAtXY( x, y, c );
			}
		}
	}

	void ClearEveryOtherRowTo ( color c ) {
		for ( uint32_t y { 0 }; y < height; y+=2 ){
			for ( uint32_t x { 0 }; x < width; x++ ) {
				SetAtXY( x, y, c );
			}
		}
	}

	void GammaCorrect ( const float gamma, bool touchAlpha = false ) {
		for ( uint32_t y { 0 }; y < height; y++ ){
			for ( uint32_t x { 0 }; x < width; x++ ) {
				color src = GetAtXY( x, y );
				for ( uint8_t c { 0 }; c < numChannels; c++ ) {
					if ( !touchAlpha && c == 3 ) break;
					src[ c ] = std::pow( src[ c ], 1.0f / gamma );
				}
				SetAtXY( x, y, src );
			}
		}
	}

//======= Esoterica ===================================================================================================

	void Swizzle ( const char swizzle [ numChannels ] ) {
	// options for each char are the following:
		// 'r' is input red value,	'R' is max - input red value
		// 'g' is input green value,'G' is max - input green value
		// 'b' is input blue value,	'B' is max - input blue value
		// 'a' is input alpha value,'A' is max - input alpha value
		// 'l' is the input luma,	'L' is max - input luma
		// '0' saturates to min, ignoring input
		// '1' saturates to max, ignoring input

	// there are some assumptions made that it's a 4 channel image ( and that's ok )

	// you can do a pretty arbitrary transform on the data with these options - there
		// are many ( ( 8 + 2 + 2 ) ^ 4 ) options, so hopefully one of those fits your need
		const bool isUint = std::is_same< uint8_t, imageType >::value;
		const imageType min = isUint ?   0 : 0.0f;
		const imageType max = isUint ? 255 : 1.0f;
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				const color sourceData = GetAtXY( x, y );
				const float sourceLuma = sourceData.GetLuma();
				color setData;
				for ( uint8_t c { 0 }; c < numChannels; c++ ) {
					switch ( swizzle[ c ] ) {
						case 'r': setData[ c ] = sourceData[ red ]; break;
						case 'R': setData[ c ] = max - sourceData[ red ]; break;
						case 'g': setData[ c ] = sourceData[ green ]; break;
						case 'G': setData[ c ] = max - sourceData[ green ]; break;
						case 'b': setData[ c ] = sourceData[ blue ]; break;
						case 'B': setData[ c ] = max - sourceData[ blue ]; break;
						case 'a': setData[ c ] = sourceData[ alpha ]; break;
						case 'A': setData[ c ] = max - sourceData[ alpha ]; break;
						case 'l': setData[ c ] = isUint ? sourceLuma * 255 : sourceLuma; break;
						case 'L': setData[ c ] = max - ( isUint ? sourceLuma * 255 : sourceLuma ); break;
						case '0': setData[ c ] = min; break;
						case '1': setData[ c ] = max; break;
					}
				}
				SetAtXY( x, y, setData );
			}
		}
	}

	void SaturateAlpha () {
		Swizzle( "rgb1" );
	}

	// show only a subset of the image, or make it larger, and fill with all zeroes
	void Crop ( uint32_t newWidth, uint32_t newHeight, uint32_t offsetX = 0, uint32_t offsetY = 0 ) {
		// out of bounds reads are all zeroes, we will keep that convention for simplicity
		std::vector< imageType > newData;
		newData.reserve( newWidth * newHeight * numChannels );
		for ( uint32_t y { 0 }; y < newHeight; y++ ) {
			for ( uint32_t x { 0 }; x < newWidth; x++ ) {
				color oldVal = GetAtXY( x + offsetX, y + offsetY );
				for ( uint8_t i { 0 }; i < numChannels; i++ ) {
					newData.push_back( oldVal[ i ] );
				}
			}
		}
		data.resize( 0 );
		data.reserve( newWidth * newHeight * numChannels );
		data.assign( newData.begin(), newData.end() );
		newData.clear();
		width = newWidth;
		height = newHeight;
	}

	// scale each channel of the image, using some input color
	void ColorCast ( color cast ) {
		const bool isUint = std::is_same< uint8_t, imageType >::value;
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				color existingColor = GetAtXY( x, y );
				for ( uint8_t c { 0 }; c < numChannels; c++ ) {
					float scalar = isUint ? ( cast[ c ] / 255.0f ) : cast[ c ];
					existingColor[ c ] *= scalar;
				}
				SetAtXY( x, y, existingColor );
			}
		}
	}

	// TODO: thresholding logic, masking?
	void PixelSort ( int orientation, int channel ) {
		const bool horizontal = orientation == 0;
		for ( uint32_t x { 0 }; x < ( horizontal ? width : height ); x++ ) {
			std::vector< color > vec;
			for ( uint32_t y { 0 }; y < ( horizontal ? height : width ); y++ ) {
			// currently just take all non-transparent pixels, but need a way to mark taken pixels if we want more flexible thresholding
				color pixel = GetAtXY( horizontal ? x : y, horizontal ? y : x );
				if ( pixel[ alpha ] != 0 ) {
					vec.push_back( pixel );
				}
			}
			if ( channel < 4 ) {
				std::sort( vec.begin(), vec.end(), [ channel ]( const color & a, const color & b ) -> bool {
					return a[ channel ] < b[ channel ];
				} );
			} else {
				std::sort( vec.begin(), vec.end(), []( const color & a, const color & b ) -> bool {
					return a.GetLuma() < b.GetLuma();
				} );
			}
			// this will need to know where the pixels have been taken from... mark indices in some way
			for ( uint32_t i { 0 }; i < vec.size(); i++ ) {
				SetAtXY( horizontal ? x : i, horizontal ? i : x, vec[ i ] );
			}
		}
	}

	// srgb conversions <-> linear light https://www.shadertoy.com/view/4tXcWr
		// these really only apply to float images
	void SRGBtoRGB( bool preserveAlpha = true ) {
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				color col = GetAtXY( x, y );
				vec4 sRGB = vec4( col[ 0 ], col[ 1 ], col[ 2 ], col[ 3 ] );
				bvec4 cutoff = lessThan( sRGB, vec4( 0.04045f ) );
				vec4 higher = pow( ( sRGB + vec4( 0.055f ) ) / vec4( 1.055f ), vec4( 2.4f ) );
				vec4 lower = sRGB / vec4( 12.92f );
				vec4 result =  mix( higher, lower, cutoff );
				if ( preserveAlpha == true ) {
					result = vec4( result.rgb(), sRGB.a );
				}
				col[ 0 ] = result.r;
				col[ 1 ] = result.g;
				col[ 2 ] = result.b;
				col[ 3 ] = result.a;
				SetAtXY( x, y, col );
			}
		}
	}

	void RGBtoSRGB( bool preserveAlpha = true ) {
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				color col = GetAtXY( x, y );
				vec4 linearRGB = vec4( col[ 0 ], col[ 1 ], col[ 2 ], col[ 3 ] );
				bvec4 cutoff = lessThan( linearRGB, vec4( 0.0031308f ) );
				vec4 higher = vec4( 1.055f ) * pow( linearRGB, vec4( 1.0f / 2.4f ) ) - vec4( 0.055f );
				vec4 lower = linearRGB * vec4( 12.92f );
				vec4 result = mix( higher, lower, cutoff );
				if ( preserveAlpha == true ) {
					result = vec4( result.rgb(), linearRGB.a );
				}
				col[ 0 ] = result.r;
				col[ 1 ] = result.g;
				col[ 2 ] = result.b;
				col[ 3 ] = result.a;
				SetAtXY( x, y, col );
			}
		}
	}

	// remapping the data in the image ( particularly useful for floating point types, heightmap kind of stuff )
	enum remapOperation_t {
		// no op
		NOOP,

		// values are clamped to minimum and maximum
		HARDCLIP,

		// same as above, but the top of the ranges are compressed
		SOFTCLIP,

		// look across the image, find min and max, remap to 0.0 to 1.0
			// easy implementation: go through and find min max, set up the
			// thing as HARDCLIP, because you know you won't want to go outside
			// of that volume, and then once we have the rest of the processing
			// done for the channels, recursive call to function with the
			// updated parameters
		AUTONORMALIZE
	};

	// data for one channel's remapping operation
	struct rangeRemapInputs_t {
		remapOperation_t rangeType = AUTONORMALIZE;
		float	rangeStartLow	= 0.0f, rangeStartHigh	= 0.0f;
		float	rangeEndLow		= 0.0f, rangeEndHigh	= 0.0f;
	};

	// remap a single value from [inLow, inHigh] to [outLow, outHigh]
	imageType RangeRemapValue ( imageType value, imageType inLow, imageType inHigh, imageType outLow, imageType outHigh ) const {
		return outLow + ( value - inLow ) * ( outHigh - outLow ) / ( inHigh - inLow );
	}

	void RangeRemap ( rangeRemapInputs_t in [ numChannels ] ) {
		bool recursive = false;
		for ( uint8_t c { 0 }; c < numChannels; c++ ) {
			if ( in[ c ].rangeType == AUTONORMALIZE ) {
				recursive = true;
				// do the work to find the range
				in[ c ].rangeType = HARDCLIP;
				in[ c ].rangeStartLow = GetPixelMin( ( channel ) c );
				in[ c ].rangeStartHigh = GetPixelMax( ( channel ) c );
				in[ c ].rangeEndLow = 0.0f;
				in[ c ].rangeEndHigh = 1.0f;
				cout << "remapping from " << in[ c ].rangeStartLow << " : " << in[ c ].rangeStartHigh << " to 0.0 : 1.0" << endl;
			}
		}
		if ( recursive ) { RangeRemap( in ); }

		// now everything should have a valid config - do the range remapping for each channel
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				color colorRead = GetAtXY( x, y );

				// do the remapping for the channel
				for ( uint8_t c { 0 }; c < numChannels; c++ ) {
					switch ( in[ c ].rangeType ) {

						case NOOP:
							break;

						case HARDCLIP: {

							// imageType temp = colorRead[ c ];
							colorRead[ c ] = std::clamp(
								RangeRemapValue(
									colorRead[ c ],
									in[ c ].rangeStartLow,
									in[ c ].rangeStartHigh,
									in[ c ].rangeEndLow,
									in[ c ].rangeEndHigh
								),
								in[ c ].rangeEndLow,
								in[ c ].rangeEndHigh );
							// if ( temp != colorRead[ c ] ) {
								// cout << "Remapping " << temp << " from " << in[ c ].rangeStartLow << " : " << in[ c ].rangeStartHigh << " to 0.0 : 1.0f gives " << colorRead[ c ] << endl;
							// }
							// if ( colorRead[ c ] == in[ c ].rangeEndLow ) {
								// cout << "Remapping " << temp << " from " << in[ c ].rangeStartLow << " : " << in[ c ].rangeStartHigh << " to 0.0 : 1.0f gives " << colorRead[ c ] << endl;
							// }
							break;
						}

						case SOFTCLIP:
							// todo
							break;

						default:
							break;

					}
				}
				SetAtXY( x, y, colorRead );
			}
		}
	}

// Lens distortion - makes use of interpolated reads
	// this one uses the Brown-Conrady lens distortion model
		// Some sample values:
			// { k1 = -0.2, k2 = 0.2, tangentialSkew =  0.2 }
			// { k1 =  0.5, k2 = 0.4, tangentialSkew = -0.2 }

	void BrownConradyLensDistort ( const float k1, const float k2, const float tangentialSkew, const bool normalize = false ) {
		// create an identical copy of the data, since we will be overwriting the entire image
		const Image2< imageType, numChannels > cachedCopy( width, height, GetImageDataBasePtr() );

		const float normalizeFactor = ( abs( k1 ) < 1.0f ) ? ( 1.0f - abs( k1 ) ) : ( 1.0f / ( k1 + 1.0f ) );

		// iterate over every pixel in the image - calculate distorted UV's and sample the cached version
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				// pixel coordinate in UV space
				const vec2 normalizedPosition = vec2( ( float ) x / ( float ) width, ( float ) y / ( float ) height );

				// TODO: normalize the width/height ratio
					// want to be able to support different behaviors
						// current behavior, treates image as -1..1
						// correction for aspect ratio, so the distort is circular
						// correction for aspect ratio, but like 1/AR, so the distort shifts wrt the center of the image

				// distort the position, based on the logic described in https://www.shadertoy.com/view/wtBXRz:
					// k1 is the main distortion coefficient, positive is barrel distortion, negative is pincusion distortion
					// k2 tweaks the edges of the distortion - can be 0.0

				vec2 remapped = ( normalizedPosition * 2.0f ) - vec2( 1.0f );
				const float r2 = remapped.x * remapped.x + remapped.y * remapped.y;
				remapped *= 1.0f + ( k1 * r2 ) * ( k2 * r2 * r2 );

				// tangential distortion
				if ( tangentialSkew != 0.0f ) {
					const float angle = r2 * tangentialSkew;
					remapped = mat2( cos( angle ), -sin( angle ), sin( angle ), cos( angle ) ) * remapped;
				}

				// restore back to the normalized space
				remapped = remapped * 0.5f + vec2( 0.5f );

				if ( normalize ) {
					// scale about the image center, to keep the image close to the same size
					remapped = remapped * normalizeFactor - ( normalizeFactor * 0.5f ) + vec2( 0.5f );
				}

				// get the sample of the cached copy
				SetAtXY( x, y, cachedCopy.Sample( remapped, samplerType_t::LINEAR_FILTER ) );
			}
		}
	}

	// same as above, but combines multiple samples with strength increasing from 0 to the specified parameters in order to blur
		// note that the MS versions of this function assume they have enough range to average across the N samples

	void BrownConradyLensDistortMSBlurred ( const int iterations, const float k1, const float k2, const float t1, const bool normalize = false ) {
		BrownConradyLensDistortMSBlurred( iterations, -k1, k1, -k2, k2, -t1, t1, normalize );
	}

	// pass zero as min values to get the old behavior
	void BrownConradyLensDistortMSBlurred ( const int iterations,
		const float k1min, const float k1max,
		const float k2min, const float k2max,
		const float t1min, const float t1max,
		const bool normalize = false ) {

		// create an identical copy of the data, since we will be overwriting the entire image
		const Image2< imageType, numChannels > cachedCopy( width, height, GetImageDataBasePtr() );

		// iterate over every pixel in the image - calculate distorted UV's and sample the cached version
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {

				color accumulated; // making use of zero initialization
				for ( int i = 0; i < iterations; i++ ) {
					// pixel coordinate in UV space
					const vec2 normalizedPosition = vec2( ( float ) x / ( float ) width, ( float ) y / ( float ) height );
					vec2 remapped = ( normalizedPosition * 2.0f ) - vec2( 1.0f );
					const float r2 = remapped.x * remapped.x + remapped.y * remapped.y;

					const float iterationK1 = RangeRemapValue( i, 0.0f, iterations, k1min, k1max );
					const float iterationK2 = RangeRemapValue( i, 0.0f, iterations, k2min, k2max );
					const float iterationT1 = RangeRemapValue( i, 0.0f, iterations, t1min, t1max );

					remapped *= 1.0f + ( iterationK1 * r2 ) * ( iterationK2 * r2 * r2 );

					const float angle = r2 * iterationT1;
					remapped = mat2( cos( angle ), -sin( angle ), sin( angle ), cos( angle ) ) * remapped;

					remapped = remapped * 0.5f + vec2( 0.5f ); // restore back to the normalized space

					if ( normalize ) { // scale about the image center, to keep the image close to the same size
						const float normalizeFactor = ( abs( iterationK1 ) < 1.0f ) ? ( 1.0f - abs( iterationK1 ) ) : ( 1.0f / ( iterationK1 + 1.0f ) );
						remapped = remapped * normalizeFactor - ( normalizeFactor * 0.5f ) + vec2( 0.5f );
					}

					// get the sample of the cached copy
					accumulated = accumulated + cachedCopy.Sample( remapped, samplerType_t::LINEAR_FILTER );
				}

				accumulated = accumulated / ( float ) iterations;
				SetAtXY( x, y, accumulated );
			}
		}
	}

	void BrownConradyLensDistortMSBlurredChromatic ( const int iterations, const float k1, const float k2, const float t1 ) {
		BrownConradyLensDistortMSBlurredChromatic( iterations, -k1, k1, -k2, k2, -t1, t1 );
	}

	void BrownConradyLensDistortMSBlurredChromatic ( const int iterations,
		const float k1min, const float k1max,
		const float k2min, const float k2max,
		const float t1min, const float t1max ) {
		// create an identical copy of the data, since we will be overwriting the entire image
		const Image2< imageType, numChannels > cachedCopy( width, height, GetImageDataBasePtr() );

		// iterate over every pixel in the image - calculate distorted UV's and sample the cached version
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {

				color weight;
				color weightAccum;
				color accumulated;

				const float midPoint = ( float ) iterations / 2.0f;
				for ( int i = 0; i < iterations; i++ ) {

					const float iterationK1 = RangeRemapValue( i, 0.0f, iterations, k1min, k1max );
					const float iterationK2 = RangeRemapValue( i, 0.0f, iterations, k2min, k2max );
					const float iterationT1 = RangeRemapValue( i, 0.0f, iterations, t1min, t1max );

					// pixel coordinate in UV space
					const vec2 normalizedPosition = vec2( ( float ) x / ( float ) width, ( float ) y / ( float ) height );

					vec2 remapped = ( normalizedPosition * 2.0f ) - vec2( 1.0f );
					const float r2 = remapped.x * remapped.x + remapped.y * remapped.y;

					remapped *= 1.0f + ( iterationK1 * r2 ) * ( iterationK2 * r2 * r2 );
					// remapped *= 1.0f + ( iterationK1 * r2 ) + ( iterationK2 * r2 * r2 ); // interesting

					if ( iterationT1 != 0.0f ) { // tangential distortion
						const float angle = r2 * iterationT1;
						remapped = mat2( cos( angle ), -sin( angle ), sin( angle ), cos( angle ) ) * remapped;
					}

					remapped = remapped * 0.5f + vec2( 0.5f ); // restore back to the normalized space

					if ( i < midPoint ) { // red to green if less than midpoint, green to blue if greater
						weight[ blue ]	= RangeRemapValue( i, 0.0f, midPoint, 1.0f, 0.0f );
						weight[ green ]	= ( 1.0f - weight[ blue ] ) / 2.0f;
						weight[ red ]	= 0.0f;
						weight[ alpha ]	= 1.0f;
					} else {
						weight[ red ]	= RangeRemapValue( i, midPoint, iterations, 0.0f, 1.0f );
						weight[ blue ]	= 0.0f;
						weight[ green ]	= ( 1.0f - weight[ red ] ) / 2.0f;
						weight[ alpha ]	= 1.0f;
					}

					// get the sample of the cached copy
					accumulated = accumulated + ( cachedCopy.Sample( remapped, samplerType_t::LINEAR_FILTER ) * weight );
					weightAccum = weightAccum + weight;
				}

				// normalize wrt the accumulated weights
				accumulated = accumulated / weightAccum;
				SetAtXY( x, y, accumulated );
			}
		}
	}

	void BrownConradyLensDistortMSBlurredChromaticSmooth ( const int iterations, const float k1, const float k2, const float t1 ) {
		BrownConradyLensDistortMSBlurredChromaticSmooth( iterations, -k1, k1, -k2, k2, -t1, t1 );
	}

	void BrownConradyLensDistortMSBlurredChromaticSmooth ( const int iterations,
		const float k1min, const float k1max,
		const float k2min, const float k2max,
		const float t1min, const float t1max ) {
		// create an identical copy of the data, since we will be overwriting the entire image
		const Image2< imageType, numChannels > cachedCopy( width, height, GetImageDataBasePtr() );

		// iterate over every pixel in the image - calculate distorted UV's and sample the cached version
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {

				color weight;
				color weightAccum;
				color accumulated;

				for ( int i = 0; i < iterations; i++ ) {

					const float iterationK1 = RangeRemapValue( i, 0.0f, iterations, k1min, k1max );
					const float iterationK2 = RangeRemapValue( i, 0.0f, iterations, k2min, k2max );
					const float iterationT1 = RangeRemapValue( i, 0.0f, iterations, t1min, t1max );

					// pixel coordinate in UV space
					const vec2 normalizedPosition = vec2( ( float ) x / ( float ) width, ( float ) y / ( float ) height );

					vec2 remapped = ( normalizedPosition * 2.0f ) - vec2( 1.0f );
					const float r2 = remapped.x * remapped.x + remapped.y * remapped.y;

					remapped *= 1.0f + ( iterationK1 * r2 ) * ( iterationK2 * r2 * r2 );
					// remapped *= 1.0f + ( iterationK1 * r2 ) + ( iterationK2 * r2 * r2 ); // interesting

					if ( iterationT1 != 0.0f ) { // tangential distortion
						const float angle = r2 * iterationT1;
						remapped = mat2( cos( angle ), -sin( angle ), sin( angle ), cos( angle ) ) * remapped;
					}

					remapped = remapped * 0.5f + vec2( 0.5f ); // restore back to the normalized space

					float interp = RangeRemapValue( i + 0.5f, 0.0f, iterations, 0.0f, 1.0f );
					weight[ red ] = sin( interp );
					weight[ green ] = sin( interp * 2.0f );
					weight[ blue ] = cos( interp );
					weight[ alpha ] = 1.0f;

					// get the sample of the cached copy
					accumulated = accumulated + ( cachedCopy.Sample( remapped, samplerType_t::LINEAR_FILTER ) * weight );
					weightAccum = weightAccum + weight;
				}

				// normalize wrt the accumulated weights
				accumulated = accumulated / weightAccum;
				SetAtXY( x, y, accumulated );
			}
		}
	}

	void BrownConradyLensDistortMSBlurredChromaticNormalized ( const int iterations, const float k1, const float k2, const float t1 ) {
		BrownConradyLensDistortMSBlurredChromaticNormalized( iterations, -k1, k1, -k2, k2, -t1, t1 );
	}

	void BrownConradyLensDistortMSBlurredChromaticNormalized ( const int iterations,
		const float k1min, const float k1max,
		const float k2min, const float k2max,
		const float t1min, const float t1max ) {

		// create an identical copy of the data, since we will be overwriting the entire image
		const Image2< imageType, numChannels > cachedCopy( width, height, GetImageDataBasePtr() );

		// iterate over every pixel in the image - calculate distorted UV's and sample the cached version
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {

				color weight;
				color weightAccum;
				color accumulated;

				const float midPoint = ( float ) iterations / 2.0f;
				for ( int i = 0; i < iterations; i++ ) {

					const float iterationK1 = RangeRemapValue( i, 0.0f, iterations, k1min, k1max );
					const float iterationK2 = RangeRemapValue( i, 0.0f, iterations, k2min, k2max );
					const float iterationT1 = RangeRemapValue( i, 0.0f, iterations, t1min, t1max );

					// pixel coordinate in UV space
					const vec2 normalizedPosition = vec2( ( float ) x / ( float ) width, ( float ) y / ( float ) height );

					vec2 remapped = ( normalizedPosition * 2.0f ) - vec2( 1.0f );
					const float r2 = remapped.x * remapped.x + remapped.y * remapped.y;

					remapped *= 1.0f + ( iterationK1 * r2 ) * ( iterationK2 * r2 * r2 );
					// remapped *= 1.0f + ( iterationK1 * r2 ) + ( iterationK2 * r2 * r2 ); // interesting

					if ( iterationT1 != 0.0f ) { // tangential distortion
						const float angle = r2 * iterationT1;
						remapped = mat2( cos( angle ), -sin( angle ), sin( angle ), cos( angle ) ) * remapped;
					}

					remapped = remapped * 0.5f + vec2( 0.5f ); // restore back to the normalized space

					// scale about the image center, to keep the image close to the same size
					const float normalizeFactor = ( abs( iterationK1 ) < 1.0f ) ?
						( 1.0f - abs( iterationK1 ) ) :
						( 1.0f / ( iterationK1 + 1.0f ) );
					remapped = remapped * normalizeFactor - ( normalizeFactor * 0.5f ) + vec2( 0.5f );

					if ( i < midPoint ) { // red to green if less than midpoint, green to blue if greater
						weight[ blue ]	= RangeRemapValue( i, 0.0f, midPoint, 1.0f, 0.0f );
						weight[ green ]	= ( 1.0f - weight[ blue ] ) / 2.0f;
						weight[ red ]	= 0.0f;
						weight[ alpha ]	= 1.0f;
					} else {
						weight[ red ]	= RangeRemapValue( i, midPoint, iterations, 0.0f, 1.0f );
						weight[ blue ]	= 0.0f;
						weight[ green ]	= ( 1.0f - weight[ red ] ) / 2.0f;
						weight[ alpha ]	= 1.0f;
					}

					// get the sample of the cached copy
					accumulated = accumulated + ( cachedCopy.Sample( remapped, samplerType_t::LINEAR_FILTER ) * weight );
					weightAccum = weightAccum + weight;
				}

				// normalize wrt the accumulated weights
				accumulated = accumulated / weightAccum;
				SetAtXY( x, y, accumulated );
			}
		}
	}

	// DeCarpienter Barrel Distortion from https://www.decarpentier.nl/lens-distortion
	void DeCarpentierLensDistort ( const float strength, const float cylinderRatio ) {
		const float aspectRatio = ( float ) width / ( float ) height;
		const float FoVDegrees = 90.0f;
		const float barrelDistortionHeight = tan( glm::radians( FoVDegrees ) / 2.0f );
		const float scaledHeight = strength * barrelDistortionHeight;
		const float cylAspectRatio = aspectRatio * cylinderRatio;
		const float aspectDiagSq = aspectRatio * aspectRatio + 1.0f;
		const float diagSq = scaledHeight * scaledHeight * aspectDiagSq;

		// cached copy of the original image data
		const Image2< imageType, numChannels > cachedCopy( width, height, GetImageDataBasePtr() );

		// iterate through all the pixels
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {

				// calculate the normalized pixel coordinates
				const vec2 normalizedPosition = vec2( ( float ) x / ( float ) width, ( float ) y / ( float ) height );

				// remap tc from [0..1] to [-1..1]
				const vec2 signedUV = 2.0f * normalizedPosition - vec2( 1.0f );

				// clamp to minimum value of 1.0 to prevent degenerate case
				const float z = glm::max( 0.5f * sqrt( diagSq - 1.0f ) + 0.5f, 1.0f );
				const float ny = ( z - 1.0f ) / ( cylAspectRatio * cylAspectRatio + 1.0f );
				const vec2 vUVDot = sqrt( ny ) * vec2( cylAspectRatio, 1.0f ) * signedUV;
				const vec3 vUV = vec3( 0.5f, 0.5f, 1.0f ) * z + vec3( -0.5f, -0.5f, 0.0f ) + vec3( normalizedPosition.xy(), 0.0f ); // is this correct? should be signedUV?
				const vec3 tempTC = dot( vUVDot, vUVDot ) * vec3( -0.5f, -0.5f, -1.0f ) + vUV;
				const vec2 sampleLocation = tempTC.xy() / tempTC.z;

				// sample from the cached copy and write to the current data
				SetAtXY( x, y, cachedCopy.Sample( sampleLocation, samplerType_t::LINEAR_FILTER ) );
			}
		}
	}

	void BlendOverConstantColor ( color background ) {
		// use the alpha channel in the existing image, alpha blend every pixel in the image over this background color value

	}

//======= Access to Internal Data =====================================================================================

	bool BoundsCheck ( uint32_t x, uint32_t y ) const {
		// are the given indices inside the image?
		return !( x >= width || x < 0 || y >= height || y < 0 );
	}

	color GetAtXY ( uint32_t x, uint32_t y ) const {
		color col;
		if ( BoundsCheck( x, y ) ) {
			const size_t baseIndex = ( x + y * width ) * numChannels;
			for ( uint8_t c { 0 }; c < numChannels; c++ ) // populate values
				col[ c ] = data[ baseIndex + c ];
		}
		return col;
	}

	void SetAtXY ( uint32_t x, uint32_t y, color col ) {
		if ( BoundsCheck( x, y ) ) {
			const size_t baseIndex = ( x + y * width ) * numChannels;
			for ( uint8_t c { 0 }; c < numChannels; c++ ) // populate values
				data[ baseIndex + c ] = col[ c ];
		} // else { cout << "Out of Bounds Write :(\n"; }
	}

	// getting filtered samples
	enum samplerType_t {
		NEAREST_FILTER,
		LINEAR_FILTER,
		CUBIC_LAGRANGE_FILTER,
		CUBIC_HERMITE_FILTER,
		SIN_FILTER
	};

	vec4 CubicLagrangeWeight ( vec4 A, vec4 B, vec4 C, vec4 D, float t ) {
		// cubic lagrange weights for cubic interpolation
		const float c_x0 = -1.0f;
		const float c_x1 =  0.0f;
		const float c_x2 =  1.0f;
		const float c_x3 =  2.0f;
		return A * (
			( t - c_x1 ) / ( c_x0 - c_x1 ) *
			( t - c_x2 ) / ( c_x0 - c_x2 ) *
			( t - c_x3 ) / ( c_x0 - c_x3 )
		) + B * (
			( t - c_x0 ) / ( c_x1 - c_x0 ) *
			( t - c_x2 ) / ( c_x1 - c_x2 ) *
			( t - c_x3 ) / ( c_x1 - c_x3 )
		) + C * (
			( t - c_x0 ) / ( c_x2 - c_x0 ) *
			( t - c_x1 ) / ( c_x2 - c_x1 ) *
			( t - c_x3 ) / ( c_x2 - c_x3 )
		) + D * (
			( t - c_x0 ) / ( c_x3 - c_x0 ) *
			( t - c_x1 ) / ( c_x3 - c_x1 ) *
			( t - c_x2 ) / ( c_x3 - c_x2 )
		);
	}

	vec4 CubicHermiteWeight ( vec4 A, vec4 B, vec4 C, vec4 D, float t ) {
		// hermite weights for cubic interpolation
		float t2 = t * t;
		float t3 = t * t * t;
		vec4 a = -A / 2.0f + ( 3.0f * B ) / 2.0f - ( 3.0f * C ) / 2.0f + D / 2.0f;
		vec4 b = A - ( 5.0f * B ) / 2.0f + 2.0f * C - D / 2.0f;
		vec4 c = -A / 2.0f + C / 2.0f;
		vec4 d = B;
		return a * t3 + b * t2 + c * t + d;
	}

	// need to be able to get linear filtered samples - potentially higher order interpolation? tbd, would be nice
	color Sample ( vec2 pos, samplerType_t samplerType = LINEAR_FILTER ) const {
		return Sample( pos.x, pos.y, samplerType );
	}

	color Sample ( float x, float y, samplerType_t samplerType = LINEAR_FILTER ) const {
		color c;
		const vec2 sampleLocationInPixelSpace = vec2( x * ( width - 1 ), y * ( height - 1 ) );

		switch ( samplerType ) {
		case NEAREST_FILTER:
			c = GetAtXY( ( uint32_t ) sampleLocationInPixelSpace.x, ( uint32_t ) sampleLocationInPixelSpace.y );
			break;

		case LINEAR_FILTER:
		{
			// figure out the fractional pixel location
			const vec2 floorCoord = glm::floor( sampleLocationInPixelSpace );
			const vec2 fractCoord = glm::fract( sampleLocationInPixelSpace );

			// figure out the four nearest samples
			color samples[ 4 ] = {
				GetAtXY( ( uint32_t ) floorCoord.x, ( uint32_t ) floorCoord.y ),
				GetAtXY( ( uint32_t ) floorCoord.x + 1, ( uint32_t ) floorCoord.y ),
				GetAtXY( ( uint32_t ) floorCoord.x, ( uint32_t ) floorCoord.y + 1 ),
				GetAtXY( ( uint32_t ) floorCoord.x + 1, ( uint32_t ) floorCoord.y + 1 )
			};

			// figure out the output, based on mixing them
			for ( uint8_t channel = 0; channel < numChannels; channel++ ) {
				// horizontal interpolation first
				imageType value1 = ( imageType ) glm::mix( samples[ 0 ][ channel ], samples[ 1 ][ channel ], fractCoord.x );
				imageType value2 = ( imageType ) glm::mix( samples[ 2 ][ channel ], samples[ 3 ][ channel ], fractCoord.x );

				// then the vertical interpolation between those samples
				c[ channel ] = ( imageType ) glm::mix( value1, value2, fractCoord.y );
			}

			break;
		}

		// more methods ( higher order filtering, would be a nice-to-have )

			// catmull-rom / bicubic
				// https://www.decarpentier.nl/2d-catmull-rom-in-4-samples
				// https://www.shadertoy.com/view/MtVGWz
				// http://vec3.ca/bicubic-filtering-in-fewer-taps/

			// lanzcos?

			// iq remapping linear filtering
			// https://iquilezles.org/articles/texture/
			// https://www.shadertoy.com/view/XsfGDn
				// mentions raised-cosine filter https://en.wikipedia.org/wiki/Raised-cosine_filter

		// lagrange / hermite demofox impl from https://www.shadertoy.com/view/MllSzX
		case CUBIC_LAGRANGE_FILTER:
		{

			break;
		}

		case CUBIC_HERMITE_FILTER:
		{

			break;
		}

		case SIN_FILTER:
		{
			// https://www.shadertoy.com/view/XdGXWt this is interesting
			break;
		}

		default:
			break;
		}

		return c;
	}

	imageType GetPixelMin ( channel in ) const {
		imageType currentMin = std::numeric_limits< imageType >::max();
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				currentMin = std::min( GetAtXY( x, y )[ in ], currentMin );
			}
		}
		return currentMin;
	}

	imageType GetPixelMax ( channel in ) const {
		imageType currentMax = std::numeric_limits< imageType >::min();
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				currentMax = std::max( GetAtXY( x, y )[ in ], currentMax );
			}
		}
		return currentMax;
	}

	color AverageColor () const {
		double sums[ numChannels ] = { 0.0 };
		for ( uint32_t y { 0 }; y < height; y++ ) {
			for ( uint32_t x { 0 }; x < width; x++ ) {
				color pixelData = GetAtXY( x, y );
				for ( uint8_t i { 0 }; i < numChannels; i++ ) {
					sums[ i ] += pixelData[ i ];
				}
			}
		}
		color avg;
		const double numPixels = width * height;
		for ( uint8_t i { 0 }; i < numChannels; i++ ) {
			avg[ i ] = sums[ i ] / numPixels;
		}
		return avg;
	}

	uint32_t Width () const { return width; }
	uint32_t Height () const { return height; }

	// tbd, need to make sure this works for passing texture data to GPU
	const imageType* GetImageDataBasePtr () const	{ return data.data(); }
		imageType* GetImageDataBasePtr () 			{ return data.data(); }

	std::vector< imageType >* GetData() {
		return &data;
	}

private:

//===== Internal Data =================================================================================================

	// image dimensions
	uint32_t width = 0;
	uint32_t height = 0;

	// image data
	std::vector< imageType > data;

//===== Loader Functions == ( Accessed via Load() ) ===================================================================

	// this will handle a number of different file extensions ( png, jpg, etc )
	bool LoadSTB_img ( string path ) {
		std::vector< uint8_t > loadedData;
		int w,h,n=w=h=0;
		unsigned char *image = stbi_load( path.c_str(), &w, &h, &n, STBI_rgb_alpha );
		data.resize( 0 );
		width = ( uint32_t ) w;
		height = ( uint32_t ) h;
		const uint32_t numPixels = width * height;
		data.reserve( numPixels * numChannels );
		const bool uintType = std::is_same< uint8_t, imageType >::value;
		// maybe need to rethink this, if I want to have it handle e.g. single channel load correctly
		for ( uint32_t i = 0; i < numPixels; i++ ) {
			data.push_back( uintType ? image[ i * numChannels + 0 ] : image[ i * numChannels + 0 ] / 255.0f );
			data.push_back( uintType ? image[ i * numChannels + 1 ] : image[ i * numChannels + 1 ] / 255.0f );
			data.push_back( uintType ? image[ i * numChannels + 2 ] : image[ i * numChannels + 2 ] / 255.0f );
			data.push_back( n == 4 ? ( uintType ? image[ i * numChannels + 3 ] : image[ i * numChannels + 3 ] / 255.0f ) : uintType ? 255 : 1.0f );
		}
		return true;
	}

	// preferred loader for .png
	bool LoadLodePNG ( string path ) {
		std::vector< uint8_t > loaded;
		unsigned error = lodepng::decode( loaded, width, height, path.c_str() );
		if ( !error ) {
			const size_t num = width * height * numChannels;
			data.reserve( num );
			for ( size_t idx = 0; idx < num; idx++ ) {
				// uint image type, use data directly, float needs to be remapped to [0.0, 1.0]
				data.push_back( std::is_same< uint8_t, imageType >::value ? loaded[ idx ] : loaded[ idx ] / 255.0f );
			}
			return true;
		} else {
			cout << "lodepng load error: " << lodepng_error_text( error ) << endl;
			return false;
		}
	}

	// high bit depth images, .exr format
	bool LoadTinyEXR ( string path ) {
		float* out; // width * height * RGBA
		const char* errorCode = NULL;
		int w, h;
		int ret = LoadEXR( &out, &w, &h, path.c_str(), &errorCode );
		if ( ret != TINYEXR_SUCCESS ) {
			if ( errorCode ) {
				fprintf( stderr, "ERR : %s\n", errorCode );
				FreeEXRErrorMessage( errorCode ); // release memory of error message.
			}
		} else {
			width = w; height = h;
			data.resize( width * height * 4 );
			const bool uintType = std::is_same< uint8_t, imageType >::value;
			for ( size_t idx = 0; idx < width * height * numChannels; idx++ ) {
				// copy to the image data
				data[ idx ] = uintType ? out[ idx ] * 255.0f : out[ idx ];
			}
			free( out ); // release memory of image data
		}
		return ( ret == TINYEXR_SUCCESS );
	}

//===== Save Functions == ( Accessed via Save() ) =====================================================================

	bool SaveSTB_img ( string path ) const {
		// TODO: figure out return value semantics for error reporting, it's an int, I didn't read the header very closely
		if ( std::is_same< uint8_t, imageType >::value ) {
			return stbi_write_png( path.c_str(), width, height, 8, &data[ 0 ], width * numChannels );
		} else { // float type
			std::vector< uint8_t > remappedData;
			remappedData.reserve( data.size() );
			for ( size_t i { 0 }; i < data.size(); i += numChannels ) {
				for ( uint32_t c { 0 }; c < numChannels; c++ ) {
					remappedData.push_back( std::clamp( data[ i + c ] * 255.0f, 0.0f, 255.0f ) );
				}
				for ( uint32_t c { numChannels }; c < 4; c++ ) {
					remappedData.push_back( c == 3 ? 255 : 0 );
				}
			}
			return stbi_write_png( path.c_str(), width, height, 8, &remappedData[ 0 ], width * numChannels );
		}
	}

	bool SaveLodePNG ( string path ) const {
		const bool uintType = std::is_same< uint8_t, imageType >::value;
		if ( uintType ) {
			unsigned error = lodepng::encode( path.c_str(), ( uint8_t* ) data.data(), width, height );
			if ( !error ) {
				return true;
			} else {
				std::cout << "lodepng save error: " << lodepng_error_text( error ) << std::endl;
				return false;
			}
		} else {
			// remap the float data to uints before saving
			std::vector< uint8_t > remappedData;
			remappedData.reserve( data.size() );
			for ( size_t i { 0 }; i < data.size(); i += numChannels ) {
				for ( uint32_t c { 0 }; c < numChannels; c++ ) {
					remappedData.push_back( std::clamp( data[ i + c ] * 255.0f, 0.0f, 255.0f ) );
				}
				for ( uint32_t c { numChannels }; c < 4; c++ ) {
					remappedData.push_back( c == 3 ? 255 : 0 );
				}
			}
			unsigned error = lodepng::encode( path.c_str(), ( uint8_t* ) remappedData.data(), width, height );
			if ( !error ) {
				return true;
			} else {
				std::cout << "lodepng save error: " << lodepng_error_text( error ) << std::endl;
				return false;
			}
		}
	}

	bool SaveTinyEXR ( string path ) const {
		EXRHeader header;
		EXRImage image;
		InitEXRHeader( &header );
		InitEXRImage( &image );

		// maybe at some point rethink this - this kind of assumes 4-channel float
		image.num_channels = 4;

		std::vector<float> images[ 4 ];
		images[ 0 ].resize( width * height );
		images[ 1 ].resize( width * height );
		images[ 2 ].resize( width * height );
		images[ 3 ].resize( width * height );

		// Split RGBARGBARGBA... into R, G, B, A layers
		for ( size_t i = 0; i < width * height; i++ ) {
			for ( int c { 0 }; c < numChannels; c++ ) {
				images[ c ][ i ] = data[ numChannels * i + c ];
			}
			for ( int c { numChannels }; c < 4; c++ ) {
				images[ c ][ i ] = ( c == 3 ) ? 1.0f : 0.0f;
			}
		}

		float* image_ptr[ 4 ];
		image_ptr[ 0 ] = &( images[ 3 ].at( 0 ) ); // A
		image_ptr[ 1 ] = &( images[ 2 ].at( 0 ) ); // B
		image_ptr[ 2 ] = &( images[ 1 ].at( 0 ) ); // G
		image_ptr[ 3 ] = &( images[ 0 ].at( 0 ) ); // R

		image.images = ( unsigned char** ) image_ptr;
		image.width = width;
		image.height = height;

		header.num_channels = 4;
		header.channels = ( EXRChannelInfo * ) malloc( sizeof( EXRChannelInfo ) * header.num_channels );
		// Must be (A)BGR order, since most of EXR viewers expect this channel order.
		strncpy( header.channels[ 0 ].name, "A", 255 ); header.channels[ 0 ].name[ strlen( "A" ) ] = '\0';
		strncpy( header.channels[ 1 ].name, "B", 255 ); header.channels[ 1 ].name[ strlen( "B" ) ] = '\0';
		strncpy( header.channels[ 2 ].name, "G", 255 ); header.channels[ 2 ].name[ strlen( "G" ) ] = '\0';
		strncpy( header.channels[ 3 ].name, "R", 255 ); header.channels[ 3 ].name[ strlen( "R" ) ] = '\0';

		header.pixel_types = ( int * ) malloc( sizeof( int ) * header.num_channels );
		header.requested_pixel_types = ( int * ) malloc( sizeof( int ) * header.num_channels );
		for ( int i = 0; i < header.num_channels; i++ ) {
			header.pixel_types[ i ] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
			// header.requested_pixel_types[ i ] = TINYEXR_PIXELTYPE_HALF; // pixel type of output image to be stored in .EXR
			header.requested_pixel_types[ i ] = TINYEXR_PIXELTYPE_FLOAT;
		}

		const char* err = NULL; // or nullptr in C++11 or later.
		int ret = SaveEXRImageToFile( &image, &header, path.c_str(), &err );
		if ( ret != TINYEXR_SUCCESS ) {
			fprintf( stderr, "Save EXR err: %s\n", err );
			FreeEXRErrorMessage( err ); // free's buffer for an error message
		}

		// free( rgb );
		free( header.channels );
		free( header.pixel_types );
		free( header.requested_pixel_types );
		return ret;
	}
};

//===== Common Usage Aliases ==========================================================================================
// standard 8bpc color + alpha
typedef Image2< uint8_t, 4 > Image_4U;
typedef Image2< uint8_t, 4 >::color color_4U;

// 1 channel float e.g. Depth
typedef Image2< float, 1 > Image_1F;
typedef Image2< float, 1 >::color color_1F;

// 4 channel float e.g. HDR color
typedef Image2< float, 4 > Image_4F;
typedef Image2< float, 4 >::color color_4F;


#endif // IMAGE2_H