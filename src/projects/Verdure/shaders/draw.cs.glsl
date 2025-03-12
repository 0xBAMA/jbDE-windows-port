#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
//=============================================================================================================================
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
//=============================================================================================================================
// gpu-side code for ray-BVH traversal
	// used for computing rD, reciprocal direction
float tinybvh_safercp( const float x ) { return x > 1e-12f ? ( 1.0f / x ) : ( x < -1e-12f ? ( 1.0f / x ) : 1e30f ); }
vec3 tinybvh_safercp( const vec3 x ) { return vec3( tinybvh_safercp( x.x ), tinybvh_safercp( x.y ), tinybvh_safercp( x.z ) ); }
//=============================================================================================================================
// node and triangle data specifically for the BVH
layout( binding = 0, std430 ) readonly buffer cwbvhNodesBuffer { vec4 cwbvhNodes[]; };
layout( binding = 1, std430 ) readonly buffer cwbvhTrisBuffer { vec4 cwbvhTris[]; };
// vertex data for the individual triangles' vertices, in a usable format ( .w can be used to pack more data )
layout( binding = 2, std430 ) readonly buffer triangleDataBuffer { vec4 triangleData[]; };

// second set, for the grass blades
layout( binding = 3, std430 ) readonly buffer cwbvhNodesBuffer2 { vec4 cwbvhNodes2[]; };
layout( binding = 4, std430 ) readonly buffer cwbvhTrisBuffer2 { vec4 cwbvhTris2[]; };
layout( binding = 5, std430 ) readonly buffer triangleDataBuffer2 { vec4 triangleData2[]; }; // todo
//=============================================================================================================================
#include "consistentPrimitives.glsl.h" // ray-sphere, ray-box inside traverse.h
#include "noise.h"
#include "pbrConstants.glsl"

#define NODEBUFFER cwbvhNodes
#define TRIBUFFER cwbvhTris
#define TRAVERSALFUNC traverse_cwbvh_terrain

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC

#define NODEBUFFER cwbvhNodes2
#define TRIBUFFER cwbvhTris2
#define TRAVERSALFUNC traverse_cwbvh_grass

vec3 grassColorLeaf = vec3( 0.0f );
bool leafTestFunc ( vec3 origin, vec3 direction, uint index, inout float tmax, inout vec2 uv ) {
	// test against N triangles for one blade of grass
		// initially, we will just be doing a single triangle

	const uint baseIndex = 4 * index;
	vec4 v0 = triangleData2[ baseIndex + 0 ];
	vec4 v1 = triangleData2[ baseIndex + 1 ];
	vec4 v2 = triangleData2[ baseIndex + 2 ];

// moller trombore on a dynamic triangle
	// precompute vectors representing edges
	const vec3 e1 = v1.xyz - v0.xyz; // edge1 = vertex1 - vertex0
	const vec3 e2 = v2.xyz - v0.xyz; // edge2 = vertex2 - vertex0

	const vec3 r = cross( direction.xyz, e1 );
	const float a = dot( e2, r );
	if ( abs( a ) < 0.0000001f )
		return false;
	const float f = 1.0f / a;
	const vec3 s = origin.xyz - v0.xyz;
	const float u = f * dot( s, r );
	if ( u < 0 || u > 1 )
		return false;
	const vec3 q = cross( s, e2 );
	const float v = f * dot( direction.xyz, q );
	if ( v < 0 || u + v > 1 )
		return false;
	const float d = f * dot( e1, q );
	if ( d <= 0.0f || d >= tmax )
		return false;
	uv = vec2( u, v ), tmax = d;
	grassColorLeaf = vec3( v0.w, v1.w, v2.w );
	return true;
}
#define CUSTOMLEAFTEST leafTestFunc

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC
#undef CUSTOMLEAFTEST

#include "random.h"
//=============================================================================================================================

uniform mat3 invBasis;
uniform float time;
uniform float scale;
uniform float blendAmount;
uniform ivec2 blueNoiseOffset;
uniform ivec2 uvOffset;
uniform float globeIoR;

// enable flags for the three lights
uniform bvec3 lightEnable;

// Key Light
uniform vec3 lightDirections0[ 16 ];
uniform vec4 lightColor0;

// Fill Light
uniform vec3 lightDirections1[ 16 ];
uniform vec4 lightColor1;

// Back Light
uniform vec3 lightDirections2[ 16 ];
uniform vec4 lightColor2;

