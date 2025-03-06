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
struct Ray {
	// data is defined here as 16-byte values to encourage the compilers
	// to fetch 16 bytes at a time: 12 (so, 8 + 4) will be slower.
	vec4 O, D, rD; // 48 byte - rD is reciprocal direction
	vec4 hit; // 16 byte
};
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
uniform ivec2 noiseOffset;
uniform ivec2 uvOffset;

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
	return traverse_cwbvh_terrain( origin, direction, tinybvh_safercp( direction ), 1e30f );
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
	// solve for jittered pixel uv

	// initialize color value

	// initial ray-sphere test against snowglobe

	// if the sphere test hits

		// update ray origin

		// shoot a ray straight upwards

		// if you hit the ground

			// pixel gets skirts color

		// else

			// refract the ray based on the sphere hit normal

			// update ray origin and direction

			// get new intersections with the intersectors
				// terrain
				// grass
				// sphere

			// solve for minimum of the three distances

			// compute the fog term based on this minimum distance

			// if the sphere is not the closest of the three

				// TODO: depth term, normal, position, is now known, so we can write this to another target for SSAO

				// pull the vertex data for the hit triangle

				// solve for normal, frontface

				// shadow rays in the light direction
					// terrain
					// grass
					// sphere

				// resolve final color ( N dot L term, shadow term, , etc )

			// else

				// take fog term

	// load previous color and blend with the result, write back to accumulator


	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec2 centeredUV = ( ( vec2( writeLoc + uvOffset ) + blue().xy ) / vec2( imageSize( accumulatorTexture ).xy ) ) - vec2( 0.5f );
	centeredUV.y *= ( float( imageSize( accumulatorTexture ).y ) / float( imageSize( accumulatorTexture ).x ) );

	// probably bring over more of the Voraldo13 camera https://github.com/0xBAMA/Voraldo13/blob/main/resources/engineCode/shaders/renderers/raymarch.cs.glsl#L70C28-L70C37
	Ray r;
	r.O.xyz = invBasis * vec3( scale * centeredUV, -2.0f );
	r.D.xyz = normalize( invBasis * vec3( 0.0f, 0.0f, 2.0f ) );
	r.rD.xyz = tinybvh_safercp( r.D.xyz );

	// do a ray-sphere test, refract and update origin
	vec3 normal = vec3( 0.0f ), normal2 = normal, normal3 = normal;
	float d = iSphere( r.O.xyz, r.D.xyz, normal, 1.0f );

	vec3 color = vec3( 0.0f );
	const vec3 lightDirection = erot( normalize( vec3( 1.0f, 1.0f, -1.0f ) ), vec3( 0.0f, 0.0f, 1.0f ), time );

	if ( d != MAX_DIST_CP ) {
		// update the origin
		r.O.xyz += r.D.xyz * d;

		// skirts - shoot a ray directly upwards, and if it hits, we should take the sphere hit and quit
		Ray skirtCheckRay;
		skirtCheckRay.O.xyz = r.O.xyz + 0.00001f * r.D.xyz;
		skirtCheckRay.D.xyz = vec3( 0.0f, 0.0f, -1.0f );
		skirtCheckRay.rD.xyz = tinybvh_safercp( skirtCheckRay.D.xyz );
		skirtCheckRay.hit = traverse_cwbvh_terrain( skirtCheckRay.O.xyz, skirtCheckRay.D.xyz, skirtCheckRay.rD.xyz, 1e30f );
		
		if ( skirtCheckRay.hit.x < iSphere( skirtCheckRay.O.xyz, skirtCheckRay.D.xyz, normal2, 1.0f ) ) {
			// this is the area "below" the ground
			color = vec3( 0.01f );
		} else {

			// refract the ray
			r.D.xyz = refract( r.D.xyz, normal, 1.0f / 1.4f );
			r.rD.xyz = tinybvh_safercp( r.D.xyz );

			// traverse the terrain BVH
			vec4 terrainHit = traverse_cwbvh_terrain( r.O.xyz, r.D.xyz, r.rD.xyz, 1e30f );

			// traverse the grass BVH
			vec4 grassHit = traverse_cwbvh_grass( r.O.xyz, r.D.xyz, r.rD.xyz, 1e30f );

			// get a second hit with the sphere
			float dSphereBackface = iSphere( r.O.xyz, r.D.xyz, normal, 1.0f );

			// which is closer?
			float dCloser = min( dSphereBackface, min( terrainHit.x, grassHit.x ) );
			vec3 fogTerm = exp( 0.5f * dCloser ) * vec3( 0.01f, 0.05f, 0.0618f );

			// if we hit the terrain or grass before the sphere
			if ( ( terrainHit.x < 1e30f || grassHit.x < 1e30f ) && ( terrainHit.x == dCloser || grassHit.x == dCloser ) ) {

				uint vertexIdx = 3 * floatBitsToUint( terrainHit.w );
				vec3 vertex0 = triangleData[ vertexIdx + 0 ].xyz;
				vec3 vertex1 = triangleData[ vertexIdx + 1 ].xyz;
				vec3 vertex2 = triangleData[ vertexIdx + 2 ].xyz;

				// determining shadow contribution
				Ray shadowRay;
				shadowRay.O.xyz = r.O.xyz + r.D.xyz * dCloser * 0.99999f;
				shadowRay.D.xyz = lightDirection;
				shadowRay.rD.xyz = tinybvh_safercp( shadowRay.D.xyz ); // last argument for traverse_cwbvh is a max distance, maybe useful for simplifying this

				vertexIdx = 3 * floatBitsToUint( grassHit.w );
				vec3 vertex0g = triangleData2[ vertexIdx + 0 ].xyz;
				vec3 vertex1g = triangleData2[ vertexIdx + 1 ].xyz;
				vec3 vertex2g = triangleData2[ vertexIdx + 2 ].xyz;

				float sphereD = iSphere( shadowRay.O.xyz, shadowRay.D.xyz, normal3, 1.0f );
				bool inShadow = ( ( traverse_cwbvh_terrain( shadowRay.O.xyz, shadowRay.D.xyz, shadowRay.rD.xyz, 1e30f ).x < sphereD )
					|| ( traverse_cwbvh_grass( shadowRay.O.xyz, shadowRay.D.xyz, shadowRay.rD.xyz, 1e30f ).x < sphereD ) );

				bool grassCloser = ( grassHit.x < terrainHit.x );
				vec3 baseColor = ( grassCloser ? vec3( 0.7f, 0.3f, 0.0f ) : vec3( 0.0f, 1.0f, 0.0f ) );

				// solving for the normal vector
				vec3 N = grassCloser ? normalize( cross( vertex1g - vertex0g, vertex2g - vertex0g ) ) : normalize( cross( vertex1 - vertex0, vertex2 - vertex0 ) );
				bool frontFace = dot( N, r.D.xyz ) < 0.0f;

				float shadowTerm = ( ( inShadow ) ? 0.01f : clamp( dot( ( frontFace ? N : -N ), lightDirection ), 0.01f, 1.0f ) );

				color = fogTerm + shadowTerm * baseColor;

			} else {
				// we just want to use the fog color
				color = fogTerm;
			}
		}
	}

	// add blending with history
	vec4 previousColor = imageLoad( accumulatorTexture, writeLoc );
	imageStore( accumulatorTexture, writeLoc, mix( vec4( color, 1.0f ), previousColor, 0.7f ) );
}
