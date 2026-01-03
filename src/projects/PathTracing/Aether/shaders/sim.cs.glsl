#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

//=============================================================================================================================
// RNG seeding...
#include "random.h"
uniform uint wangSeed;
//=============================================================================================================================
// blue noise
uniform ivec2 noiseOffset;
vec4 blueNoiseRef ( ivec2 pos ) {
	pos.x = ( pos.x + noiseOffset.x ) % imageSize( blueNoiseTexture ).x;
	pos.y = ( pos.y + noiseOffset.y ) % imageSize( blueNoiseTexture ).y;
	return imageLoad( blueNoiseTexture, pos ) / 255.0f;
}
//=============================================================================================================================
// the field buffer image/tex
//uniform isampler3D bufferTexX;
//uniform isampler3D bufferTexY;
//uniform isampler3D bufferTexZ;
//uniform isampler3D bufferTexCount;
layout( binding = 2, r32ui ) uniform uimage3D bufferImageX;
layout( binding = 3, r32ui ) uniform uimage3D bufferImageY;
layout( binding = 4, r32ui ) uniform uimage3D bufferImageZ;
layout( binding = 5, r32ui ) uniform uimage3D bufferImageCount;
//=============================================================================================================================
#include "hg_sdf.glsl"
#include "aetherScene.h"
//=============================================================================================================================
float myWavelength = 0.0f;
//=============================================================================================================================
// CDF for the light wavelengths
uniform sampler2D iCDFtex;
float getWavelength ( int lightType ) {
	return texture( iCDFtex, vec2( NormalizedRandomFloat(), ( lightType + 0.5f ) / textureSize( iCDFtex, 0 ).y ) ).r;
}
//=============================================================================================================================
#include "colorspaceConversions.glsl"
#include "intersect.h"
uniform ivec3 dimensions;
//=============================================================================================================================
struct lightSpecGPU {
// less than ideal way to do this...
	vec4 typeVec;		// emitter type, LUT type, 0, 0
	vec4 parameters0;	// varies
	vec4 parameters1;	// varies
	vec4 pad;

