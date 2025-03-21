#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;

uniform vec3 point1;
uniform vec3 point2;
uniform vec3 point3;
uniform float thickness;
uniform vec4 color;
uniform bool draw;
uniform int mask;

bool PlaneTest ( vec3 planePoint, vec3 planeNormal, vec3 testPoint ) {
	// Determines whether a point is above or below a plane
	//		false if the point is above the plane
	//		true if the point is below the plane

	float a = planeNormal.x;
	float b = planeNormal.y;
	float c = planeNormal.z;

	float x1 = planePoint.x;
	float y1 = planePoint.y;
	float z1 = planePoint.z;

	float x = testPoint.x;
	float y = testPoint.y;
	float z = testPoint.z;

	// equation of plane
	//  a (x-x1) + b (y-y1) + c (z-z1) = 0
	return ( ( a * ( x - x1 ) + b * ( y - y1 ) + c * ( z - z1 ) ) < 0 ) ? true : false;
}

bool inShape () {
	bool drawTriangle = false;
	vec3 calculatedTriangleCenter = ( point1 + point2 + point3 ) / 3.0f;
	//calculate the top normal vector of the triangle
	//     ^  < - - -normal
	//     |
	//     |
	//   1 .______. 2
	//      \							taking the cross product of the two sides ( 1-2 and 1-3 )
	//       \						will give either the normal or the inverse of the normal
	//        \						check this against the center point of the triangle to determine
	//         * 3					and invert it if neccesary ( depending on the relative positions
	//								of these three points / effectively the winding order )

	vec3 calculatedTopNormal = normalize( cross( point1 - point2, point1 - point3 ) );
	calculatedTopNormal = PlaneTest( point1 + thickness * calculatedTopNormal, calculatedTopNormal, calculatedTriangleCenter ) ? calculatedTopNormal : ( calculatedTopNormal * -1.0f );

	//calculate the side normal vectors
	//         ^
	//         |  < - - top normal
	//       _________
	//      |\       /| ^
	//      | \ top / | | thickness
	//      |  \   /  | v
	//      \   \ /  /
	//       \   |  /
	//        \  | /
	//         \ |/
	//          \/
	//
	//	looking at this from one of the edges:
	//
	//   ^
	//   | < - - - - the triangle's top normal
	//   *-------> < - - - vector representing the side being considered
	//
	//   take the cross product of these two vectors, then do a similar test involving the center point of the triangle to invert it if neccesary

	vec3 side12Normal = normalize( cross( calculatedTopNormal, point2 - point1 ) );
	side12Normal = PlaneTest( point1, side12Normal, calculatedTriangleCenter) ? side12Normal : ( side12Normal * -1.0 );

	vec3 side23Normal = normalize( cross( calculatedTopNormal, point3 - point2 ) );
	side23Normal = PlaneTest( point2, side23Normal, calculatedTriangleCenter) ? side23Normal : ( side23Normal * -1.0 );

	vec3 side31Normal = normalize( cross( calculatedTopNormal, point1 - point3 ) );
	side31Normal = PlaneTest( point3, side31Normal, calculatedTriangleCenter) ? side31Normal : ( side31Normal * -1.0 );


	// do the tests - for each of the normals, top, bottom, and the three sides,
	//	use the PlaneTest function to determine whether the current point is
	//	'below' all 5 planes - if it is, it is inside this triangular prism
	//	- this method actually may generalize to any convex solid

	drawTriangle = PlaneTest( point1 + ( thickness / 2.0f ) * calculatedTopNormal, calculatedTopNormal, gl_GlobalInvocationID.xyz ) &&
	PlaneTest( point1 - ( thickness / 2.0f ) * calculatedTopNormal, -1.0f * calculatedTopNormal, gl_GlobalInvocationID.xyz ) &&
	PlaneTest( point1, side12Normal, gl_GlobalInvocationID.xyz ) &&
	PlaneTest( point2, side23Normal, gl_GlobalInvocationID.xyz ) &&
	PlaneTest( point3, side31Normal, gl_GlobalInvocationID.xyz );

	return drawTriangle;
}

void main () {
	const ivec3 blockLocation = ivec3( gl_GlobalInvocationID.xyz );
	const uint previousMask = imageLoad( maskBlockBack, blockLocation ).x;
	const vec4 previousColor = imageLoad( colorBlockBack, blockLocation );
	if ( inShape() ) {
		vec4 blendedColor = mix( color, previousColor, float( previousMask ) / 255.0 );
		imageStore( colorBlockFront, blockLocation, draw ? blendedColor : previousColor );
		imageStore( maskBlockFront, blockLocation, uvec4( max( uint( mask ), previousMask ) ) );
	} else {
		imageStore( colorBlockFront, blockLocation, previousColor );
		imageStore( maskBlockFront, blockLocation, uvec4( previousMask ) );
	}
}
