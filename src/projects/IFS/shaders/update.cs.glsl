#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 2, r32ui ) uniform uimage2D ifsAccumulatorR;
layout( binding = 3, r32ui ) uniform uimage2D ifsAccumulatorG;
layout( binding = 4, r32ui ) uniform uimage2D ifsAccumulatorB;

layout( binding = 0, std430 ) buffer maxBuffer { uint maxCount; };

#include "mathUtils.h"
#include "random.h"
#include "complexNumbers.glsl.h"
uniform int wangSeed;

float rand() { return 2.0f * ( NormalizedRandomFloat() - 0.5f ); }

uniform mat3 tridentMatrix;
uniform float scale;
uniform vec2 offset;

// 1 channel swizzle
#define SWIZZLE_X 1
#define SWIZZLE_Y 2
#define SWIZZLE_Z 3

float InputSwizzle1D ( vec3 p, int pick ) {
	float result;
	switch ( pick ) {
		case SWIZZLE_X: result = p.x; break;
		case SWIZZLE_Y: result = p.y; break;
		case SWIZZLE_Z: result = p.z; break;
	}
	return result;
}

vec3 OutputSwizzle1D ( vec3 p, float val, int pick ) {
	switch ( pick ) {
		case SWIZZLE_X: p.x = val; break;
		case SWIZZLE_Y: p.y = val; break;
		case SWIZZLE_Z: p.z = val; break;
	}
	return p;
}

// 2 channel swizzles
#define SWIZZLE_XY 4
#define SWIZZLE_YX 5
#define SWIZZLE_YZ 6
#define SWIZZLE_ZY 7
#define SWIZZLE_XZ 8
#define SWIZZLE_ZX 9

vec2 InputSwizzle2D ( vec3 p, int pick ) {
	vec2 result;
	switch ( pick ) {
		case SWIZZLE_XY: result = p.xy; break;
		case SWIZZLE_YX: result = p.yx; break;
		case SWIZZLE_YZ: result = p.yz; break;
		case SWIZZLE_ZY: result = p.zy; break;
		case SWIZZLE_XZ: result = p.xz; break;
		case SWIZZLE_ZX: result = p.zx; break;
	}
	return result;
}

vec3 OutputSwizzle2D ( vec3 p, vec2 val, int pick ) {
	switch ( pick ) {
		case SWIZZLE_XY: p.xy = val; break;
		case SWIZZLE_YX: p.yx = val; break;
		case SWIZZLE_YZ: p.yz = val; break;
		case SWIZZLE_ZY: p.zy = val; break;
		case SWIZZLE_XZ: p.xz = val; break;
		case SWIZZLE_ZX: p.zx = val; break;
	}
	return p;
}

// 3 channel swizzles
#define SWIZZLE_XYZ 10
#define SWIZZLE_YXZ 11
#define SWIZZLE_YZX 12
#define SWIZZLE_ZYX 13
#define SWIZZLE_XZY 14
#define SWIZZLE_ZXY 15

vec3 InputSwizzle3D ( vec3 p, int pick ) {
	vec3 result;
	switch ( pick ) {
		case SWIZZLE_XYZ: result = p.xyz; break;
		case SWIZZLE_YXZ: result = p.yxz; break;
		case SWIZZLE_YZX: result = p.yzx; break;
		case SWIZZLE_ZYX: result = p.zyx; break;
		case SWIZZLE_XZY: result = p.xzy; break;
		case SWIZZLE_ZXY: result = p.zxy; break;
	}
	return result;
}

vec3 OutputSwizzle3D ( vec3 p, vec3 val, int pick ) {
	switch ( pick ) {
		case SWIZZLE_XYZ: p.xyz = val; break;
		case SWIZZLE_YXZ: p.yxz = val; break;
		case SWIZZLE_YZX: p.yzx = val; break;
		case SWIZZLE_ZYX: p.zyx = val; break;
		case SWIZZLE_XZY: p.xzy = val; break;
		case SWIZZLE_ZXY: p.zxy = val; break;
	}
	return p;
}

