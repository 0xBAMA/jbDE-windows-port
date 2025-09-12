// rayState_t setup for Newton
//=============================================================================================================================
// material types
#define NOHIT						0
#define DIFFUSE						1
#define METALLIC					2
#define MIRROR						3
#define REFRACTIVE					4
// specific glass definitions here... tbd

//=============================================================================================================================
// total 64 bytes
struct rayState_t {
	vec4 data1; // ray origin in .xyz, distance to hit in .w
	vec4 data2; // ray direction in .xyz, type of material in .w
	vec4 data3; // backface in .x, transmission in .y, energy total in .z, albedo in .w
	vec4 data4; // hit normal in .xyz, ray wavelength in .w
};
//=============================================================================================================================
void SetRayOrigin 		( inout rayState_t rayState, vec3 origin )		{ rayState.data1.xyz = origin; }
vec3 GetRayOrigin		( rayState_t rayState )							{ return rayState.data1.xyz; }

void SetHitDistance		( inout rayState_t rayState, float dist )		{ rayState.data1.w = dist; }
float GetHitDistance	( rayState_t rayState )							{ return rayState.data1.w; }

void SetRayDirection	( inout rayState_t rayState, vec3 direction )	{ rayState.data2.xyz = direction; }
vec3 GetRayDirection	( rayState_t rayState )							{ return rayState.data2.xyz; }

void SetHitMaterial		( inout rayState_t rayState, int material )		{ rayState.data2.w = float( material ); }
int GetHitMaterial		( rayState_t rayState )							{ return int( rayState.data2.w ); }

// maybe encoding more information here, not sure yet
void SetHitFrontface 	( inout rayState_t rayState, bool frontface )	{ rayState.data3.x = frontface ? 1.0f : 0.0f; }
bool GetHitFrontface	( rayState_t rayState )							{ return ( rayState.data3.x == 1.0f ); }

void SetRayTransmission	( inout rayState_t rayState, float transmission ) { rayState.data3.y = transmission; }
float GetRayTransmission ( rayState_t rayState )						{ return rayState.data3.y; }

void SetEnergyTotal 	( inout rayState_t rayState, float energy )		{ rayState.data3.z = energy; }
float GetEnergyTotal		( rayState_t rayState )						{ return rayState.data3.z; }
void AddEnergy			( inout rayState_t rayState, float energy )		{ SetEnergyTotal( rayState, GetEnergyTotal( rayState ) + energy ); }

void SetHitAlbedo 		( inout rayState_t rayState, float albedo )		{ rayState.data3.w = albedo; }
float GetHitAlbedo		( rayState_t rayState )							{ return rayState.data3.w; }

void SetHitNormal 		( inout rayState_t rayState, vec3 normal )		{ rayState.data4.xyz = normal; }
vec3 GetHitNormal		( rayState_t rayState )							{ return rayState.data4.xyz; }

void SetRayWavelength	( inout rayState_t rayState, float wavelength )	{ rayState.data4.w = wavelength; }
float GetRayWavelength	( rayState_t rayState )							{ return rayState.data4.w; }

//=============================================================================================================================
void StateReset ( inout rayState_t rayState ) {
	// write zeroes
	rayState.data1 = rayState.data2 = rayState.data3 = rayState.data4 = vec4( 0.0f );

	// need saner defaults...
	SetTransmission( rayState, 1.0f );
	SetHitDistance( rayState, 1e6f );
}