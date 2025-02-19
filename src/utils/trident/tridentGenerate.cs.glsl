#version 430
layout( local_size_x = 8, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D tridentStorage;

uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;

// what to draw at the center
uniform int mode;

#define MAXSTEPS 45
#define MAXDIST 2.5
#define EPSILON 0.001

// version without materials
float SdSmoothMin( float a, float b ) {
	// float k = 0.12f;
	float k = 0.1;
	float h = clamp( 0.5f + 0.5f * ( b - a ) / k, 0.0f, 1.0f );
	return mix( b, a, h ) - k * h * ( 1.0f - h );
}

// version with materials
float SdSmoothMin( float a, float b, vec3 mtl1, vec3 mtl0, inout vec3 mtl ) {
	// float k = 0.12f;
	float k = 0.1f;
	float h = clamp( 0.5 + 0.5 * ( b - a ) / k, 0.0f, 1.0f );
	float s = mix( b, a, h ) - k * h * ( 1.0f - h );
	float sA = s - a;
	float sB = s - b;
	float r = sA / ( sA + sB );
	mtl = mix( mtl1, mtl0, r );
	return s;
}

float deRoundedCone ( vec3 p, vec3 a, vec3 b ) {
	float r1 = 0.1f; // radius at b
	float r2 = 0.02f; // radius at a

	vec3  ba = b - a;
	float l2 = dot( ba, ba );
	float rr = r1 - r2;
	float a2 = l2 - rr * rr;
	float il2 = 1.0f / l2;

	vec3 pa = p - a;
	float y = dot( pa, ba );
	float z = y - l2;
	vec3 d2 =  pa * l2 - ba * y;
	float x2 = dot( d2, d2 );
	float y2 = y * y * l2;
	float z2 = z * z * l2;

	float k = sign( rr ) * rr * rr * x2;
	if( sign( z ) * a2 * z2 > k )
		return sqrt( x2 + z2 ) * il2 - r2;
	if( sign( y ) * a2 * y2 < k )
		return sqrt( x2 + y2 ) * il2 - r1;
	return ( sqrt( x2 * a2 * il2 ) + y * rr ) * il2 - r1;
}

float deF ( vec3 p ){
	// float scalar = 6.0;
	float scalar = 4.0f;
	mat3 transform = inverse( mat3( basisX, basisY, basisZ ) );
	p = transform * p;
	p *= scalar;
	float a = 1.0f;
	for ( int i = 0; i < 5; i++ ) { // adjust iteration count here
		p = abs( p ) - 1.0f / 3.0f;
		if ( p.y > p.x ) p.xy = p.yx;
		if ( p.z > p.y ) p.yz = p.zy;
		p.z = abs( p.z );
		p *= 3.0f;
		p -= 1.0f;
		a *= 3.0f;
	}
	return ( length( max( abs( p ) - 1.0f, 0.0f ) ) / a ) / scalar;
 }

// core distance
float cDist ( vec3 p ) {
	switch ( mode ) {
		case 0:	// menger core
			return deF( p );
		case 1: // sphere core
			return distance( vec3( 0.0f ), p ) - 0.18f;
		// etc...
	}
}

// with materials
vec4 deMat ( vec3 p ) {
	// return value has .rgb color and .a is distance
	vec4 result = vec4( 1000.0f );
	float x = deRoundedCone( p, vec3( 0.0f ), basisX / 2.0f );
	vec3 xc = vec3( 1.0f, 0.0f, 0.0f );
	float y = deRoundedCone( p, vec3( 0.0f ), basisY / 2.0f );
	vec3 yc = vec3( 0.0f, 1.0f, 0.0f );
	float z = deRoundedCone( p, vec3( 0.0f ), basisZ / 2.0f );
	vec3 zc = vec3( 0.0f, 0.0f, 1.0f );
	float c = cDist( p );
	// vec3 cc = vec3( 0.16 );
	// vec3 cc = vec3( 0.32 );
	vec3 cc = vec3( 0.64f );
	result.a = SdSmoothMin( 1000.0f, x, vec3( 0.0f ), xc, result.rgb );
	result.a = SdSmoothMin( result.a, y, result.rgb, yc, result.rgb );
	result.a = SdSmoothMin( result.a, z, result.rgb, zc, result.rgb );
	result.a = SdSmoothMin( result.a, c, result.rgb, cc, result.rgb );
	return result;
}

// without materials
float de ( vec3 p ) {
	float x = deRoundedCone( p, vec3( 0.0f ), basisX / 2.0f );
	float y = deRoundedCone( p, vec3( 0.0f ), basisY / 2.0f );
	float z = deRoundedCone( p, vec3( 0.0f ), basisZ / 2.0f );
	float c = cDist( p );
	return SdSmoothMin( SdSmoothMin( SdSmoothMin( x, y ), z ), c );
}

vec3 normal ( vec3 p ) { // to get the normal vector for a point in space, this function evaluates the gradient of the distance function
#define METHOD 2
#if METHOD == 0
	// tetrahedron version, unknown source - 4 evaluations
	vec2 e = vec2( 1.0f, -1.0f ) * EPSILON;
	return normalize( e.xyy * de( p + e.xyy ) + e.yyx * de( p + e.yyx ) + e.yxy * de( p + e.yxy ) + e.xxx * de( p + e.xxx ) );
#elif METHOD == 1
	// by iq = more efficient, 4 evaluations
	vec2 e = vec2( EPSILON, 0.0f ); // computes the gradient of the estimator function
	return normalize( vec3( de( p ) ) - vec3( de( p - e.xyy ), de( p - e.yxy ), de( p - e.yyx ) ) );
#elif METHOD == 2
// by iq - less efficient, 6 evaluations
	vec3 eps = vec3( EPSILON, 0.0f, 0.0f );
	return normalize( vec3(
		de( p + eps.xyy ) - de( p - eps.xyy ),
		de( p + eps.yxy ) - de( p - eps.yxy ),
		de( p + eps.yyx ) - de( p - eps.yyx ) ) );
#endif
}

float calcAO ( in vec3 pos, in vec3 nor ) {
	float occ = 0.0f;
	float sca = 1.0f;
	for( int i = 0; i < 5; i++ ) {
		float h = 0.001f + 0.15f * float( i ) / 4.0f;
		float d = de( pos + h * nor );
		occ += ( h - d ) * sca;
		sca *= 0.95f;
	}
	return clamp( 1.0f - 1.5f * occ, 0.0f, 1.0f );
}

void main () {
	vec4 pixCol = vec4( 0.0f );
	int aa = 3;
	int samplesHit = 0;
	for ( int x = 0; x < aa; x++ ) {
		for ( int y = 0; y < aa; y++ ) {
			vec2 loc = vec2( ivec2( gl_GlobalInvocationID.xy ) ) + ( ( 1.0f / aa ) * vec2( x, y ) );
			vec2 position = loc / vec2( imageSize( tridentStorage ).xy );

			// ray starting location comes from mapped location on the texture
			position = ( 2.0f * position ) - vec2( 1.0f );
			position.x *= ( float( imageSize( tridentStorage ).x ) / float( imageSize( tridentStorage ).y ) );

			vec3 iterationColor = vec3( 0.0f );
			vec3 rayOrigin = vec3( position * 0.55f, -1.0f );
			vec3 rayDirection = vec3( 0.0f, 0.0f, 1.0f );

			// raymarch
			float t = 0.1f;
			for ( int i = 0; i < MAXSTEPS; i++ ) {
				vec3 p = rayOrigin + t * rayDirection;
				float dist = de( p );
				if ( dist < EPSILON || t > MAXDIST ) {
					break;
				}
				t += dist;
			}

			vec3 hitPoint = rayOrigin + t * rayDirection;
			vec3 surfaceNormal = normal( hitPoint );
			vec4 wCol = deMat( hitPoint ); // albedo + distance

			if ( wCol.a <= EPSILON ) {
				// go go gadget phong
				vec3 light1Position = vec3( -1.4f, -0.8f, -0.15f );
				vec3 light2Position = vec3( 1.8f, -0.3f,  0.25f );
				vec3 eyePosition = vec3( 0.0f, 0.0f, -1.0f );
				vec3 l1 = normalize( hitPoint - light1Position );
				vec3 l2 = normalize( hitPoint - light2Position );
				vec3 v = normalize( hitPoint - eyePosition );
				vec3 n = normalize( surfaceNormal );
				vec3 r1 = normalize( reflect( l1, n ) );
				vec3 r2 = normalize( reflect( l2, n ) );
				iterationColor = wCol.xyz;

				float ambient = 0.0f;
				iterationColor += ambient * vec3( 0.1f, 0.1f, 0.2f );
				
				float diffuse1 = ( 1.0f / ( pow( 0.25f * distance( hitPoint, light1Position ), 2.0f ) ) ) * 0.18f * max( dot( n,  l1 ), 0.0f );
				float diffuse2 = ( 1.0f / ( pow( 0.25f * distance( hitPoint, light2Position ), 2.0f ) ) ) * 0.18f * max( dot( n,  l2 ), 0.0f );

				iterationColor += diffuse1 * vec3( 0.09f, 0.09f, 0.04f );
				iterationColor += diffuse2 * vec3( 0.09f, 0.09f, 0.04f );

				float specular1 = ( 1.0f / ( pow( 0.25f * distance( hitPoint, light1Position ), 2.0f ) ) ) * 0.4f * pow( max( dot( r1, v ), 0.0f ), 60.0f );
				float specular2 = ( 1.0f / ( pow( 0.25f * distance( hitPoint, light2Position ), 2.0f ) ) ) * 0.4f * pow( max( dot( r2, v ), 0.0f ), 80.0f );

				if ( dot( n, l1 ) > 0.0f ) iterationColor += specular1 * vec3( 0.4f, 0.2f, 0.0f );
				if ( dot( n, l2 ) > 0.0f ) iterationColor += specular2 * vec3( 0.4f, 0.2f, 0.0f );

				iterationColor *= calcAO( hitPoint, surfaceNormal );
				pixCol.xyz += iterationColor;
				pixCol.a += 1.0f;
				samplesHit++;
			}
		}
	}

	if ( samplesHit > 0 ) {
		pixCol /= float( samplesHit );
	}

	uvec4 colorResult = uvec4( uvec3( pixCol * 255 ), ( float( samplesHit ) / float( aa * aa ) ) * 255 );
	imageStore( tridentStorage, ivec2( gl_GlobalInvocationID.xy ), colorResult );
}
