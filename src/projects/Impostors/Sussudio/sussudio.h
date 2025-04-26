#include "../../../engine/includes.h"

#define CAPSULE		0
#define CAPPEDCONE	1
#define ROUNDEDBOX	2
#define ELLIPSOID	3

// ==== Geometry Manager ======================================================================================================
// a very basic set of primitives are supported, Chorizo style rasterizing compute-fit bounding box -> raytrace in fragment shader
struct geometryManager_t {

	// ========================================================================================================================
	//	capsule is the surface "radius" distance away from the line segment between "pointA" and "pointB"
	//		parameters are:
	//			pointA, pointB as vec3's
	//			radius as float
	//		total of 7 floats ( 56 bytes ) + 4 floats ( 16 bytes ) for RGBA color
	void AddCapsule ( const vec3 pointA, const vec3 pointB, const float radius, const vec4 color ) {
		// add data for a capsule
		float parameters[ 16 ] = {
			CAPSULE, pointA.x, pointA.y, pointA.z,
			pointB.x, pointB.y, pointB.z, radius,
			0.0f, 0.0f, 0.0f, 0.0f,
			color.r, color.g, color.b, color.a
		};
		AddPrimitive( parameters );
	}

	// ========================================================================================================================
	//	"capped cone" is a superset of sphere + capsule, can easily use it for a lot of segment-type uses... bounding boxes for capsule is somewhat wasteful
	//		parameters are:
	//			point A, point B as vec3's
	//			radius A, radius B, as floats
	//		total of 8 floats ( 64 bytes ) + 4 floats ( 16 bytes ) for RGBA color
	void AddCappedCone ( const vec3 pointA, const vec3 pointB, const float radiusA, const float radiusB, const vec4 color ) {
		// add data for a capped cone
		float parameters[ 16 ] = {
			CAPPEDCONE, pointA.x, pointA.y, pointA.z,
			pointB.x, pointB.y, pointB.z, radiusA,
			radiusB, 0.0f, 0.0f, 0.0f,
			color.r, color.g, color.b, color.a
		};
		AddPrimitive( parameters );
	}

	// ========================================================================================================================
	//	"rounded box" is a box with a configurable bevel around the edges, from sharp to possibly basically spherical
	//		parameters are:
	//			center point as vec3
	//			scale factors as vec3
	//			euler angle rotation... Chorizo has this packed as a single float but I think that's unneccesary
	//			rounding factor for the edges as a float
	//		total of 10 floats ( 80 bytes ) + 4 floats ( 16 bytes ) for RGBA color
	void AddRoundedBox ( const vec3 center, const vec3 scale, const vec3 rotation, const float rounding, const vec4 color ) {
		// add data for a rounded box
		float parameters[ 16 ] = {
			ROUNDEDBOX, center.x, center.y, center.z,
			scale.x, scale.y, scale.z, rotation.x,
			rotation.y, rotation.z, rounding, 0.0f,
			color.r, color.g, color.b, color.a
		};
		AddPrimitive( parameters );
	}

	// ========================================================================================================================
	//	"ellipsoid" is a 3d ellipsoid shape - can be spherical but also easy to bias into a rounded needle type of primitive
	//		parameters are:
	//			center point as vec3
	//			radii as vec3
	//			rotation as vec3 ( euler angles )
	//		total of 9 floats ( 72 bytes ) + 4 floats ( 16 bytes ) for RGBA color
	void AddEllipsoid ( const vec3 center, const vec3 radii, const vec3 rotation, const vec4 color ) {
		// add data for an ellipsoid
		float parameters[ 16 ] = {
			ELLIPSOID, center.x, center.y, center.z,
			radii.x, radii.y, radii.z, rotation.x,
			rotation.y, rotation.z, 0.0f, 0.0f,
			color.r, color.g, color.b, color.a
		};
		AddPrimitive( parameters );
	}

	// ========================================================================================================================
	// can keep Chorizo's design of 16 floats ( 128 bytes ) per primitive because nothing needs more than that
	//	Not going to be supporting alpha here, I don't think... maybe dither-type... tbd
	void AddPrimitive ( const float parameters[ 16 ] ) {
		count++;
		parametersList.reserve( 16 * count );
		for ( int i = 0; i < 16; i++ ) {
			parametersList.push_back( parameters[ i ] );
		}
	}

	void Clear () {
		parametersList.resize( 0 );
		count = 0;
	}

	// containing sets of 16 floats describing each primitive
	std::vector< float > parametersList;
	int count;
};
