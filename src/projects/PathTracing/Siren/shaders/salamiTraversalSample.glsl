// Scene

// Fork of "Gelami Raymarching Template" by gelami. https://shadertoy.com/view/mslGRs
// 2023-05-14 11:59:30

#define BLOOM
#define BLOOM_MAX_LOD 6
#define BLOOM_THRESHOLD 0.6
#define BLOOM_STRENGTH 0.75
#define CAMERA_SPEED 6.5
#define EMI_SCALE 5.0
#define FXAA
#define MOTION_BLUR
#define MOTION_BLUR_STRENGTH 4.0
#define SPHERES
#define STEPS 256
#define MAX_DIST 100.
#define EPS 1e-4
#define PI (acos(-1.))
#define TAU (PI*2.)
vec4 SampleLod ( sampler2D tex, vec2 uv, vec2 res, const int lod ) {
	vec2 hres = floor( res / 2.0f );
	vec2 nres = hres;
	float xpos = 0.0f;
	int i = 0;
	for ( ; i < lod; i++ ) {
		xpos += nres.x;
		nres = floor( nres / 2.0 );
	}
	vec2 nuv = uv * vec2( nres );
	nuv = clamp( nuv, vec2( 0.5f ), vec2( nres ) - 0.5f );
	nuv += vec2( xpos, 0.0f );
	return texture( tex, nuv / res );
}

float pack ( vec2 v ) { return uintBitsToFloat( packHalf2x16( v ) ); }
vec2 unpack ( float v ) { return unpackHalf2x16( floatBitsToUint( v ) ); }

// Ray-sphere intersesction
// https://www.iquilezles.org/www/articles/intersectors/intersectors.htm
vec2 sphereIntersection ( vec3 ro, vec3 rd, float ra ) {
	vec3 oc = ro;
	float b = dot( oc, rd );
	float c = dot( oc, oc ) - ra * ra;
	if ( b > 0.0f && c > 0.0f )
		return vec2( MAX_DIST );
	float h = b * b - c;
	if( h < 0.0f ) return vec2( MAX_DIST ); // no intersection
	h = sqrt( h );
	return vec2( -b - h, -b + h );
}

// Ray-rhombic dodecahedron intersection
vec2 rhombicDodecahedronIntersection ( vec3 ro, vec3 rd, float s, out vec3 normal ) {
	const vec3 rdn = vec3( 0.5f, -0.5f, sqrt( 2.0f ) / 2.0f );
	float tN, tF;
	{
		vec2 ird = 1.0f / rd.xz;
		vec2 n = ro.xz * ird;
		vec2 k = abs( ird ) * s;
		vec2 t0 = -n - k;
		vec2 t1 = -n + k;
		tN = max( t0.x, t0.y );
		tF = min( t1.x, t1.y );
		normal = vec3( -sign( ird ) * step( t0.yx, t0 ), 0.0f ).xzy;
	}
	{
		vec4 ird = 1.0f / vec4( dot( rd, rdn.xzx ), dot( rd, rdn.yzx ), dot( rd, rdn.xzy ), dot( rd, rdn.yzy ) );
		vec4 n = ird * vec4( dot( ro, rdn.xzx ), dot( ro, rdn.yzx ), dot( ro, rdn.xzy ), dot( ro, rdn.yzy ) );
		vec4 k = abs( ird ) * s;
		vec4 t0 = -n - k;
		vec4 t1 = -n + k;
		float t = max( max( t0.x, t0.y ), max( t0.z, t0.w ) );
		if ( t > tN ) {
			if ( t0.x == t )
				normal = rdn.xzx * -sign( ird.x );
			else if ( t0.y == t )
				normal = rdn.yzx * -sign( ird.y );
			else if ( t0.z == t )
				normal = rdn.xzy * -sign( ird.z );
			else
				normal = rdn.yzy * -sign( ird.w );
		}
		tN = max( tN, t );
		tF = min( tF, min( min( t1.x, t1.y ), min( t1.z, t1.w ) ) );
	}

	if ( tN > tF ) {
		tN = MAX_DIST;
		tF = MAX_DIST;
	}

	return vec2( tN, tF );
}

