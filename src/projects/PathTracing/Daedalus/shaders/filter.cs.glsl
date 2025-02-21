#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) readonly uniform image2D sourceData;
layout( rgba32f ) writeonly uniform image2D destData;

// median filter code from Nameless
float luma( vec3 c ) {
	return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

vec3 Gaussian( ivec2 writeLoc ) {
	return ( // 3x3 gaussian kernel
		1.0f * imageLoad( sourceData, writeLoc + ivec2( -1, -1 ) ).rgb +
		1.0f * imageLoad( sourceData, writeLoc + ivec2( -1,  1 ) ).rgb +
		1.0f * imageLoad( sourceData, writeLoc + ivec2(  1, -1 ) ).rgb +
		1.0f * imageLoad( sourceData, writeLoc + ivec2(  1,  1 ) ).rgb +
		2.0f * imageLoad( sourceData, writeLoc + ivec2(  0,  1 ) ).rgb +
		2.0f * imageLoad( sourceData, writeLoc + ivec2(  0, -1 ) ).rgb +
		2.0f * imageLoad( sourceData, writeLoc + ivec2(  1,  0 ) ).rgb +
		2.0f * imageLoad( sourceData, writeLoc + ivec2( -1,  0 ) ).rgb +
		4.0f * imageLoad( sourceData, writeLoc + ivec2(  0,  0 ) ).rgb ) / 16.0f;
}

vec3 lumaSort( vec3 a, vec3 b, vec3 c ) {
	float lm1 = luma( a );
	float lm2 = luma( b );
	float lm3 = luma( c );

	float lowest = min( lm1, min( lm2, lm3 ) );
	float maxest = max( lm1, max( lm2, lm3 ) );

	if ( lowest == lm1 ) {
		if ( maxest == lm2 ) {
			return c;
		} else {
			return b;
		}
	}

	if ( lowest == lm2 ) {
		if ( maxest == lm1 ) {
			return c;
		} else {
			return a;
		}
	}

	if ( lowest == lm3 ) {
		if ( maxest == lm1 ) {
			return b;
		} else {
			return a;
		}
	}

	return a;
}

vec3 median( ivec2 uv ) {
	const vec3 c = vec3( 0.0f );
	vec3 colArray[9] = vec3[](c,c,c,c,c,c,c,c,c);

	for ( int i = 0; i < 9; i++ ) {
		ivec2 offset = ivec2( ( i % 3 ) - 1, ( i / 3 ) - 1 );
		vec3 currCol = imageLoad( sourceData, uv + offset ).xyz;
		colArray[ i ] = currCol;
	}

	vec3 first = lumaSort( colArray[ 0 ], colArray[ 1 ], colArray[ 2 ] );
	vec3 second = lumaSort( colArray[ 3 ], colArray[ 4 ], colArray[ 5 ] );
	vec3 third = lumaSort( colArray[ 6 ], colArray[ 7 ], colArray[ 8 ] );
	return lumaSort( first, second, third );
}

// code from Nameless
vec3 Kuwahara( ivec2 loc ) {

	float sl1 = 0.0f;
	float sl2 = 0.0f;
	float sl3 = 0.0f;
	float sl4 = 0.0f;

	vec3 slv1 = vec3( 0.0f );
	vec3 slv2 = vec3( 0.0f );
	vec3 slv3 = vec3( 0.0f );
	vec3 slv4 = vec3( 0.0f );

	float aa[ 16 ] = float[]( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
	float bb[ 16 ] = float[]( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
	float cc[ 16 ] = float[]( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
	float dd[ 16 ] = float[]( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );

	int a1 = 0;
	int a2 = 0;
	int a3 = 0;
	int a4 = 0;

	for ( int i = 0; i < 49; i++ ) {
		ivec2 coords = ivec2( i % 7, i / 7 ) - ivec2( 3 );
		vec3 currS = imageLoad( sourceData, loc + coords ).xyz;
		if ( coords.x <= 0.0f && coords.y <= 0.0f ) {
			slv1 += currS / 16.0f;
			sl1 += luma( currS ) / 16.0f;
			aa[ a1 ] = luma( currS );
			a1++;
		}
		if ( coords.x >= 0.0f && coords.y <= 0.0f ) {
			slv2 += currS / 16.0f;
			sl2 += luma( currS ) / 16.0f;
			bb[ a2 ] = luma( currS );
			a2++;
		}
		if ( coords.x <= 0.0f && coords.y >= 0.0f ) {
			slv3 += currS / 16.0f;
			sl3 += luma( currS ) / 16.0f;
			cc[ a3 ] = luma( currS );
			a3++;
		}
		if ( coords.x >= 0.0f && coords.y >= 0.0f ) {
			slv4 += currS / 16.0f;
			sl4 += luma( currS ) / 16.0f;
			dd[ a4 ] = luma( currS );
			a4++;
		}
	}

	float sss1 = 0.0f;
	float sss2 = 0.0f;
	float sss3 = 0.0f;
	float sss4 = 0.0f;
	for ( int i = 0; i < 16; i++ ) {
		float k1 = aa[ i ];
		float k2 = bb[ i ];
		float k3 = cc[ i ];
		float k4 = dd[ i ];
		sss1 += pow( k1 - sl1, 2.0f ) / 16.0f;
		sss2 += pow( k2 - sl2, 2.0f ) / 16.0f;
		sss3 += pow( k3 - sl3, 2.0f ) / 16.0f;
		sss4 += pow( k4 - sl4, 2.0f ) / 16.0f;
	}

	sss1 = sqrt( sss1 );
	sss2 = sqrt( sss2 );
	sss3 = sqrt( sss3 );
	sss4 = sqrt( sss4 );
	float mins = min( sss1, min( sss2, min( sss3, sss4 ) ) );

	if ( mins == sss1 ) {
		return slv1;
	} else if ( mins == sss2 ) {
		return slv2;
	} else if ( mins == sss3 ) {
		return slv3;
	}
	return slv4;
}

// from nameless
// int midean ( in vec3 a ) {
// 	int maxx = max( a.x, max( a.y, a.z ) );
// 	int minx = min( a.x, min( a.y, a.z ) );

// 	return ( maxx == a.x ) ? ( minx == a.z ) ? 1 : 2 : ( maxx == a.y ) ? ( minx == a.x ) ? 2 : 0 : ( minx == a.x ) ? 1 : 0;
// }

// vec3 medianof9s(vec2 uv, vec2 iResolution, float offsetS, sampler2D tex){
// 	vec3 colArray[9];

// 	for(int i = 0; i < 9; i++){
// 		vec2 offset = vec2(float(i%3)-1., float(i/3)-1.)*offsetS;
// 		vec3 currCol = texelFetch(tex, ivec2((uv*iResolution + offset)*0.5),0).rgb;
// 		colArray[i] = currCol;
// 	}

// 	int ms1 = int(midean(vec3(lum(colArray[0]), lum(colArray[1]),lum(colArray[2]))));
// 	int ms2 = int(midean(vec3(lum(colArray[3]), lum(colArray[4]),lum(colArray[5]))));
// 	int ms3 = int(midean(vec3(lum(colArray[6]), lum(colArray[7]),lum(colArray[8]))));


// 	int id1 = ms1;
// 	int id2 = 3+ms2;
// 	int id3 = 6+ms3;

// 	int finid = int(midean(vec3(lum(colArray[id1]),lum(colArray[id2]),lum(colArray[id3]))));

// 	//return (finid == 0)?colArray[id1]:(finid == 1)?colArray[id2]:colArray[id3];

// 	int as = id1 + (id2 - id1)*finid;
// 	return colArray[as + (id3-as)*max(finid-1,0)];
// }

#define GAUSSIAN	0
#define MEDIAN		1
#define BOTH		2
#define KUWAHARA	3
// #define MEDIAN2		4
uniform int mode;

void main () {
	// write the data to the image
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const float sampleCount = imageLoad( sourceData, writeLoc ).a;

	vec3 result;
	switch ( mode ) {
	case GAUSSIAN:
		result = Gaussian( writeLoc );
		break;

	case MEDIAN:
		result = median( writeLoc );
		break;

	case BOTH:
		result = ( // 3x3 gaussian kernel, but each tap is a 3x3 median filter
			1.0f * median( writeLoc + ivec2( -1, -1 ) ) +
			1.0f * median( writeLoc + ivec2( -1,  1 ) ) +
			1.0f * median( writeLoc + ivec2(  1, -1 ) ) +
			1.0f * median( writeLoc + ivec2(  1,  1 ) ) +
			2.0f * median( writeLoc + ivec2(  0,  1 ) ) +
			2.0f * median( writeLoc + ivec2(  0, -1 ) ) +
			2.0f * median( writeLoc + ivec2(  1,  0 ) ) +
			2.0f * median( writeLoc + ivec2( -1,  0 ) ) +
			4.0f * median( writeLoc + ivec2(  0,  0 ) ) ) / 16.0f;
		break;

	case KUWAHARA:
		result = Kuwahara( writeLoc );
		break;

	// case MEDIAN2:
	}

	imageStore( destData, writeLoc, vec4( result, sampleCount ) );
}
