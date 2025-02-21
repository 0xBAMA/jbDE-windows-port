#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform sampler2D sourceC;
uniform sampler2D sourceDN;
layout( rgba8 ) uniform image2D displayTexture;

#include "tonemap.glsl" // tonemapping curves

uniform int tonemapMode;
uniform float gamma;
uniform float postExposure;
uniform mat3 saturation;
uniform vec3 colorTempAdjust;
uniform vec2 resolution;
uniform vec3 bgColor;

// bayer pattern stuff from izutionix
// https://www.shadertoy.com/view/mlK3Dz

/*
#define iterBayerMat 15
//https://www.shadertoy.com/view/ltsyWl
#define bayer2x2(a) (4-(a).x-((a).y<<1))%4
//return bayer matrix (bitwise operands for speed over compatibility)
float GetBayerFromCoordLevel(vec2 pixelpos) {
    ivec2 p=ivec2(pixelpos);
    int a=0;
    for(int i=0; i<iterBayerMat; i++) {
        a += bayer2x2(p >> (iterBayerMat-1-i) & 1) << (2*i);
    }
    return float(a)/float(2<<(iterBayerMat*2-1));
}

float Bayer2(vec2 a) {
    a = floor(a);
    return fract(a.x / 2. + a.y * a.y * .75);
}
// https://www.shadertoy.com/view/7sfXDn
#define Bayer4(a)     (Bayer2  (.5 *(a)) * .25 + Bayer2(a))
#define Bayer8(a)     (Bayer4  (.5 *(a)) * .25 + Bayer2(a))
#define Bayer16(a)    (Bayer8  (.5 *(a)) * .25 + Bayer2(a))
#define Bayer32(a)    (Bayer16 (.5 *(a)) * .25 + Bayer2(a))
#define Bayer64(a)    (Bayer32 (.5 *(a)) * .25 + Bayer2(a))
#define Bayer128(a)   (Bayer64 (.5 *(a)) * .25 + Bayer2(a))
#define Bayer256(a)   (Bayer128(.5 *(a)) * .25 + Bayer2(a))
#define Bayer512(a)   (Bayer256(.5 *(a)) * .25 + Bayer2(a))
#define Bayer1024(a)  (Bayer512(.5 *(a)) * .25 + Bayer2(a))


#define scale 2.

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    //float dithering = (Bayer1024(fragCoord / scale) * 2.0 - 1.0) * 0.5;
    float dithering = GetBayerFromCoordLevel(fragCoord / scale);
    vec2 uv = fragCoord / iResolution.xy;
 
    //uv.x += dithering;
    
    vec2 mouse = iMouse.xy/iResolution.xy;
    if(dot(iMouse.xy, vec2(1.))<=0.) // no mouse?
        mouse = vec2(1.-2.*abs(fract(iTime*.05)-0.5),-1.); // move threshold left-right;

    fragColor = vec4(dithering < pow(mouse.x, 3.));//vec4(uv.x < 0.5);
    //fragColor = vec4(pow(dithering, 256.));
}
*/

bool inBounds ( in ivec2 loc ) {
	return !(
		loc.x < 0 ||
		loc.y < 0 ||
		loc.x >= imageSize( displayTexture ).x ||
		loc.y >= imageSize( displayTexture ).y );

	// vec2 fLoc = loc / imageSize( displayTexture );
	// return !( fLoc.x < 0.0f || fLoc.y < 0.0f || fLoc.x > 1.0f || fLoc.y > 1.0f );
}

#define OUTPUT		0
#define ACCUMULATOR	1
#define NORMAL		2
#define NORMALR		3
#define DEPTH		4

uniform int activeMode;

void main () {

	// there is something wrong with the sampling here - I'm not sure exactly what it is
		// I need to put an XOR or something with exact per-pixel detail in sourceC and evaluate

	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	vec2 sampleLoc = ( vec2( loc ) + vec2( 0.5f ) ) / resolution;
	vec4 originalValue;

	switch ( activeMode ) {
		case OUTPUT:
			originalValue = postExposure * texture( sourceC, sampleLoc );
			originalValue.rgb = saturation * originalValue.rgb;
			originalValue.rgb = colorTempAdjust * originalValue.rgb;
			originalValue.rgb = Tonemap( tonemapMode, originalValue.rgb );
			originalValue.rgb = GammaCorrect( gamma, originalValue.rgb );
			break;

		case ACCUMULATOR:
			originalValue = texture( sourceC, sampleLoc );
			break;

		case NORMAL:
			originalValue = texture( sourceDN, sampleLoc );
			break;

		case NORMALR:
			originalValue.rgb = 0.5f * texture( sourceDN, sampleLoc ).xyz + vec3( 0.5f );
			break;

		case DEPTH:
			originalValue.rgb = vec3( postExposure / texture( sourceDN, sampleLoc ).a );
			break;

		default: // no
			originalValue.rgb = vec3( 0.0f );
			break;
	}

	originalValue.a = 1.0f;
	if ( inBounds( loc ) ) {
		imageStore( displayTexture, loc, originalValue );
	}
}