// Same as above but returns the far normal
vec2 rhombicDodecahedronIntersection2 ( vec3 ro, vec3 rd, float s, out vec3 normal ) {
	const vec3 rdn = vec3( 0.5f, -0.5f, sqrt( 2.0f ) / 2.0f );
	float tN, tF;
	{
		vec2 ird = 1.0 / rd.xz;
		vec2 n = ro.xz * ird;
		vec2 k = abs(ird) * s;
		vec2 t0 = -n - k;
		vec2 t1 = -n + k;
		tN = max(t0.x, t0.y);
		tF = min(t1.x, t1.y);
		normal = vec3(-sign(ird) * step(t1, t1.yx), 0).xzy;
	}
	{
		vec4 ird = 1.0f / vec4( dot( rd, rdn.xzx ), dot( rd, rdn.yzx ), dot( rd, rdn.xzy ), dot( rd, rdn.yzy ) );
		vec4 n = ird * vec4( dot( ro, rdn.xzx ), dot( ro, rdn.yzx ), dot( ro, rdn.xzy ), dot( ro, rdn.yzy ) );
		vec4 k = abs( ird ) * s;
		vec4 t0 = -n - k;
		vec4 t1 = -n + k;
		float t = max( max( t0.x, t0.y ), max( t0.z, t0.w ) );
		float t2 = min( tF, min( min( t1.x, t1.y ), min( t1.z, t1.w ) ) );
		if ( t2 < tF ) {
			if ( t1.x == t2 )
				normal = rdn.xzx * -sign( ird.x );
			else if ( t1.y == t2 )
				normal = rdn.yzx * -sign( ird.y );
			else if ( t1.z == t2 )
				normal = rdn.xzy * -sign( ird.z );
			else
				normal = rdn.yzy * -sign( ird.w );
		}
		tN = max(tN, t);
		tF = min(tF, t2);
	}

	if ( tN > tF ) {
		tN = MAX_DIST;
		tF = MAX_DIST;
	}
	return vec2( tN, tF );
}

mat3 getCameraMatrix ( vec3 ro, vec3 lo ) {
	vec3 cw = normalize( lo - ro );
	vec3 cu = normalize( cross( cw, vec3( 0.0f, 1.0f, 0.0f ) ) );
	vec3 cv = cross( cu, cw );
	return mat3( cu, cv, cw );
}

float safeacos ( float x ) { return acos( clamp( x, -1.0f, 1.0f ) ); }
float saturate ( float x ) { return clamp( x, 0.0f, 1.0f ); }
vec2 saturate ( vec2 x ) { return clamp( x, vec2( 0.0f ), vec2( 1.0f ) ); }
vec3 saturate ( vec3 x ) { return clamp( x, vec3( 0.0f ), vec3( 1.0f ) ); }
float sqr ( float x ) { return x * x; }
vec2 sqr ( vec2 x ) { return x * x; }
vec3 sqr ( vec3 x ) { return x * x; }
float luminance ( vec3 col ) { return dot( col, vec3( 0.2126729f, 0.7151522f, 0.0721750f ) ); }
mat2 rot2D( float a ) {
	float c = cos( a );
	float s = sin( a );
	return mat2( c, s, -s, c );
}

// https://iquilezles.org/articles/smin/
float smin( float d1, float d2, float k ) {
	float h = clamp( 0.5f + 0.5f * ( d2 - d1 ) / k, 0.0f, 1.0f );
	return mix( d2, d1, h ) - k * h * ( 1.0f - h );
}
float smax( float d1, float d2, float k ) {
	float h = clamp( 0.5f - 0.5f * ( d2 - d1 ) / k, 0.0f, 1.0f );
	return mix( d2, d1, h ) + k * h * ( 1.0f - h );
}


// https://iquilezles.org/articles/palettes/
vec3 palette ( float t ) {
	return 0.55f + 0.45f * cos( TAU * ( vec3( 1.0f ) * t + vec3( 0.5f, 0.25f, 0.75f ) ) );
}

vec3 palette2 ( float t ) {
	return 0.58f + 0.42f * cos( TAU * ( vec3( 1.0f, 0.6f, 0.6f ) * t + vec3( 0.5f, 0.3f, 0.25f ) ) );
}

