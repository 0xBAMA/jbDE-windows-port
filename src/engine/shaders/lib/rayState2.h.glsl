// rayState_t/intersection_t setup for Icarus (Rev. 2)
//	Second revision splits the structs into two separate pieces, for better parallel scalability (barrier removal + gather)
//=============================================================================================================================
// something to abstract the buffer stride stuff... tbd
#define INTERSECT_SDF		0
#define INTERSECT_TRIANGLE	1
#define INTERSECT_TINYBVH	2
#define INTERSECT_VOLUME	3
#define NUM_INTERSECTORS	4
//=============================================================================================================================
// material types
#define NONE				0
#define EMISSIVE			1
#define MIRROR				2
#define DIFFUSE				3
#define REFRACTIVE			4
#define VOLUME				5

//=============================================================================================================================
// Constants
//=============================================================================================================================
#define MAX_DIST		1e5f

//=============================================================================================================================
// Ray State Struct (64 bytes)
//   unit length ray direction indicates a living ray, otherwise it is not
//=============================================================================================================================
// represents the more persistent ray state, things which exist across bounce loop iterations
struct rayState_t {
	vec4 data1; // ray origin in .xyz, pixel x value in .w
	vec4 data2; // ray direction in .xyz, pixel y value in .w
	vec4 data3; // transmission in .xyz, unused .w
	vec4 data4; // energy total in .xyz, unused .w
};
//=============================================================================================================================
void SetPixelIndex		( inout rayState_t rayState, ivec2 index )		{ rayState.data1.w = index.x; rayState.data2.w = index.y; }
ivec2 GetPixelIndex		( in rayState_t rayState )						{ return ivec2( int( rayState.data1.w ), int( rayState.data2.w ) ); }

void SetRayOrigin		( inout rayState_t rayState, vec3 origin )		{ rayState.data1.xyz = origin; }
vec3 GetRayOrigin		( rayState_t rayState )							{ return rayState.data1.xyz; }

void SetRayDirection	( inout rayState_t rayState, vec3 direction )	{ rayState.data2.xyz = direction; }
vec3 GetRayDirection	( rayState_t rayState )							{ return rayState.data2.xyz; }

void SetTransmission	( inout rayState_t rayState, vec3 transmission ){ rayState.data3.xyz = transmission; }
vec3 GetTransmission	( rayState_t rayState )							{ return rayState.data3.xyz; }

void SetEnergyTotal 	( inout rayState_t rayState, vec3 energy )		{ rayState.data4.xyz = energy; }
vec3 GetEnergyTotal		( rayState_t rayState )							{ return rayState.data4.xyz; }
void AddEnergy			( inout rayState_t rayState, vec3 energy )		{ SetEnergyTotal( rayState, GetEnergyTotal( rayState ) + energy ); }
//=============================================================================================================================
bool IsLiving			( in rayState_t rayState )						{ return ( length( GetRayDirection( rayState ) ) > 0.5f ); }
//=============================================================================================================================
void StateReset ( inout rayState_t rayState ) {
	rayState.data1 = rayState.data2 = rayState.data3 = rayState.data4 = vec4( 0.0f );

	// what are sane defaults?
	SetEnergyTotal( rayState, vec3( 0.0f ) );
	SetTransmission( rayState, vec3( 1.0f ) );
}


//=============================================================================================================================
// Intersection Struct (64 bytes)
//=============================================================================================================================
// represents the output from any one of the intersection stages... gathered by the ray shading stage
struct intersection_t {
	vec4 data1; // normal vector in .xyz, distance to hit in .w
	vec4 data2; // albedo of hit in .xyz, roughness in .w
	vec4 data3; // .x type of material, IoR .y, frontface .z, .w unused
	vec4 data4; // unused
};
//=============================================================================================================================
void SetHitNormal		( inout intersection_t intersection, vec3 normal )		{ intersection.data1.xyz = normal; }
vec3 GetHitNormal		( intersection_t intersection )							{ return intersection.data1.xyz; }

void SetHitDistance		( inout intersection_t intersection, float dist )		{ intersection.data1.w = clamp( dist, 0.0f, MAX_DIST ); }
float GetHitDistance	( intersection_t intersection )							{ return intersection.data1.w; }

void SetHitAlbedo		( inout intersection_t intersection, vec3 albedo )		{ intersection.data2.xyz = albedo; }
vec3 GetHitAlbedo		( intersection_t intersection )							{ return intersection.data2.xyz; }

void SetHitRoughness	( inout intersection_t intersection, float roughness )	{ intersection.data2.w = roughness; }
float GetHitRoughness	( intersection_t intersection )							{ return intersection.data2.w; }

void SetHitMaterial		( inout intersection_t intersection, int material )		{ intersection.data3.x = float( material ); }
int GetHitMaterial		( intersection_t intersection )							{ return int( intersection.data3.x ); }

void SetHitIoR			( inout intersection_t intersection, float IoR )		{ intersection.data3.y = IoR; }
float GetHitIoR			( intersection_t intersection )							{ return intersection.data3.y; }

void SetHitFrontface	( inout intersection_t intersection, bool frontface )	{ intersection.data3.z = frontface ? 1.0f : 0.0f; }
bool GetHitFrontface	( intersection_t intersection )							{ return ( intersection.data3.z == 1.0f ); }
//=============================================================================================================================
void IntersectionReset ( inout intersection_t intersection ) {
	intersection.data1 = intersection.data2 = intersection.data3 = intersection.data4 = vec4( 0.0f );

	// what are sane defaults?
	SetHitDistance( intersection, MAX_DIST );
	SetHitMaterial( intersection, NONE );
}