	vec4 cachedTypeVec;
	vec4 cachedParameters0;
	vec4 cachedParameters1;
	vec4 pad2;

// so, everything we need to know:
// type of light ( 1 float -> int )
// type of emitter ( 1 float -> int )
// emitter parameterization
// varies by emitter...

// Parameters:
// point emitter has 1x vec3 parameter
// cauchy beam has 2x vec3 parameters and 1x float parameter
// laser disk has 2x vec3 parameters and 1x float parameter
// uniform line emitter has 2x vec3 parameters...
};
layout( binding = 0, std430 ) readonly buffer lightBuffer {
	int lightIStructure[ 1024 ]; // we uniformly sample an index out of this list of 1024 to know which light we want to pick...
	lightSpecGPU lightList[]; // we do not need to know how many lights exist, because it is implicitly encoded in the importance structure's indexing
};
//=============================================================================================================================
void configureLightRay ( out vec3 rO, out vec3 rD, in lightSpecGPU pickedLight ) {
	// let's figure out what wavelength we are
	myWavelength = getWavelength( int( pickedLight.typeVec.y ) );

// generate a new ray, based on the properties of the selected light...
	// what is my starting position, direction?
	rO = vec3( 0.0f ), rD = vec3( 0.0f );
	vec3 x, y;
	vec2 c;
	float frameF = fract( frame );
	switch ( int( pickedLight.typeVec.x ) ) { // based on the type of emitter specified...
		case 0: // point light
		rO = mix( pickedLight.cachedParameters0.xyz, pickedLight.parameters0.xyz, frameF );
		rD = RandomUnitVector();
		break;

		case 1: // cauchy beam
		// emitting from a single point
		// we need to be able to place a jittered target position... it's a 2D offset in the plane whose normal is defined by the beam direction
		createBasis( normalize( mix( pickedLight.cachedParameters1.xyz, pickedLight.parameters1.xyz, frameF ) ), x, y );
		c = rnd_disc_cauchy();
		// rO = pickedLight.parameters0.xyz + 2.0f * ( 16.18f * int( 10.0f * ( NormalizedRandomFloat() ) ) * y + 20.0f * int( 10.0f * ( NormalizedRandomFloat() ) ) * x );
		rO = mix( pickedLight.cachedParameters0.xyz, pickedLight.parameters0.xyz, frameF );
		rD = normalize( mix( pickedLight.cachedParameters0.w, pickedLight.parameters0.w, frameF ) * ( x * c.x + y * c.y ) + mix( pickedLight.cachedParameters1.xyz, pickedLight.parameters1.xyz, frameF ) );
		break;

		case 2: // laser disk
		// similar to above, but using a constant direction value, and using the basis jitter for a scaled disk offset
		createBasis( normalize( mix( pickedLight.cachedParameters1.xyz, pickedLight.parameters1.xyz, frameF ) ), x, y );
		c = CircleOffset();
		// rO = pickedLight.parameters0.xyz + 2.0f * ( 16.18f * int( 6.0f * ( NormalizedRandomFloat() ) ) * y + 20.0f * int( 4.0f * ( NormalizedRandomFloat() ) ) * x ) + pickedLight.parameters0.w * ( x * c.x + y * c.y );
		rO = mix( pickedLight.cachedParameters0.xyz, pickedLight.parameters0.xyz, frameF ) + mix( pickedLight.cachedParameters0.w, pickedLight.parameters0.w, frameF ) * ( x * c.x + y * c.y );
		// emitting along a single direction vector
		rD = normalize( mix( pickedLight.cachedParameters1.xyz, pickedLight.parameters1.xyz, vec3( frameF ) ) );
		break;

		case 3: // uniform line emitter
		rO = mix( mix( pickedLight.cachedParameters0.xyz, pickedLight.parameters0.xyz, frameF ), mix( pickedLight.cachedParameters1.xyz, pickedLight.parameters1.xyz, vec3( frameF ) ), NormalizedRandomFloat() );
		rD = RandomUnitVector();
		break;

		case 4: // line beam
		createBasis( normalize( mix( pickedLight.parameters1.xyz, pickedLight.parameters1.xyz, frameF ) ), x, y );
		rO = mix( pickedLight.cachedParameters0.xyz, pickedLight.parameters0.xyz, frameF ) + mix( pickedLight.cachedParameters0.w, pickedLight.parameters0.w, frameF ) * ( x * NormalizedRandomFloat() * mix( pickedLight.cachedParameters0.w, pickedLight.parameters0.w, frameF ) );
		// emitting along a single direction vector
		rD = normalize( mix( pickedLight.cachedParameters1.xyz, pickedLight.parameters1.xyz, frameF ) + c.y * 0.01f * rnd_disc_cauchy().y );
		break;

		default:
		break;
	}
}

//=============================================================================================================================
/* scene SDF should be part of a header, so we can use it more than one place - eventually that wrapper can include a BVH, too
	we need it:
		1. here - forward pathtrace, light source emits a ray which interacts with one or more scene objects, to populate 3D fluence field
		2. to manage the scene intersections between bounces during the fluence trace for output pixel
			( if we look at glass, our view ray needs to refract accordingly, or our sampling is wrong... we will need to figure out particulars for this, what we use as a wavelength )
		3. potential future renderers...

		environment will expect ( *before* header's #include statement, because it will be used in the code substituted in by the preprocessor ):
			myWavelength should exist... we'll probably either use a random one or we have to do spectral samples for the output trace ( we can, it's not really a problem )
			bool invert toggle should exist - used for refractive objects, and we're assuming that I'm manually managing overlap of scene objects or else accepting artifacts
			...more?
*/

#include "spectrumXYZ.h"

void drawPixel ( ivec3 p, float AAFactor, vec3 XYZColor ) {
	// do the atomic increments for this sample
	ivec3 increment = ivec3(
		int( 16 * XYZColor.r ),
		int( 16 * XYZColor.g ),
		int( 16 * XYZColor.b )
	);
	// maintaining sum + count by doing atomic writes along the ray
	const ivec3 iS = imageSize( bufferImageY ).xyz - ivec3( 1 );
	if ( p == clamp( p, ivec3( 0 ), iS ) ) {
		imageAtomicAdd( bufferImageX, p, increment.x );
		imageAtomicAdd( bufferImageY, p, increment.y );
		imageAtomicAdd( bufferImageZ, p, increment.z );
		imageAtomicAdd( bufferImageCount, p, int( 256 * AAFactor ) );
	}
}

