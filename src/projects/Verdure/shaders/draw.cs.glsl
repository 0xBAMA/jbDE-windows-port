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

#define NODEBUFFER cwbvhNodes
#define TRIBUFFER cwbvhTris
#define TRAVERSALFUNC traverse_cwbvh_terrain

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC

// /* what is the return type? */ leafTestFunc ( /* what is the parameterization? rO, rD, index of leaf node */ ) {
	// test against N triangles for one blade of grass
// }

#define NODEBUFFER cwbvhNodes2
#define TRIBUFFER cwbvhTris2
#define TRAVERSALFUNC traverse_cwbvh_grass
// #define CUSTOMLEAFTEST // leafTestFunc

#include "traverse.h" // all support code for CWBVH8 traversal

#undef NODEBUFFER
#undef TRIBUFFER
#undef TRAVERSALFUNC
#undef CUSTOMLEAFTEST

//=============================================================================================================================

uniform mat3 invBasis;
uniform float time;
uniform float scale;
uniform float blendAmount;
uniform ivec2 noiseOffset;
uniform ivec2 uvOffset;
uniform vec3 lightDirection;

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
	return vec4( imageLoad( blueNoiseTexture, ivec2( noiseOffset + ivec2( gl_GlobalInvocationID.xy ) ) % ivec2( imageSize( blueNoiseTexture ).xy ) ) ) / 255.0f;
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
	vec2 uv = scale * ( ( ( vec2( writeLoc + uvOffset ) + blue().xy - vec2( 0.5f ) ) / is ) - vec2( 0.5f ) );
	uv.y *= -float( imageSize( accumulatorTexture ).y ) / float( imageSize( accumulatorTexture ).x );

	// TODO: DoF calculation...
		// probably bring over more of the Voraldo13 camera https://github.com/0xBAMA/Voraldo13/blob/main/resources/engineCode/shaders/renderers/raymarch.cs.glsl#L70C28-L70C37

	// initialize color value
	vec3 color = vec3( 0.0f );

	// initial ray origin and direction
	vec3 rayOrigin = invBasis * vec3( uv, -2.0f );
	vec3 rayDirection = normalize( invBasis * vec3( 0.0f, 0.0f, 2.0f ) );

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

			// refract the ray based on the sphere hit normal... probably also parameterize IoR
			rayDirection = refract( rayDirection, initialSphereTest.yzw, 1.0f / 1.4f );

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
				vertexIdx = 3 * floatBitsToUint( grassPrimaryHit.w );
				vec3 vertex0g = triangleData2[ vertexIdx + 0 ].xyz;
				vec3 vertex1g = triangleData2[ vertexIdx + 1 ].xyz;
				vec3 vertex2g = triangleData2[ vertexIdx + 2 ].xyz;
				vec3 grassColor = vec3( triangleData2[ vertexIdx + 0 ].w, triangleData2[ vertexIdx + 1 ].w, triangleData2[ vertexIdx + 2 ].w );

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

				// test shadow rays in the light direction
				rayOrigin = rayOrigin + rayDirection * dClosest * 0.99999f;
				vec4 terrainShadowHit = terrainTrace( rayOrigin, lightDirection );	// terrain
				vec4 grassShadowHit = grassTrace( rayOrigin, lightDirection );		// grass
				vec4 sphereShadowHit = sphereTrace( rayOrigin, lightDirection );	// sphere

				// resolve whether we hit an occluder before leaving the sphere
				bool inShadow = ( terrainShadowHit.x < sphereShadowHit.x ) || ( grassShadowHit.x < sphereShadowHit.x );

				// resolve final color ( N dot L diffuse term * shadow term + fog term )
				vec3 baseColor = ( ( grassPrimaryHit.x < terrainPrimaryHit.x ) ? grassColor : vec3( 1.0f ) );
				float shadowTerm = ( ( inShadow ) ? 0.01f : 1.0f ) * clamp( dot( frontFace ? normal : -normal, lightDirection ), 0.01f, 1.0f );

				color = fogTerm + shadowTerm * baseColor;

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
