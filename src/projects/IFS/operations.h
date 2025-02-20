#include "../../engine/includes.h"

// for now, only handling swizzles that have no repeats

enum swizzle {
	// for initialization type operations? tbd
	SWIZZLE_NONE = 0,

	// 1 channel swizzles
	SWIZZLE_X = 1,
	SWIZZLE_Y = 2,
	SWIZZLE_Z = 3,

	// 2 channel swizzles
	SWIZZLE_XY = 4,
	SWIZZLE_YX = 5,
	SWIZZLE_YZ = 6,
	SWIZZLE_ZY = 7,
	SWIZZLE_XZ = 8,
	SWIZZLE_ZX = 9,

	// 3 channel swizzles
	SWIZZLE_XYZ = 10,
	SWIZZLE_YXZ = 11,
	SWIZZLE_YZX = 12,
	SWIZZLE_ZYX = 13,
	SWIZZLE_XZY = 14,
	SWIZZLE_ZXY = 15,

	// how many
	SWIZZLE_COUNT = 16
};

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
#define NUM_OPERATIONS 26

swizzle Get1DSwizzle () {
	rngi pick = rngi( 1, 3 );
	return swizzle( pick() );
}

swizzle Get2DSwizzle () {
	rngi pick = rngi( 4, 9 );
	return swizzle( pick() );
}

swizzle Get3DSwizzle () {
	rngi pick = rngi( 10, 15 );
	return swizzle( pick() );
}

string GetStringForSwizzle ( swizzle in ) {
	string result;
	switch ( in ) {
		case SWIZZLE_X: result = string( "X" ); break;
		case SWIZZLE_Y: result = string( "Y" ); break;
		case SWIZZLE_Z: result = string( "Z" ); break;
		case SWIZZLE_XY: result = string( "XY" ); break;
		case SWIZZLE_YX: result = string( "YX" ); break;
		case SWIZZLE_YZ: result = string( "YZ" ); break;
		case SWIZZLE_ZY: result = string( "ZY" ); break;
		case SWIZZLE_XZ: result = string( "XZ" ); break;
		case SWIZZLE_ZX: result = string( "ZX" ); break;
		case SWIZZLE_XYZ: result = string( "XYZ" ); break;
		case SWIZZLE_YXZ: result = string( "YXZ" ); break;
		case SWIZZLE_YZX: result = string( "YZX" ); break;
		case SWIZZLE_ZYX: result = string( "ZYX" ); break;
		case SWIZZLE_XZY: result = string( "XZY" ); break;
		case SWIZZLE_ZXY: result = string( "ZXY" ); break;
		default: break;
	}
	return result;
}

// structure of an operation (mostly used for imgui menu layout)
struct operationTemplate_t {
	string identifier;
	uint32_t inputSize;
	uint32_t outputSize;
	uint32_t numArgs;

	operationTemplate_t( string in_identifier, uint32_t in_inputSize, uint32_t in_outputSize, uint32_t in_numArgs ) :
		identifier( in_identifier ), inputSize( in_inputSize ), outputSize( in_outputSize ), numArgs( in_numArgs ) {}
};

// specific instantiation of an operation
struct operation_t {
	uint32_t index; // what operation is this
	swizzle inputSwizzle;
	swizzle outputSwizzle;
	vec4 args;
	vec4 color;
};

static const std::vector< operationTemplate_t > operationList = {
	operationTemplate_t( "cx_mul", 2, 2, 2 ),
	operationTemplate_t( "cx_div", 2, 2, 2 ),
	operationTemplate_t( "cx_modulus", 2, 1, 0 ),
	operationTemplate_t( "cx_conj", 2, 2, 0 ),
	operationTemplate_t( "cx_arg", 2, 1, 0 ),
	operationTemplate_t( "cx_sin", 2, 2, 0 ),
	operationTemplate_t( "cx_cos", 2, 2, 0 ),
	operationTemplate_t( "cx_sqrt", 2, 2, 0 ),
	operationTemplate_t( "cx_tan", 2, 2, 0 ),
	operationTemplate_t( "cx_log", 2, 2, 0 ),
	operationTemplate_t( "cx_mobius", 2, 2, 0 ),
	operationTemplate_t( "cx_z_plus_one_over_z", 2, 2, 0 ),
	operationTemplate_t( "cx_z_squared_plus_c", 2, 2, 2 ),
	operationTemplate_t( "cx_sin_of_one_over_z", 2, 2, 0 ),
	operationTemplate_t( "cx_sub", 2, 2, 2 ),
	operationTemplate_t( "cx_add", 2, 2, 2 ),
	operationTemplate_t( "cx_abs", 2, 1, 0 ),
	operationTemplate_t( "cx_to_polar", 2, 2, 0 ),
	operationTemplate_t( "cx_pow", 2, 2, 1 ),
	operationTemplate_t( "1D Offset", 1, 1, 1 ),
	operationTemplate_t( "2D Offset", 2, 2, 2 ),
	operationTemplate_t( "3D Offset", 3, 3, 3 ),
	operationTemplate_t( "1D Scale", 1, 1, 1 ),
	operationTemplate_t( "2D Scale", 2, 2, 2 ),
	operationTemplate_t( "3D Scale", 3, 3, 3 ),
	operationTemplate_t( "2D Rotate", 2, 2, 1 )
};

operation_t GetRandomOperation () {
	rngi pick = rngi( 0, NUM_OPERATIONS - 1 );
	const int picked = pick();

	operationTemplate_t operationTemplate = operationList[ picked ];
	operation_t current;

	current.index = picked;
	switch( operationTemplate.inputSize ) {
		case 1: current.inputSwizzle = Get1DSwizzle(); break;
		case 2: current.inputSwizzle = Get2DSwizzle(); break;
		case 3: current.inputSwizzle = Get3DSwizzle(); break;
		default: break;
	}

	switch( operationTemplate.outputSize ) {
		case 1: current.outputSwizzle = Get1DSwizzle(); break;
		case 2: current.outputSwizzle = Get2DSwizzle(); break;
		case 3: current.outputSwizzle = Get3DSwizzle(); break;
		default: break;
	}

	// initialize the arguments
	rng init = rng( -2.0f, 2.0f );
	current.args = vec4( init(), init(), init(), init() );

	// get a random color
	rng colorInit = rng( 0.0f, 1.0f );
	current.color = vec4( palette::paletteRef( colorInit() ), 1.0f );

	return current;
}

static char * operationLabels[ NUM_OPERATIONS ];
static char * swizzleLabels[ SWIZZLE_COUNT ];

#define INIT_MODES_COUNT 7
static const char * initializationLabels[ INIT_MODES_COUNT ] = {
// more? these are pretty boring, but they still create distinct and interesting behavior
	"Uniform Random in a Cube",
	"Shaped Offset (Heart)",
	"Spherical Shell",
	"Grid Distributed Cubes",
	"Grid Distributed Spherical Shells",
	"Flat Disk",
	"Flat Hexagon"
};

void PopulateLabels () {
	// get the labels
	for ( int i = 0; i < NUM_OPERATIONS; i++ ) {
		operationLabels[ i ] = ( char * ) malloc( 64 ); // whatever
		sprintf( operationLabels[ i ], "%s", operationList[ i ].identifier.c_str() );
	}

	for ( int i = 1; i < SWIZZLE_COUNT; i++ ) {
		string swizz = GetStringForSwizzle( swizzle( i ) );
		swizzleLabels[ i ] = ( char * ) malloc( 16 ); // whatever
		sprintf( swizzleLabels[ i ], "%s", swizz.c_str() );
	}
}