// Hash without Sine
// https://www.shadertoy.com/view/4djSRW
float hash12 ( vec2 p ) {
	vec3 p3 = fract( vec3( p.xyx ) * 0.1031f );
	p3 += dot( p3, p3.yzx + 33.33f );
	return fract( ( p3.x + p3.y ) * p3.z );
}
float hash13 ( vec3 p3 ) {
	p3  = fract( p3 * 0.1031f );
	p3 += dot( p3, p3.zyx + 31.32f );
	return fract( ( p3.x + p3.y ) * p3.z );
}
vec2 hash22 ( vec2 p ) {
	vec3 p3 = fract( vec3( p.xyx ) * vec3( 0.1031f, 0.1030f, 0.0973f ) );
	p3 += dot( p3, p3.yzx + 33.33f );
	return fract( ( p3.xx+p3.yz ) * p3.zy );
}
vec2 hash23 ( vec3 p3 ) {
	p3 = fract( p3 * vec3( 0.1031f, 0.1030f, 0.0973f ) );
	p3 += dot( p3, p3.yzx + 33.33f );
	return fract( ( p3.xx + p3.yz ) * p3.zy );
}
vec3 hash33 ( vec3 p3 ) {
	p3 = fract( p3 * vec3( 0.1031f, 0.1030f, 0.0973f));
	p3 += dot( p3, p3.yxz + 33.33f );
	return fract( ( p3.xxy + p3.yxx ) * p3.zyx );
}

vec3 sRGBToLinear ( vec3 col ) {
	return mix( pow( ( col + 0.055f ) / 1.055f, vec3( 2.4f ) ), col / 12.92f, lessThan( col, vec3( 0.04045f ) ) );
}

vec3 linearTosRGB ( vec3 col ) {
	return mix( 1.055f * pow( col, vec3( 1.0f / 2.4f ) ) - 0.055f, col * 12.92f, lessThan( col, vec3( 0.0031308f ) ) );
}

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm ( vec3 x ) {
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp( ( x * ( a * x + b ) ) / ( x * ( c * x + d ) + e ), 0.0f, 1.0f );
}

vec3 getID ( vec3 p ) {
	p -= 0.5f;
	p.y /= sqrt( 2.0f );
	vec3 id0 = floor( p );
	vec3 id1 = floor( p + 0.5f );
	vec3 p0 = fract( p ) - 0.5f;
	vec3 p1 = fract( p + 0.5f ) - 0.5f;
	p0.y *= sqrt( 2.0 );
	p1.y *= sqrt( 2.0 );
	float d0 = dot( p0, p0 );
	float d1 = dot( p1, p1 );
	vec3 id = d0 < d1 ? id0 : id1 - 0.5f;
	id += 0.5f;
	id.y *= sqrt( 2.0f );
	return id;
}

vec3 getLightsID ( vec3 p ) {
	const float s = EMI_SCALE;
	p.xz *= rot2D( PI / 4.0f );
	p.yx *= rot2D( PI / 4.0f );
	vec3 id = ( floor( p / s ) + 0.5f ) * s;
	id.yx *= rot2D( -PI / 4.0f );
	id.xz *= rot2D( -PI / 4.0f );
	return id;
}

vec3 IDToPos ( vec3 id ) {
	id = id * 0.5f + 0.5f;
	id.y *= sqrt( 2.0f );
	return id;
}

vec3 posToID ( vec3 p ) {
	p.y *= 1.0f / sqrt( 2.0f );
	p = ( p - 0.5f ) * 2.0f;
	return floor( p );
}

vec3 getCameraPos ( float time ) {
	return vec3( sin( time * 0.3f ) * 3.0f + cos( time * 0.8f ) * 2.0f, cos( time * 0.2f ) + sin( time * 0.5f ) * 4.0f, time * CAMERA_SPEED );
}

/*
float map ( vec3 p ) {
	//p = IDToPos( posToID( p ) );
	p.y *= -1.0f;
	vec3 dp = p - vec3( -2.0f, 5.0f, 7.0f );
	dp *= 0.1f / 32.0f;
	float d = texture( iChannel1, dp ).r * 0.5f;
	d += texture( iChannel1, dp * 2.0f ).r * 0.25f;
	d += texture( iChannel1, dp * 4.0f ).r * 0.125f;
	d = d / 0.825f - 0.51f;
	p.y *= -1.0f;
	vec3 po = texture( iChannel2, 0.3f * p / 32.0f ).rgb - 0.5f;
	vec3 pc = p + po * 4.0f;
	float pdc = ( length( pc.xy - getCameraPos( pc.z / CAMERA_SPEED ).xy ) ) - 4.8f;
	d = max( d * 32.0f / 0.1f, -pdc * CAMERA_SPEED );
	return d;
}
*/

