#version 430
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout( binding = 0, rgba8 ) uniform image3D colorBlockFront;
layout( binding = 1, rgba8 ) uniform image3D colorBlockBack;
layout( binding = 2, r8ui ) uniform uimage3D maskBlockFront;
layout( binding = 3, r8ui ) uniform uimage3D maskBlockBack;

uniform vec3 bottomVector;
uniform vec3 topVector;
uniform float innerRadius;
uniform float outerRadius;
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
	const vec3 location = vec3( gl_GlobalInvocationID.xyz );
	vec3 cylinderCenter = ( bottomVector + topVector ) / 2.0f;
	vec3 topNormal = bottomVector - topVector;
	topNormal = PlaneTest( topVector, topNormal, cylinderCenter ) ? topNormal : ( topNormal * -1.0f );
	vec3 bottomNormal = bottomVector - topVector;
	bottomNormal = PlaneTest( bottomVector, bottomNormal, cylinderCenter ) ? bottomNormal : ( bottomNormal * -1.0f );
	if( PlaneTest( bottomVector, bottomNormal, location ) && PlaneTest( topVector, topNormal, location ) ) {
		float distanceToLine = length( cross( topVector - bottomVector, bottomVector - location ) ) / length( topVector - bottomVector );
		if( distanceToLine < outerRadius && distanceToLine > innerRadius ) {
			//distance from point to line from http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html
			return true;
		}
	}
	return false;
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