// DoF parameters
uniform float DoFRadius;
uniform float DoFDistance;

//=============================================================================================================================
// bayer matrix for indexing into the queues
const int bayerMatrix[ 16 ] = int [] (
	 0,  8,  2, 10,
	12,  4, 14,  6,
	 3, 11,  1,  9,
	15,  7, 13,  5
);
//=============================================================================================================================

// vector axis/angle rotation, from https://suricrasia.online/blog/shader-functions/
vec3 erot( vec3 p, vec3 ax, float ro ) {
	return mix( dot( ax, p ) * ax, p, cos( ro ) ) + cross( ax, p ) * sin( ro );
}

// blue noise helper
vec4 blue() {
	return vec4( imageLoad( blueNoiseTexture, ivec2( blueNoiseOffset + ivec2( gl_GlobalInvocationID.xy ) ) % ivec2( imageSize( blueNoiseTexture ).xy ) ) ) / 255.0f;
}

// terrain trace helper
vec4 terrainTrace ( vec3 origin, vec3 direction ) {
	return traverse_cwbvh_terrain( origin, direction, tinybvh_safercp( direction ), 1e30f );
}

// grass trace helper
vec4 grassTrace ( vec3 origin, vec3 direction ) {
	return traverse_cwbvh_grass( origin, direction, tinybvh_safercp( direction ), 1e30f );
}

// sphere trace helper
vec4 sphereTrace ( vec3 origin, vec3 direction ) {
	vec3 normal;
	float d = iSphere( origin, direction, normal, 1.0f );
	// same as the grass/terrain, x is distance, yzw in this case is normal
	return vec4( d, normal );
}

//=============================================================================================================================