vec3 mapGrad ( vec3 p, float eps ) {
	vec2 e = vec2( eps, 0.0f );
	return ( map( p ) - vec3(
		map( p - e.xyy ),
		map( p - e.yxy ),
		map( p - e.yyx )
	) ) / e.x;
}

vec3 getColor ( vec3 id ) {
	id.y /= sqrt( 2.0f );
	id = floor( id * 2.0f );
	float h = abs( ( sin( id.x * 0.15f ) + sin( id.y * 0.25f ) + cos( id.z * 0.4f ) ) * 0.333f );
	return vec3( palette2( h ) );
	return palette2( hash13( id ) );
}

vec3 getEmission ( vec3 id ) {
	id.y /= sqrt( 2.0f );
	id = floor( id * 2.0f );
	float h = ( ( sin( id.x * 0.03f ) + sin( id.y * 0.08f ) + cos( id.z * 0.12f ) ) * 0.333f );
	h = fract( h + iTime * 0.6f );
	return vec3( 0.0f );
}

struct HitInfo {
	float t;
	vec3 n;
	vec3 id;
};

bool trace ( vec3 ro, vec3 rd, out HitInfo hit, float tmax ) {
	const vec3 rdn = vec3( 0.5f, -0.5f, sqrt( 2.0f ) / 2.0f );
	const mat4x3 rdm = mat4x3( rdn.xzx, rdn.yzx, rdn.xzy, rdn.yzy );
	const float s = 0.5f;

	vec3 id = getID( ro );
	vec3 pp = ro;

	vec2 ird0 = 1.0f / rd.xz;
	vec2 k0 = abs( ird0 ) * s;
	vec2 srd0 = sign( ird0 );

	vec3 n0 = -srd0.x * vec3( 1.0f, 0.0f, 0.0f );
	vec3 n1 = -srd0.y * vec3( 0.0f, 0.0f, 1.0f );

	vec4 ird1 = 1.0f / vec4( dot( rd, rdn.xzx ), dot( rd, rdn.yzx ), dot( rd, rdn.xzy ), dot( rd, rdn.yzy ) );
	vec4 k1 = abs( ird1 ) * s;
	vec4 srd1 = sign( ird1 );

	vec3 n2 = -srd1.x * rdn.xzx;
	vec3 n3 = -srd1.y * rdn.yzx;
	vec3 n4 = -srd1.z * rdn.xzy;
	vec3 n5 = -srd1.w * rdn.yzy;

	vec3 n;
	float t;
	bool surf = false;

	for (int i = 0; i < STEPS; i++) {
		float d = map( id );
		if ( d < 0.0f ) {
			if ( d < -3.0f ) {
				vec2 st = sphereIntersection( ro - ( id + 0.5f ), rd, 0.5f );
				if ( st.x > t && st.x < MAX_DIST ) {
					hit.t = st.x;
					hit.n = normalize( ro - ( id + 0.5f ) + rd * st.x );
					hit.id = id;
					return true;
				}
			} else {
				hit.t = t;
				hit.n = n;
				hit.id = id;
				return true;
			}
		}

		vec3 pp = ro - ( id + 0.5f );
		vec2 id0 = ird0 * pp.xz;
		vec4 id1 = ird1 * vec4( dot( pp, rdn.xzx ), dot( pp, rdn.yzx ), dot( pp, rdn.xzy ), dot( pp, rdn.yzy ) );
		vec2 sd0 = -id0 + k0;
		vec4 sd1 = -id1 + k1;
		float t0 = min( sd0.x, sd0.y );
		float t1 = min( min( sd1.x, sd1.y ), min( sd1.z, sd1.w ) );

		vec3 nrd;
		if ( t1 < t0 ) {
			if ( sd1.x == t1 ) {
				nrd = n2;
			} else if ( sd1.y == t1 ) {
				nrd = n3;
			} else if ( sd1.z == t1 ) {
				nrd = n4;
			} else {
				nrd = n5;
			}
		} else if ( sd0.x == t0 ) {
			nrd = n0;
		} else {
			nrd = n1;
		}

		float tF = min( t0, t1 );
		if ( tF >= tmax )
			return false;

		t = tF;
		n = nrd;
		id -= n;
	}
	return false;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
	vec2 ndc = ( 2.0f * ( fragCoord ) - iResolution.xy ) / iResolution.y;
	vec2 uv = fragCoord / iResolution.xy;

	const float fov = 120.0f;
	const float invTanFov = 1.0f / tan( radians( fov ) * 0.5f );

	vec3 ro = getCameraPos( iTime );
	vec3 lo = vec3( 0.0f, 0.0f, 1.0f );

	vec2 m = iMouse.xy / iResolution.xy;
	float ax = iMouse.x == 0.0f ? -PI * 0.7f + iTime * 0.2f : -m.x * TAU + PI;
	float ay = iMouse.y == 0.0f ? PI * 0.2f : -m.y * PI + PI * 0.5f;

	mat3 cmat = getCameraMatrix( ro, lo );

	vec3 rd = normalize( cmat * vec3( ndc, invTanFov ) );
	vec3 col = vec3( 0.0f );

	const vec3 rdn = vec3( 0.5f, -0.5f, sqrt( 2.0f ) / 2.0f );
	vec3 pos = ro;
	vec3 id = getID( pos );
	vec3 n;
	float t = 0.0f;

	HitInfo hit;
	bool isHit = trace( pos, rd, hit, MAX_DIST );
	t = isHit ? hit.t : MAX_DIST;
	n = hit.n;
	id = hit.id;

	pos = pos + rd * t;
	id = getID( pos - n * EPS );

	vec3 alb = getColor( id );
	vec3 emi = getEmission( id );

	vec3 fogCol = vec3( 0.6f, 0.85f, 1.0f ) * 0.6f;
	vec3 ambCol = fogCol;
	vec3 accCol = vec3( 1.0f, 0.12f, 0.15f );

	float ft = smoothstep( MAX_DIST * 0.7f, MAX_DIST, t ) * smoothstep( 0.98f, 1.0f, rd.z );

	fogCol = mix(fogCol, accCol * 5.0f, saturate( ft ) );
	vec3 ref = reflect( rd, n );

	vec3 ligCol = vec3( 1.0f, 0.85f, 0.7f ) * 1.5f;
	vec3 lpos = getCameraPos( iTime + 1.25f );
	vec3 ldir = lpos - pos;
	float ldist = length( ldir );
	ldir /= ldist;

	HitInfo hitL;
	bool isHitL = trace( pos + n * EPS, ldir, hitL, ldist );

	float atten = 1.0f / ( 0.8f + 0.08f * ldist + 0.05f * ldist * ldist );
	float dif = max( dot( n, ldir ), 0.0f ) * float( !isHitL );
	float spec = pow( max( dot( ref, ldir ), 0.0f ), 5.0f ) * float( !isHitL );

	float ao = smoothstep( -2.5f, 1.0f, map( pos ) / length( mapGrad( pos, 1.0f ) ) );

	col += alb * ligCol * dif * atten * 1.8f;
	col += alb * ligCol * spec * atten * 1.0f * step( hash13( id ), 0.5f );
	col += ( alb * 0.2f + 0.1f ) * ambCol;

	vec3 emiPos = getID( getLightsID( id ) );
	vec3 emiDir = emiPos - pos;
	float edist = length( emiDir );
	emiDir /= edist;

	float edif = max( dot( n, emiDir ), 0.0f );

	HitInfo hitE;
	bool isHitE = trace( pos + n * EPS, emiDir, hitE, edist );
	vec3 eid = getID( pos + n * EPS + emiDir * hitE.t - hitE.n * 1e-3f );
	bool isE = isHitE && distance( emiPos, hitE.id ) < 1e-3f;
	float estr = 10.0f * ( ( sin( ( hash13( emiPos ) + iTime * 0.25f ) * TAU ) * 0.5f + 0.5f ) * 0.8f + 0.2f );
	vec3 ecol = accCol * estr;
	float eAtten = 1.0f / ( 0.8f + 0.4f * edist + 0.3f * edist * edist );
	col += alb * edif * ecol * eAtten * float( isE );
	col *= ao * 0.8f + 0.2f;

	if ( distance( emiPos, id ) < 1e-3f )
		col = ecol;

	col = mix( col, fogCol, 1.0f - exp( -max( t * t - 50.0f, 0.0f ) * 0.0003f ) );

	if ( !isHit )
		col = fogCol;

	fragColor = vec4( col, t );
}
