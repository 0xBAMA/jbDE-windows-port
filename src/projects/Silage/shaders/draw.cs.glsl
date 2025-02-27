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
//=============================================================================================================================
#include "traverse.h" // all support code for CWBVH8 traversal
#include "consistentPrimitives.glsl.h" // ray-sphere
//=============================================================================================================================
uniform mat3 invBasis;
uniform float time;
uniform float scale;
//=============================================================================================================================
vec3 erot( vec3 p, vec3 ax, float ro ) {
	return mix( dot( ax, p ) * ax, p, cos( ro ) ) + cross( ax, p ) * sin( ro );
}
//=============================================================================================================================
void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec2 centeredUV = ( ( vec2( writeLoc ) + vec2( 0.5f ) ) / vec2( imageSize( accumulatorTexture ).xy ) ) - vec2( 0.5f );
	centeredUV.y *= ( float( imageSize( accumulatorTexture ).y ) / float( imageSize( accumulatorTexture ).x ) );

	// probably bring over more of the Voraldo13 camera https://github.com/0xBAMA/Voraldo13/blob/main/resources/engineCode/shaders/renderers/raymarch.cs.glsl#L70C28-L70C37
	Ray r;
	r.O.xyz = invBasis * vec3( scale * centeredUV, -2.0f );
	r.D.xyz = normalize( invBasis * vec3( 0.0f, 0.0f, 2.0 ) );
	r.rD.xyz = tinybvh_safercp( r.D.xyz );

	// do a ray-sphere test, refract and update origin
	vec3 normal = vec3( 0.0f ), normal2 = normal, normal3 = normal;
	float d = iSphere( r.O.xyz, r.D.xyz, normal, 1.0f );

	vec3 color = vec3( 0.0f );
	const vec3 lightDirection = erot( normalize( vec3( 1.0f, 1.0f, -1.0f  ) ), vec3( 0.0f, 0.0f, 1.0f ), time );

	if ( d != MAX_DIST_CP ) {
		// update the origin
		r.O.xyz += r.D.xyz * d;

		// refract the ray
		r.D.xyz = refract( r.D.xyz, normal, 1.0f / 1.2f );
		r.rD.xyz = tinybvh_safercp( r.D.xyz );

		// traverse the BVH
		r.hit = traverse_cwbvh( r.O.xyz, r.D.xyz, r.rD.xyz, 1e30f );

		// get a second hit with the sphere
		// float d2 = iSphere( r.O.xyz, r.D.xyz, normal, 1.0f );

		if ( r.hit.x < 1e30f ) {
			hit = true;
			// write the data to the image
			// imageStore( accumulatorTexture, writeLoc, vec4( vec3( exp( -0.01 * r.hit.x ), 0.0f, 0.0f ) + vec3( triangleData[ 3 * floatBitsToUint( r.hit.w ) ].w ), 1.0f ) );
			// imageStore( accumulatorTexture, writeLoc, vec4( vec3( pow( triangleData[ 3 * floatBitsToUint( r.hit.w ) ].w, 2.0f ) ), 1.0f ) );

			uint vertexIdx = 3 * floatBitsToUint( r.hit.w );
			vec3 vertex0 = triangleData[ vertexIdx + 0 ].xyz;
			vec3 vertex1 = triangleData[ vertexIdx + 1 ].xyz;
			vec3 vertex2 = triangleData[ vertexIdx + 2 ].xyz;
			// vec3 I = O4.xyz + t * D;
			vec3 N = normalize( cross( vertex1 - vertex0, vertex2 - vertex0 ) );

			float NdotL = dot( N, vec3( 1.0f, 1.0f, 0.0f ) );
			
			bool frontFace = dot( N, r.D.xyz ) < 0.0f;
			vec3 baseColor = frontFace ? vec3( 1.0f, 1.0f, 0.0f ) : vec3( 0.0f, 1.0f, 0.0f );
			imageStore( accumulatorTexture, writeLoc, vec4( NdotL * baseColor, 1.0f ) );
		}
	}

	// add blending with history
	vec4 previousColor = imageLoad( accumulatorTexture, writeLoc );
	imageStore( accumulatorTexture, writeLoc, mix( vec4( color, 1.0f ), previousColor, 0.7f ) );
}
