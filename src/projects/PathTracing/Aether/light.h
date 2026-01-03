#pragma once

// specifying the distribution of emitted rays
inline const char* emitterTypes[] = {
	/* 0 */ "Point",
	/* 1 */ "Cauchy Beam",
	/* 2 */ "Laser Disk",
	/* 3 */ "Uniform Line Emitter",
	"Line Beam",
	"Image Light" };
inline const int numEmitters = sizeof( emitterTypes ) / sizeof( emitterTypes[ 0 ] );

// specifying the LUT which will be used for selecting wavelengths
inline const char* LUTFilenames[] = {
	"RedLED",
	"GreenLED",
	"BlueLED",
	/* 0 */ "AmberLED",
	/* 1 */ "2700kLED",
	/* 2 */ "6500kLED",
	/* 3 */ "Candle",
	/* 4 */ "Flourescent1",
	/* 5 */ "Flourescent2",
	/* 6 */ "Flourescent3",
	/* 7 */ "Halogen",
	/* 8 */ "HPMercury",
	/* 9 */ "HPSodium1",
	/* 10 */ "HPSodium2",
	/* 11 */ "LPSodium",
	/* 12 */ "Incandescent",
	/* 13 */ "MetalHalide1",
	/* 14 */ "MetalHalide2",
	/* 15 */ "SkyBlueLED",
	/* 16 */ "SulphurPlasma",
	/* 17 */ "Sunlight",
	/* 18 */ "Xenon" };
inline const int numLUTs = sizeof( LUTFilenames ) / sizeof( LUTFilenames[ 0 ] );

// struct defining the state for the data-driven interaction with a user-specified light
struct lightSpec {
	int emitterType;	// type of light emitter
	int pickedLUT;		// type of light source
	float power;		// 'power' - this is an arbitrary scale factor, and only matters relative to other light sources
	vec4 emitterParams[ 2 ]; // emitter parameterization - 8 floats should be sufficient

	int cachedEmitterType;
	int cachedPickedLUT;
	float cachedPower;		// unfortunately this will require rebuilding the importance sampling structure
	vec4 cachedEmitterParams[ 2 ];

	char label[ 256 ];

	vec3 rotationAxis;

	lightSpec() {
		emitterType = 0;
		pickedLUT = 4;
		power = 1.0f;
		emitterParams[ 0 ] = emitterParams[ 1 ] = vec4( 0.0f );

		cachedEmitterParams[ 0 ] = cachedEmitterParams[ 1 ] = vec4( 0.0f );
		cachedPower = 1.0f;
		cachedEmitterType = 0;
		cachedPickedLUT = 0;

		rng axisPicker( -1.0f, 1.0f );
		rotationAxis = glm::normalize( vec3( axisPicker(), axisPicker(), axisPicker() ) );

		// reset the label string
		sprintf( label, "Default Light Label" );
	}

};