void main () {
	// solve for jittered pixel uv, aspect ratio adjust
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const vec2 is = vec2( imageSize( accumulatorTexture ).xy );
	vec2 uv = scale * ( ( ( vec2( writeLoc + uvOffset ) + ( blue().xy - vec2( 0.5f ) ) ) / is ) - vec2( 0.5f ) );
	uv.y *= -float( is.y ) / float( is.x );

	// seeding the RNG
	seed = writeLoc.x * 6969 + writeLoc.y * 420 + blueNoiseOffset.x * 1313 + blueNoiseOffset.y * 31415;

	// initialize color value
	vec3 color = vec3( 0.0f );

	// initial ray origin and direction
	vec3 rayOrigin = invBasis * vec3( uv, -2.0f );
	vec3 rayDirection = normalize( invBasis * vec3( 0.0f, 0.0f, 2.0f ) );

	// TODO: DoF calculation...
		// probably bring over more of the Voraldo13 camera https://github.com/0xBAMA/Voraldo13/blob/main/resources/engineCode/shaders/renderers/raymarch.cs.glsl#L70C28-L70C37

	if ( DoFRadius != 0.0f ) {
		// compute "perfect" ray
		vec2 uvNoJitter = scale * ( ( ( vec2( writeLoc + uvOffset ) + ( blue().zw - vec2( 0.5f ) ) ) / is ) - vec2( 0.5f ) );
		uvNoJitter.y *= -float( is.y ) / float( is.x );
	
		vec3 rayOriginNoJitter = invBasis * vec3( uvNoJitter, -2.0f );
		vec3 rayDirectionNoJitter = normalize( invBasis * vec3( 0.0f, 0.0f, 2.0f ) );

		// DoF focus distance out along the ray
		vec3 focusPoint = rayOriginNoJitter + rayDirectionNoJitter * DoFDistance;

		// this is redundant, oh well
		uv = scale * ( ( ( vec2( writeLoc + uvOffset ) + DoFRadius * ( UniformSampleHexagon( blue().xy ) ) ) / is ) - vec2( 0.5f ) );
		uv.y *= -float( is.y ) / float( is.x );
		rayOrigin = invBasis * vec3( uv, -2.0f );
		rayDirection = normalize( focusPoint - rayOrigin );
	}


	// initial ray-sphere test against snowglobe
	vec4 initialSphereTest = sphereTrace( rayOrigin, rayDirection );

	// if the sphere test hits
	if ( initialSphereTest.x != MAX_DIST_CP ) {

		// update ray origin
		rayOrigin += rayDirection * initialSphereTest.x;

		// shoot a ray straight upwards... why is this negative? need to visualize positions and fix up some stuff
		vec3 skirtCheckOrigin = rayOrigin + 0.00001f * rayDirection;
		vec3 skirtCheckDirection = vec3( 0.0f, 0.0f, -1.0f );
		vec4 skirtCheckTerrain = terrainTrace( skirtCheckOrigin, skirtCheckDirection );
		vec4 skirtCheckSphere = sphereTrace( skirtCheckOrigin, skirtCheckDirection );

		// if you hit the ground
		if ( skirtCheckTerrain.x < skirtCheckSphere.x ) {

			// pixel gets skirts color... probably parameterize this
			color = vec3( 0.01f );

		} else {

			// refract the ray based on the sphere hit normal
			rayDirection = refract( rayDirection, initialSphereTest.yzw, 1.0f / globeIoR );

			// get new intersections with the intersectors
			vec4 terrainPrimaryHit = terrainTrace( rayOrigin, rayDirection );	// terrain
			vec4 grassPrimaryHit = grassTrace( rayOrigin, rayDirection );		// grass
			vec4 spherePrimaryHit = sphereTrace( rayOrigin, rayDirection );		// sphere

			// solve for minimum of the three distances
			float dClosest = min( min( terrainPrimaryHit.x, grassPrimaryHit.x ), spherePrimaryHit.x );

			// compute the fog term based on this minimum distance - this is where the volumetrics would need to go
			// vec3 fogTerm = exp( 0.5f * dClosest ) * vec3( 0.01f, 0.05f, 0.0618f );
			// vec3 fogTerm = exp( -0.001f * dClosest ) * vec3( 0.01f, 0.05f, 0.0618f );
			vec3 fogTerm = vec3( 0.0f );

			// if the sphere is not the closest of the three, we hit some surface
			if ( dClosest != spherePrimaryHit.x ) {

				// pull the vertex data for the hit terrain/grass triangles
				uint vertexIdx = 3 * floatBitsToUint( terrainPrimaryHit.w );
				vec3 vertex0t = triangleData[ vertexIdx + 0 ].xyz;
				vec3 vertex1t = triangleData[ vertexIdx + 1 ].xyz;
				vec3 vertex2t = triangleData[ vertexIdx + 2 ].xyz;

				// the indexing for grass will change once grass blades have multiple tris
				vertexIdx = 4 * floatBitsToUint( grassPrimaryHit.w ); // stride of 4, caching the base point in the 4th coordinate
				vec3 vertex0g = triangleData2[ vertexIdx + 0 ].xyz;
				vec3 vertex1g = triangleData2[ vertexIdx + 1 ].xyz;
				vec3 vertex2g = triangleData2[ vertexIdx + 2 ].xyz;
				// vec3 grassColor = vec3( triangleData2[ vertexIdx + 0 ].w, triangleData2[ vertexIdx + 1 ].w, triangleData2[ vertexIdx + 2 ].w );
				vec3 grassColor = grassColorLeaf;

				// solve for normal, frontface
				vec3 normal = vec3( 0.0f );
				if ( terrainPrimaryHit.x < grassPrimaryHit.x ) {
					normal = normalize( cross( vertex1t - vertex0t, vertex2t - vertex0t ) ); // use terrain data
				} else {
					normal = normalize( cross( vertex1g - vertex0g, vertex2g - vertex0g ) ); // use grass data
				}

				// I need to make sure that this is correct
				bool frontFace = dot( normal, rayDirection ) < 0.0f;
				normal = frontFace ? normal : -normal;

				// TODO: depth, normal, position, are all now known, so we can write this to another target for SSAO
					// it may make sense to move some stuff to a deferred pass... need to validate normal, position, depth results first
					// RT deferred will be much more efficient than the equivalent raster operation, I think... single set of results written per pixel

				// based on the x and y pixel locations, index into the list of light directions
				const int idx = bayerMatrix[ ( writeLoc.x % 4 ) + ( writeLoc.y % 4 ) * 4 ];

				// test shadow rays in the light direction
				rayOrigin = rayOrigin + rayDirection * dClosest * 0.99999f;

				vec3 overallLightContribution = vec3( 0.0f );

				if ( lightEnable.x ) { // first light - "key light"
					vec4 terrainShadowHit = terrainTrace( rayOrigin, lightDirections0[ idx ] );	// terrain
					vec4 grassShadowHit = grassTrace( rayOrigin, lightDirections0[ idx ] );		// grass
					vec4 sphereShadowHit = sphereTrace( rayOrigin, lightDirections0[ idx ] );	// sphere

					// resolve whether we hit an occluder before leaving the sphere
					bool inShadow = ( terrainShadowHit.x < sphereShadowHit.x ) || ( grassShadowHit.x < sphereShadowHit.x );

					// resolve color contribution ( N dot L diffuse term * shadow term )
					overallLightContribution += lightColor0.rgb * lightColor0.a * ( ( inShadow ) ? 0.01f : 1.0f ) * clamp( dot( normal, lightDirections0[ idx ] ), 0.01f, 1.0f );
				}

				if ( lightEnable.y ) { // second light - "fill light"
					vec4 terrainShadowHit = terrainTrace( rayOrigin, lightDirections1[ idx ] );	// terrain
					vec4 grassShadowHit = grassTrace( rayOrigin, lightDirections1[ idx ] );		// grass
					vec4 sphereShadowHit = sphereTrace( rayOrigin, lightDirections1[ idx ] );	// sphere

					// resolve whether we hit an occluder before leaving the sphere
					bool inShadow = ( terrainShadowHit.x < sphereShadowHit.x ) || ( grassShadowHit.x < sphereShadowHit.x );

					// resolve color contribution ( N dot L diffuse term * shadow term )
					overallLightContribution += lightColor1.rgb * lightColor1.a * ( ( inShadow ) ? 0.01f : 1.0f ) * clamp( dot( normal, lightDirections1[ idx ] ), 0.01f, 1.0f );
				}

				if ( lightEnable.z ) { // third light - "back light"
					vec4 terrainShadowHit = terrainTrace( rayOrigin, lightDirections2[ idx ] );	// terrain
					vec4 grassShadowHit = grassTrace( rayOrigin, lightDirections2[ idx ] );		// grass
					vec4 sphereShadowHit = sphereTrace( rayOrigin, lightDirections2[ idx ] );	// sphere

					// resolve whether we hit an occluder before leaving the sphere
					bool inShadow = ( terrainShadowHit.x < sphereShadowHit.x ) || ( grassShadowHit.x < sphereShadowHit.x );

					// resolve color contribution ( N dot L diffuse term * shadow term )
					overallLightContribution += lightColor2.rgb * lightColor2.a * ( ( inShadow ) ? 0.01f : 1.0f ) * clamp( dot( normal, lightDirections2[ idx ] ), 0.01f, 1.0f );
				}

				/* grassColor = vec3( // testing perlin displacement, visualizing as color
					abs( perlinfbm( rayOrigin + vec3( 0.0f, 0.5f * time, 0.0f ), 2.5f, 3 ) ),
					abs( perlinfbm( rayOrigin + vec3( 0.5f * time, 0.0f, 0.0f ), 1.5f, 3 ) ),
					abs( perlinfbm( rayOrigin + vec3( 0.0f, 0.0f, 0.5f * time ), 3.5f, 3 ) ) ); */

				// base color is vertex colors - currently boring white ground if you don't hit the grass
				// vec3 baseColor = ( ( grassPrimaryHit.x < terrainPrimaryHit.x ) ? vec3( grassPrimaryHit.yz, 1.0f - grassPrimaryHit.y - grassPrimaryHit.z ) : vec3( 1.0f ) ); // visualizing UVs
				// vec3 baseColor = ( ( grassPrimaryHit.x < terrainPrimaryHit.x ) ? grassColor * ( 1.0f - grassPrimaryHit.z ) : vec3( 0.02f, 0.01f, 0.0f ) ); // fade to black at base
				vec3 baseColor = ( ( grassPrimaryHit.x < terrainPrimaryHit.x ) ? grassColor * ( 1.0f - grassPrimaryHit.z ) : tire ); // fade to black at base
				// vec3 baseColor = ( ( grassPrimaryHit.x < terrainPrimaryHit.x ) ? grassColor : vec3( 1.0f ) );

				// add fog contribution to the final color
				color = fogTerm + overallLightContribution * baseColor;

			} else {

				// just take the fog term
				color = fogTerm;

			}
		}
	}

	// load previous color and blend with the result, write back to accumulator
	vec4 previousColor = imageLoad( accumulatorTexture, writeLoc );
	imageStore( accumulatorTexture, writeLoc, mix( vec4( color, 1.0f ), previousColor, blendAmount ) );
}