// operation defines
#define CX_MUL 0
#define CX_DIV 1
#define CX_MODULUS 2
#define CX_CONJ 3
#define CX_ARG 4
#define CX_SIN 5
#define CX_COS 6
#define CX_SQRT 7
#define CX_TAN 8
#define CX_LOG 9
#define CX_MOBIUS 10
#define CX_Z_PLUS_ONE_OVER_Z 11
#define CX_Z_SQUARED_PLUS_C 12
#define CX_SIN_OF_ONE_OVER_Z 13
#define CX_SUB 14
#define CX_ADD 15
#define CX_ABS 16
#define CX_TO_POLAR 17
#define CX_POW 18
#define OFFSET1D 19
#define OFFSET2D 20
#define OFFSET3D 21
#define SCALE1D 22
#define SCALE2D 23
#define SCALE3D 24
#define ROTATE2D 25

struct entry_t {
	vec4 op;
	vec4 args;
	vec4 color;
};

uniform int numTransforms;
layout( binding = 1, std430 ) buffer dataBuffer { entry_t data[]; };

// this could be done more efficiently via code gen, but I like how general this is

vec3 ApplyTransform ( vec3 p, inout vec3 color ) {
	// selecting a transform out of the buffer
	entry_t selected = data[ int( floor( NormalizedRandomFloat() * numTransforms ) ) ];

	// apply the color manipulation
	color *= selected.color.rgb;

	const int selectedTransform = int( selected.op.x );
	const int inputSwizzle = int( selected.op.y );
	const int outputSwizzle = int( selected.op.z );

	// apply the point transform
	switch ( selectedTransform ) {
		case CX_MUL: // 2 channel -> 2 channel, with 2 additional arguments
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_mul( temp, selected.args.xy );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_DIV: // 2 channel -> 2 channel, with 2 additional arguments
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_div( temp, selected.args.xy );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_MODULUS: // 2 channel -> 1 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			float result = cx_modulus( temp );
			p = OutputSwizzle1D( p, result, outputSwizzle );
			break;
		}

		case CX_CONJ: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_conj( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_ARG: // 2 channel -> 1 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			float result = cx_arg( temp );
			p = OutputSwizzle1D( p, result, outputSwizzle );
			break;
		}

		case CX_SIN: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_sin( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_COS: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_cos( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_SQRT: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_sqrt( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_TAN: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_tan( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_LOG: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_log( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_MOBIUS: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_mobius( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_Z_PLUS_ONE_OVER_Z: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_z_plus_one_over_z( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_Z_SQUARED_PLUS_C: // 2 channel -> 2 channel, with 2 additional arguments
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_z_squared_plus_c( temp, selected.args.xy );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_SIN_OF_ONE_OVER_Z: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_sin_of_one_over_z( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_SUB: // 2 channel -> 2 channel, with 2 additional arguments
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_sub( temp, selected.args.xy );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_ADD: // 2 channel -> 2 channel, with 2 additional arguments
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_add( temp, selected.args.xy );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_ABS: // 2 channel -> 1 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			float result = cx_abs( temp );
			p = OutputSwizzle1D( p, result, outputSwizzle );
			break;
		}

		case CX_TO_POLAR: // 2 channel -> 2 channel
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_to_polar( temp );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case CX_POW: // 2 channel -> 2 channel, with 1 additional argument
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = cx_pow( temp, selected.args.x );
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case OFFSET1D: // 1 channel -> 1 channel, with 1 additional argument
		{
			float temp = InputSwizzle1D( p, inputSwizzle );
			temp = temp + selected.args.x;
			p = OutputSwizzle1D( p, temp, outputSwizzle );
			break;
		}

		case OFFSET2D: // 2 channel -> 2 channel, with 2 additional arguments
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = temp + selected.args.xy;
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case OFFSET3D: // 3 channel -> 3 channel, with 3 additional arguments
		{
			vec3 temp = InputSwizzle3D( p, inputSwizzle );
			temp = temp + selected.args.xyz;
			p = OutputSwizzle3D( p, temp, outputSwizzle );
			break;
		}

		case SCALE1D: // 1 channel -> 1 channel, with 1 additional argument
		{
			float temp = InputSwizzle1D( p, inputSwizzle );
			temp = temp * selected.args.x;
			p = OutputSwizzle1D( p, temp, outputSwizzle );
			break;
		}

		case SCALE2D: // 2 channel -> 2 channel, with 2 additional arguments
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = temp * selected.args.xy;
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		case SCALE3D: // 3 channel -> 3 channel, with 3 additional arguments
		{
			vec3 temp = InputSwizzle3D( p, inputSwizzle );
			temp = temp * selected.args.xyz;
			p = OutputSwizzle3D( p, temp, outputSwizzle );
			break;
		}

		case ROTATE2D: // 2 channel -> 2 channel, with 1 additional arguments
		{
			vec2 temp = InputSwizzle2D( p, inputSwizzle );
			temp = Rotate2D( selected.args.x ) * temp;
			p = OutputSwizzle2D( p, temp, outputSwizzle );
			break;
		}

		default: break;
	}

	return p;
}

ivec2 map3DPointTo2D( vec3 p ) {
	const ivec2 is = ivec2( imageSize( ifsAccumulatorR ).xy );
	const float ratio = is.x / is.y;
	const mat3 invTridentMatrix = inverse( tridentMatrix );

	// still need to figure out the particulars of the order of operations here
	p = p * scale;
	// p.xy += ( invTridentMatrix * vec3( offset, 0.0f ) ).xy;
	p = tridentMatrix * p;

	return ivec2(
		RangeRemapValue( p.x + offset.x * scale, -ratio, ratio, 0.0f, float( is.x ) ) + NormalizedRandomFloat(),
		RangeRemapValue( p.y + offset.y * scale, -1.0f, 1.0f, 0.0f, float( is.y ) ) + NormalizedRandomFloat() );
}

uniform int initMode;
vec3 GetInitialPointPosition () {
	vec3 p = vec3( 0.0f );
	switch ( initMode ) {
		case 0: // uniform rectangular volume
			p = vec3( rand() * 10.0f, rand() * 1.618f, rand() * 4.0f );
		break;

		case 1: // extruded 2d shape
			p = vec3( HeartOffset() * rand(), cos( rand() ) );
		break;

		case 2: // spherical shell
			p = RandomUnitVector();
		break;

		case 3: // grid of cubes
			p = 3.0f * floor( 10.0f * vec3( rand(), rand(), rand() ) ) + 0.1f * vec3( rand(), rand(), rand() );
		break;

		case 4: // grid of spherical shells
			p = 3.0f * floor( 10.0f * vec3( rand(), rand(), rand() ) ) + 0.1f * RandomUnitVector();
		break;

		case 5: // flat disk
			p = vec3( CircleOffset(), 0.0f );
		break;

		case 6: // flat hexagon
			p = vec3( UniformSampleHexagon(), 0.0f );
		break;

		case 7: // between two speheres
			p = RandomUnitVector() * ( NormalizedRandomFloat() + 0.5f );
		break;

		default:
		break;
	}
	return p;
}

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	seed = wangSeed + writeLoc.x * 42069 + writeLoc.y * 69420;

	vec3 p = GetInitialPointPosition();

	vec3 color = vec3( 1.0f );

	for ( int i = 0; i < 30; i++ ) {
		p = ApplyTransform( p, color );

		// dof offset
		// p.xy += 0.01f * UniformSampleHexagon() * ( p.z + 0.5f );

		// write the data to the image
		ivec2 location = map3DPointTo2D( p );

		uvec3 amt = uvec3( 100.0f * color );
		atomicMax( maxCount, imageAtomicAdd( ifsAccumulatorR, location, amt.r ) + amt.r );
		atomicMax( maxCount, imageAtomicAdd( ifsAccumulatorG, location, amt.g ) + amt.g );
		atomicMax( maxCount, imageAtomicAdd( ifsAccumulatorB, location, amt.b ) + amt.b );
	}
}
