#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "random.h"
#include "noise.h"
#include "oklabConversions.h.glsl"
//=============================================================================================================================
// ray state buffers
#include "rayState2.h.glsl"
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };
layout( binding = 2, std430 ) readonly buffer intersectionBuffer { intersection_t intersectionScratch[]; };
//=============================================================================================================================
// accumulation buffers
layout( binding = 0, r32ui ) uniform uimage2D rTally;
layout( binding = 1, r32ui ) uniform uimage2D gTally;
layout( binding = 2, r32ui ) uniform uimage2D bTally;
layout( binding = 3, r32ui ) uniform uimage2D count;
//=============================================================================================================================
vec3 anglePhong( float a, vec3 n ){
	float r1 = NormalizedRandomFloat();
	float r2 = NormalizedRandomFloat();
	float t = pow( r2, 2.0f / ( 1.0f + a ) );
	float x = cos( 2.0f * 3.14159f * r1 ) * sqrt( 1.0f - t );
	float y = sin( 2.0f * 3.14159f * r1 ) * sqrt( 1.0f - t );
	float z = sqrt( t );
	vec3 W = ( abs( n.x ) > 0.99f ) ? vec3( 0.0f, 1.0f, 0.0f ) : vec3( 1.0f, 0.0f, 0.0f );
	vec3 N = n;
	vec3 T = normalize( cross( N, W ) );
	vec3 B = cross( T, N );
	return normalize( x * T + y * B + z * N );
}
//=============================================================================================================================
// look at these for a potentially better fresnel approximation:
// https://samdriver.xyz/article/refraction-sphere
// https://graphics.stanford.edu/courses/cs148-10-summer/docs/2006--degreve--reflection_refraction.pdf
float Reflectance ( const float cosTheta, const float IoR ) {
	// Use Schlick's approximation for reflectance
	float r0 = ( 1.0f - IoR ) / ( 1.0f + IoR );
	r0 = r0 * r0;
	return r0 + ( 1.0f - r0 ) * pow( ( 1.0f - cosTheta ), 5.0f );
}
//=============================================================================================================================
void main () {
	rayState_t myState = state[ gl_GlobalInvocationID.x ];

	// location of the associated pixel
	const ivec2 loc = GetPixelIndex( myState );
	bool terminate = false;
	const float epsilon = 0.000001f;

	 seed = uint( loc.x ) * 19990625 + uint( loc.y ) * 2399624 + gl_GlobalInvocationID.x * 42069;
//	seed = gl_GlobalInvocationID.x * 42069;

	intersection_t closestIntersection;
//	closestIntersection.data1.x = 0.0f; // supressing warning
	IntersectionReset( closestIntersection );

	// We need to do a gather over the N intersection cantidates to find the closest, similar to the Daedalus impl
	for ( int i = 0; i < NUM_INTERSECTORS; i++ ) {
		const int baseIndex = NUM_INTERSECTORS * int( gl_GlobalInvocationID.x );
		intersection_t temp = intersectionScratch[ baseIndex + i ];
		if ( GetHitDistance( temp ) < GetHitDistance( closestIntersection ) ) {
			closestIntersection = temp; // update with nearest intersection
		}
	}

	// we need to generate a new ray, and continue
	const vec3 origin = GetRayOrigin( myState );
	const vec3 direction = GetRayDirection( myState );
	const float dist = GetHitDistance( closestIntersection );
	const vec3 normal = GetHitNormal( closestIntersection );
	const vec3 albedo = GetHitAlbedo( closestIntersection );
	const float roughness = GetHitRoughness( closestIntersection );

	// material identifier
	const int materialID = GetHitMaterial( closestIntersection );

	// precomputing the new vectors
	const vec3 diffuseVector = cosWeightedRandomHemisphereDirection( normal );
	const vec3 reflectedVector = reflect( direction, normal );

	// update origin point + epsilon bump (need to exclude epsilon bump for volumetrics, refractive hits)
	SetRayOrigin( myState, origin + dist * direction + ( 3.0f * epsilon * normal ) );

	switch ( materialID ) {
		case NONE: {
			// if you didn't any object in the scene, we're saying you hit the skybox
				// this is probably going to be setup very similar to the rectilinear map from Daedalus
			if ( GetHitDistance( closestIntersection ) == MAX_DIST ) {
				terminate = true;
				const float mixFactor = dot( normalize( vec3( -1.0f, -1.0f, -1.0f ) ), GetRayDirection( myState ) );
				// const vec3 color = oklch_to_srgb( vec3( smoothstep( 0.8f, 0.9f, mixFactor ), 0.5f, atan( direction.x, direction.y ) ) );
				vec3 color = vec3( smoothstep( 0.5f, 0.9f, mixFactor ) * 6.0f ); // + vec3( smoothstep( -0.7f, -1.0f, mixFactor ) ) * vec3( 3.9f, 0.4f, 0.1f );
//				 color += vec3( 0.5f );

				AddEnergy( myState, GetTransmission( myState ) * color );
			}
		}
		break;
		//=====================================================================
		case DIFFUSE: { // lambertian diffuse reflection
			SetRayDirection( myState, diffuseVector );
			SetTransmission( myState, GetTransmission( myState ) * albedo );
		}
		break;
		//=====================================================================
		case EMISSIVE: { // light emitting surface
			SetRayDirection( myState, diffuseVector );
			AddEnergy( myState, GetTransmission( myState ) * albedo );
		}
		break;
		//=====================================================================
		case MIRROR: { // mirror reflection
			SetRayDirection( myState, reflectedVector );
			SetTransmission( myState, GetTransmission( myState ) * albedo );
		}
		break;
		//=====================================================================
		case REFRACTIVE: { // glass-like interface
			SetTransmission( myState, GetTransmission( myState ) * albedo ); // representing colored glass
			float adjustedIOR = GetHitIoR( closestIntersection ); // initial value of IoR
			vec3 adjustedNormal = normal;

			SetRayOrigin( myState, GetRayOrigin( myState ) - 3.0f * epsilon * adjustedNormal );
			if ( GetHitFrontface( closestIntersection ) ) {

			} else { // backface has to handle things a little differently
				SetRayOrigin( myState, GetRayOrigin( myState ) + 3.0f * epsilon * adjustedNormal );
				adjustedNormal = -adjustedNormal;
				adjustedIOR = 1.0f / adjustedIOR;
			}
			float cosTheta = min( dot( -normalize( direction ), adjustedNormal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( adjustedIOR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, adjustedIOR ) > NormalizedRandomFloat() ) {
				SetRayDirection( myState, normalize( mix( reflect( normalize( direction ), adjustedNormal ), RandomUnitVector(), roughness ) ) );
			} else {
				SetRayDirection( myState, normalize( mix( refract( normalize( direction ), adjustedNormal, adjustedIOR ), RandomUnitVector(), roughness ) ) );
			}
		}
		break;
		//=====================================================================
		case VOLUME: {
			// // from Nikolay
			// vec3 other = vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() ) * 2.0f - 1.0f;
			// SetRayDirection( myState, normalize( other + anglePhong( 1800.0f, GetRayDirection( myState ) ) * 0.15f ) );

			SetRayOrigin( myState, GetRayOrigin( myState ) + GetHitDistance( closestIntersection ) * GetRayDirection( myState ) );
			// SetRayDirection( myState, normalize( 0.1f * GetRayDirection( myState ) + RandomUnitVector() ) );
			SetRayDirection( myState, normalize( 0.25f * GetRayDirection( myState ) + RandomUnitVector() ) );
			// SetRayDirection( myState, normalize( GetRayDirection( myState ) * RandomUnitVector() ) );
			// SetRayDirection( myState, normalize( anglePhong( 100.0f, GetRayDirection( myState ) ) ) );
			SetTransmission( myState, GetTransmission( myState ) * GetHitAlbedo( closestIntersection ) );
		}
		break;
		//=====================================================================
		default: {}
		break;
	}

	// russian roulette termination
	vec3 transmission = GetTransmission( myState );
	const float maxChannel = max( max( transmission.x, transmission.y ), transmission.z );
	if ( NormalizedRandomFloat() > maxChannel ) {
		terminate = true;
	}
	// RR compensation term
	transmission *= ( 1.0f / maxChannel );
	SetTransmission( myState, transmission );

	if ( terminate ) {

		// time to write to the framebuffer
		const vec3 color = GetEnergyTotal( myState );
		imageAtomicAdd( rTally, loc, uint( 1024 * color.x ) );
		imageAtomicAdd( gTally, loc, uint( 1024 * color.y ) );
		imageAtomicAdd( bTally, loc, uint( 1024 * color.z ) );
		imageAtomicAdd( count, loc, 1 );

		// and the ray is dead
		StateReset( state[ gl_GlobalInvocationID.x ] );

	} else {

		// store updated stuff back in the buffer
		state[ gl_GlobalInvocationID.x ] = myState;

	}
}