void drawLine ( vec3 p0, vec3 p1, float energyTotal ) {
	// compute the color once for this line
	const vec3 XYZColor = energyTotal * wavelengthColor( myWavelength );

	// figure out where these two endpoints lie, on the field, draw a line between them
	// use 0-255 AA factor as a scalar on the summand, so that we have soft edged rays
	const ivec3 iS = dimensions;

	const vec4 blue = blueNoiseRef( ivec2( gl_GlobalInvocationID.xy ) );
	const vec3 p0i = vec3( ( p0 + iS ) / 2.0f );
	const vec3 p1i = vec3( ( p1 + iS ) / 2.0f );
	const float stepSize = 0.619f + blue.a;

	const vec3 diff = normalize( p0i - p1i );
	const float l = length( p0i - p1i ) + 1.0f;
	float accum = blue.r;

	for ( int i = 0; i < 5000 && accum < l; i++ ) {
		const vec3 p = p1i + diff * accum;
		accum += stepSize + ( ( mod( i, 3 ) == 0 ) ? blue.b : ( mod( i, 2 ) == 0 ) ? blue.r : blue.g );
		drawPixel( ivec3(
			int( p.x + NormalizedRandomFloat() - 0.5f ),
			int( p.y + NormalizedRandomFloat() - 0.5f ),
			int( p.z + NormalizedRandomFloat() - 0.5f ) ),
		1.0f, XYZColor );
	}
}


void main () {
	// pixel location
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// initial wang state
	seed = wangSeed + loc.x * 69696 + loc.y * 8675309;

	// we need to first pick one of the lights out of the light buffer...
	lightSpecGPU lightSpec = lightList[ lightIStructure[ wangHash() % 1024 ] ];

	// wavelength is kept in global scope so it doesn't need to be passed around
	vec3 rO, rD;
	configureLightRay( rO, rD, lightSpec );

	// environment configuration for the forward pathtracing
		// ( energy starts at a maximum and attenuates, when we start from the light source - are these redunant? )
	float transmission = 1.0f;
	float energyTotal = 1.0f;

	// pathtracing loop
	const int maxBounces = 128;
	float previousIoR = 1.0f;
	for ( int i = 0; i < maxBounces; i++ ) {
		// trace the ray against the scene
		intersectionResult intersection = sceneTrace( rO, rD, myWavelength );

		// draw a line to the scene intersection point
		drawLine( rO, rO + ( intersection.materialType == NOHIT ? maxDistance : intersection.dist ) * rD, energyTotal );

		// russian roulette termination
		if ( NormalizedRandomFloat() > energyTotal ) { break; }
		energyTotal *= 1.0f / energyTotal; // rr compensation term

		// attenuate by the surface albedo
		transmission *= intersection.albedo;
		energyTotal *= intersection.albedo;

		rO = rO + ( intersection.dist - epsilon ) * rD;

		// update ray origin + new ray direction logic
		switch ( intersection.materialType ) {
			case NOHIT:
//			i = maxBounces; // we're going to break out of the loop
			break;

			case DIFFUSE:
			rD = RandomUnitVector();
			// invert if going into the surface
			if ( dot( rD, intersection.normal ) < 0.0f ) {
				rD = -rD;
			}
			break;

			case METALLIC:
			// todo
			break;

			case MIRROR:
			rD = reflect( rD, intersection.normal );
//			rO += intersection.normal * epsilon * 3.0f;
			break;

			// below this point, we have to consider the IoR for the specific form of glass... because we precomputed all the
			// varying behavior already, we can just treat it uniformly, only need to consider frontface/backface for inversion
			default:
			rO -= intersection.normal * epsilon * 3.0f;
			intersection.IoR = intersection.frontFacing ? ( 1.0f / intersection.IoR ) : ( intersection.IoR ); // "reverse" back to physical properties for IoR
			float cosTheta = min( dot( -normalize( rD ), intersection.normal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( intersection.IoR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, intersection.IoR ) > NormalizedRandomFloat() ) {
				rD = normalize( mix( reflect( normalize( rD ), intersection.normal ), RandomUnitVector(), intersection.roughness ) );
			} else {
				rD = normalize( mix( refract( normalize( rD ), intersection.normal, intersection.IoR ), RandomUnitVector(), intersection.roughness ) );
			}
			break;
		}

	}

}
