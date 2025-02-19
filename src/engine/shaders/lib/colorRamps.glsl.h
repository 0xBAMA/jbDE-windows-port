// from https://www.shadertoy.com/view/Nd3fR2

/*
Grayscale is boring. Feel free to use these for your projects.

I wanted more colormaps to choose from than Mattz functions. I had
a cmap translator into textures, graphs, and csv laying around.
I added a simple scipy fitter to translate any cmap into poly6
functions too. It doesn't work for all colormaps, but all the
pretty ones work great.

If you want to add a colormap thats not here you can use my
python code to make it yourself (https://pastebin.com/mf5GfGCc)

If you wonder how all the other colormaps look, see an
incomplete list the matplotlib documentation
(https://matplotlib.org/stable/tutorials/colors/colormaps.html)
or my graphs with everything (https://imgur.com/a/xfZlxlp).

If you want to imporve the default mpl color maps use gamma
adjustment suggested by xrx to remove color bleeding:
fragColor.xyz = pow(fragColor.xyz,vec3( .455));f
 Gamma Colorf  Adjustment isf  showcased here:
https://www.shadertoy.com/view/WdXGzl
*/

// makes afmhot colormap with polynimal 6
vec3 afmhot ( float t ) {
	const vec3 c0 = vec3( -0.020390f, 0.009557f, 0.018508f );
	const vec3 c1 = vec3( 3.108226f, -0.106297f, -1.105891f );
	const vec3 c2 = vec3( -14.539061f, -2.943057f, 14.548595f );
	const vec3 c3 = vec3( 71.394557f, 22.644423f, -71.418400f );
	const vec3 c4 = vec3( -152.022488f, -31.024563f, 152.048692f );
	const vec3 c5 = vec3( 139.593599f, 12.411251f, -139.604042f );
	const vec3 c6 = vec3( -46.532952f, -0.000874f, 46.532928f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes Blues_r colormap with polynimal 6
vec3 Blues_r ( float t ) {
	const vec3 c0 = vec3( 0.042660f, 0.186181f, 0.409512f );
	const vec3 c1 = vec3( -0.703712f, 1.094974f, 2.049478f );
	const vec3 c2 = vec3( 7.995725f, -0.686110f, -4.998203f );
	const vec3 c3 = vec3( -24.421963f, 2.680736f, 7.532937f );
	const vec3 c4 = vec3( 47.519089f, -4.615112f, -5.126531f );
	const vec3 c5 = vec3( -46.038418f, 2.606781f, 0.685560f );
	const vec3 c6 = vec3( 16.586546f, -0.279280f, 0.447047f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes bone colormap with polynimal 6
vec3 Bone ( float t ) {
	const vec3 c0 = vec3( -0.005007f, -0.003054f, 0.004092f );
	const vec3 c1 = vec3( 1.098251f, 0.964561f, 0.971829f );
	const vec3 c2 = vec3( -2.688698f, -0.537516f, 2.444353f );
	const vec3 c3 = vec3( 12.667310f, -0.657473f, -8.158684f );
	const vec3 c4 = vec3( -27.183124f, 8.398806f, 10.182004f );
	const vec3 c5 = vec3( 26.505377f, -12.576925f, -5.329155f );
	const vec3 c6 = vec3( -9.395265f, 5.416416f, 0.883918f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes BuPu_r colormap with polynimal 6
vec3 BuPu_r ( float t ) {
	const vec3 c0 = vec3( 0.290975f, 0.010073f, 0.283355f );
	const vec3 c1 = vec3( 2.284922f, -0.278000f, 1.989093f );
	const vec3 c2 = vec3( -6.399278f, 8.646163f, -3.757712f );
	const vec3 c3 = vec3( 2.681161f, -20.491129f, 4.065305f );
	const vec3 c4 = vec3( 12.990405f, 24.836197f, 0.365773f );
	const vec3 c5 = vec3( -16.216830f, -16.111779f, -4.006291f );
	const vec3 c6 = vec3( 5.331023f, 4.380922f, 2.057249f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes cividis colormap with polynimal 6
vec3 cividis ( float t ) {
	const vec3 c0 = vec3( -0.008598f, 0.136152f, 0.291357f );
	const vec3 c1 = vec3( -0.415049f, 0.639599f, 3.028812f );
	const vec3 c2 = vec3( 15.655097f, 0.392899f, -22.640943f );
	const vec3 c3 = vec3( -59.689584f, -1.424169f, 75.666364f );
	const vec3 c4 = vec3( 103.509006f, 2.627500f, -122.512551f );
	const vec3 c5 = vec3( -84.086992f, -2.156916f, 94.888003f );
	const vec3 c6 = vec3( 26.055600f, 0.691800f, -28.537831f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes CMRmap colormap with polynimal 6
vec3 CMRmap ( float t ) {
	const vec3 c0 = vec3( -0.046981f, 0.001239f, 0.005501f );
	const vec3 c1 = vec3( 4.080583f, 1.192717f, 3.049337f );
	const vec3 c2 = vec3( -38.877409f, 1.524425f, 20.200215f );
	const vec3 c3 = vec3( 189.038452f, -32.746447f, -140.774611f );
	const vec3 c4 = vec3( -382.197327f, 95.587531f, 270.024592f );
	const vec3 c5 = vec3( 339.891791f, -100.379096f, -212.471161f );
	const vec3 c6 = vec3( -110.928480f, 35.828481f, 60.985694f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes coolwarm colormap with polynimal 6
vec3 coolwarm ( float t ) {
	const vec3 c0 = vec3( 0.227376f, 0.286898f, 0.752999f );
	const vec3 c1 = vec3( 1.204846f, 2.314886f, 1.563499f );
	const vec3 c2 = vec3( 0.102341f, -7.369214f, -1.860252f );
	const vec3 c3 = vec3( 2.218624f, 32.578457f, -1.643751f );
	const vec3 c4 = vec3( -5.076863f, -75.374676f, -3.704589f );
	const vec3 c5 = vec3( 1.336276f, 73.453060f, 9.595678f );
	const vec3 c6 = vec3( 0.694723f, -25.863102f, -4.558659f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes cubehelix colormap with polynimal 6
vec3 cubehelix ( float t ) {
	const vec3 c0 = vec3( -0.079465f, 0.040608f, -0.009636f );
	const vec3 c1 = vec3( 6.121943f, -1.666276f, 1.342651f );
	const vec3 c2 = vec3( -61.373834f, 27.620334f, 19.280747f );
	const vec3 c3 = vec3( 240.127160f, -93.314549f, -154.494465f );
	const vec3 c4 = vec3( -404.129586f, 133.012936f, 388.857101f );
	const vec3 c5 = vec3( 306.008802f, -81.526778f, -397.337219f );
	const vec3 c6 = vec3( -85.633074f, 16.800478f, 143.433300f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes gist_earth colormap with polynimal 6
vec3 gist_earth ( float t ) {
	const vec3 c0 = vec3( -0.005626f, -0.032771f, 0.229230f );
	const vec3 c1 = vec3( 0.628905f, 1.462908f, 4.617318f );
	const vec3 c2 = vec3( 3.960921f, 9.740478f, -25.721645f );
	const vec3 c3 = vec3( -32.735710f, -53.470618f, 60.568598f );
	const vec3 c4 = vec3( 91.584783f, 109.398709f, -74.866221f );
	const vec3 c5 = vec3( -101.138314f, -103.815111f, 48.418061f );
	const vec3 c6 = vec3( 38.745198f, 37.752237f, -12.232828f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes hsv colormap with polynimal 6
vec3 hsv ( float t ) {
	const vec3 c0 = vec3( 0.834511f, -0.153764f, -0.139860f );
	const vec3 c1 = vec3( 8.297883f, 13.629371f, 7.673034f );
	const vec3 c2 = vec3( -80.602944f, -80.577977f, -90.865764f );
	const vec3 c3 = vec3( 245.028545f, 291.294154f, 390.181844f );
	const vec3 c4 = vec3( -376.406597f, -575.667879f, -714.180803f );
	const vec3 c5 = vec3( 306.639709f, 538.472148f, 596.580595f );
	const vec3 c6 = vec3( -102.934273f, -187.108098f, -189.286489f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes inferno colormap with polynimal 6
vec3 inferno ( float t ) {
	const vec3 c0 = vec3( 0.000129f, 0.001094f, -0.041044f );
	const vec3 c1 = vec3( 0.083266f, 0.574933f, 4.155398f );
	const vec3 c2 = vec3( 11.783686f, -4.013093f, -16.439814f );
	const vec3 c3 = vec3( -42.246539f, 17.689298f, 45.210269f );
	const vec3 c4 = vec3( 78.087062f, -33.838649f, -83.264061f );
	const vec3 c5 = vec3( -72.108852f, 32.950143f, 74.479447f );
	const vec3 c6 = vec3( 25.378501f, -12.368929f, -23.407604f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

vec3 inferno2( float t ) {
	const vec3 c0 = vec3( 0.0002189403691192265f, 0.001651004631001012f, -0.01948089843709184f );
	const vec3 c1 = vec3( 0.1065134194856116f, 0.5639564367884091f, 3.932712388889277f );
	const vec3 c2 = vec3( 11.60249308247187f, -3.972853965665698f, -15.9423941062914f );
	const vec3 c3 = vec3( -41.70399613139459f, 17.43639888205313f, 44.35414519872813f );
	const vec3 c4 = vec3( 77.162935699427f, -33.40235894210092f, -81.80730925738993f );
	const vec3 c5 = vec3( -71.31942824499214f, 32.62606426397723f, 73.20951985803202f );
	const vec3 c6 = vec3( 25.13112622477341f, -12.24266895238567f, -23.07032500287172f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes jet colormap with polynimal 6
vec3 jet ( float t ) {
	const vec3 c0 = vec3( -0.071839f, 0.105300f, 0.510959f );
	const vec3 c1 = vec3( 3.434264f, -5.856282f, 5.020179f );
	const vec3 c2 = vec3( -35.088272f, 62.590515f, -12.661725f );
	const vec3 c3 = vec3( 125.621078f, -187.192678f, 8.399805f );
	const vec3 c4 = vec3( -179.495111f, 277.458688f, -26.089763f );
	const vec3 c5 = vec3( 113.825719f, -218.486063f, 52.463600f );
	const vec3 c6 = vec3( -27.714880f, 71.427477f, -27.714893f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes jet colormap with polynimal 6
vec3 magma ( float t ) {
	const vec3 c0 = vec3( -0.002292f, -0.001348f, -0.011890f );
	const vec3 c1 = vec3( 0.234451f, 0.702427f, 2.497211f );
	const vec3 c2 = vec3( 8.459706f, -3.649448f, 0.385699f );
	const vec3 c3 = vec3( -28.029205f, 14.441378f, -13.820938f );
	const vec3 c4 = vec3( 52.814176f, -28.301374f, 13.021646f );
	const vec3 c5 = vec3( -51.349945f, 29.406659f, 4.305315f );
	const vec3 c6 = vec3( 18.877608f, -11.626687f, -5.627010f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes ocean colormap with polynimal 6
vec3 ocean ( float t ) {
	const vec3 c0 = vec3( 0.005727f, 0.451550f, -0.000941f );
	const vec3 c1 = vec3( -0.112625f, 1.079697f, 1.001170f );
	const vec3 c2 = vec3( -0.930272f, -28.415474f, 0.004744f );
	const vec3 c3 = vec3( 15.125713f, 109.226840f, -0.011841f );
	const vec3 c4 = vec3( -54.993643f, -168.660130f, 0.012964f );
	const vec3 c5 = vec3( 74.155713f, 119.622305f, -0.005110f );
	const vec3 c6 = vec3( -32.297651f, -32.297650f, -0.000046f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes PuBu_r colormap with polynimal 6
vec3 PuBu_r ( float t ) {
	const vec3 c0 = vec3( -0.006363f, 0.212872f, 0.336555f );
	const vec3 c1 = vec3( 1.081919f, 1.510170f, 1.985891f );
	const vec3 c2 = vec3( -14.783872f, -6.062404f, -2.068039f );
	const vec3 c3 = vec3( 71.020484f, 24.455925f, -4.350981f );
	const vec3 c4 = vec3( -127.620020f, -46.977973f, 14.599012f );
	const vec3 c5 = vec3( 101.930678f, 41.789097f, -14.293631f );
	const vec3 c6 = vec3( -30.634205f, -13.967854f, 4.778537f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes rainbow colormap with polynimal 6
vec3 rainbow ( float t ) {
	const vec3 c0 = vec3( 0.503560f, -0.002932f, 1.000009f );
	const vec3 c1 = vec3( -1.294985f, 3.144463f, 0.001872f );
	const vec3 c2 = vec3( -16.971202f, 0.031355f, -1.232219f );
	const vec3 c3 = vec3( 97.134102f, -5.180126f, -0.029721f );
	const vec3 c4 = vec3( -172.585487f, -0.338714f, 0.316782f );
	const vec3 c5 = vec3( 131.971426f, 3.514534f, -0.061568f );
	const vec3 c6 = vec3( -37.784412f, -1.171512f, 0.003376f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes PuRd_r colormap with polynimal 6
vec3 PuRd_r ( float t ) {
	const vec3 c0 = vec3( 0.425808f, -0.016400f, 0.108687f );
	const vec3 c1 = vec3( 0.317304f, 0.729767f, 2.091430f );
	const vec3 c2 = vec3( 13.496685f, -7.880910f, -14.132707f );
	const vec3 c3 = vec3( -48.433187f, 38.030685f, 64.370712f );
	const vec3 c4 = vec3( 60.867293f, -65.403385f, -126.336402f );
	const vec3 c5 = vec3( -28.305816f, 50.079623f, 111.580346f );
	const vec3 c6 = vec3( 2.578842f, -14.582396f, -36.726260f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes RdYlBu_r colormap with polynimal 6
vec3 RdYlBu_r ( float t ) {
	const vec3 c0 = vec3( 0.207621f, 0.196195f, 0.618832f );
	const vec3 c1 = vec3( -0.088125f, 3.196170f, -0.353302f );
	const vec3 c2 = vec3( 8.261232f, -8.366855f, 14.368787f );
	const vec3 c3 = vec3( -2.922476f, 33.244294f, -43.419173f );
	const vec3 c4 = vec3( -34.085327f, -74.476041f, 37.159352f );
	const vec3 c5 = vec3( 50.429790f, 67.145621f, -1.750169f );
	const vec3 c6 = vec3( -21.188828f, -20.935464f, -6.501427f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes Spectral_r colormap with polynimal 6
vec3 Spectral_r ( float t ) {
	const vec3 c0 = vec3( 0.426208f, 0.275203f, 0.563277f );
	const vec3 c1 = vec3( -5.321958f, 3.761848f, 5.477444f );
	const vec3 c2 = vec3( 42.422339f, -15.057685f, -57.232349f );
	const vec3 c3 = vec3( -100.917716f, 57.029463f, 232.590601f );
	const vec3 c4 = vec3( 106.422535f, -116.177338f, -437.123306f );
	const vec3 c5 = vec3( -48.460514f, 103.570154f, 378.807920f );
	const vec3 c6 = vec3( 6.016269f, -33.393152f, -122.850806f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes twilight colormap with polynimal 6
vec3 twilight ( float t ) {
	const vec3 c0 = vec3( 0.996106f, 0.851653f, 0.940566f );
	const vec3 c1 = vec3( -6.529620f, -0.183448f, -3.940750f );
	const vec3 c2 = vec3( 40.899579f, -7.894242f, 38.569228f );
	const vec3 c3 = vec3( -155.212979f, 4.404793f, -167.925730f );
	const vec3 c4 = vec3( 296.687222f, 24.084913f, 315.087856f );
	const vec3 c5 = vec3( -261.270519f, -29.995422f, -266.972991f );
	const vec3 c6 = vec3( 85.335349f, 9.602600f, 85.227117f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes twilight_shifted colormap with polynimal 6
vec3 twilight_shifted ( float t ) {
	const vec3 c0 = vec3( 0.120488f, 0.047735f, 0.106111f );
	const vec3 c1 = vec3( 5.175161f, 0.597944f, 7.333840f );
	const vec3 c2 = vec3( -47.426009f, -0.862094f, -49.143485f );
	const vec3 c3 = vec3( 197.225325f, 47.538667f, 194.773468f );
	const vec3 c4 = vec3( -361.218441f, -146.888121f, -389.642741f );
	const vec3 c5 = vec3( 298.941929f, 151.947507f, 359.860766f );
	const vec3 c6 = vec3( -92.697067f, -52.312119f, -123.143476f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes viridis colormap with polynimal 6
vec3 viridis ( float t ) {
	const vec3 c0 = vec3( 0.274344f, 0.004462f, 0.331359f );
	const vec3 c1 = vec3( 0.108915f, 1.397291f, 1.388110f );
	const vec3 c2 = vec3( -0.319631f, 0.243490f, 0.156419f );
	const vec3 c3 = vec3( -4.629188f, -5.882803f, -19.646115f );
	const vec3 c4 = vec3( 6.181719f, 14.388598f, 57.442181f );
	const vec3 c5 = vec3( 4.876952f, -13.955112f, -66.125783f );
	const vec3 c6 = vec3( -5.513165f, 4.709245f, 26.582180f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

vec3 viridis2( float t ) {
	const vec3 c0 = vec3( 0.2777273272234177f, 0.005407344544966578f, 0.3340998053353061f );
	const vec3 c1 = vec3( 0.1050930431085774f, 1.404613529898575f, 1.384590162594685f );
	const vec3 c2 = vec3( -0.3308618287255563f, 0.214847559468213f, 0.09509516302823659f );
	const vec3 c3 = vec3( -4.634230498983486f, -5.799100973351585f, -19.33244095627987f );
	const vec3 c4 = vec3( 6.228269936347081f, 14.17993336680509f, 56.69055260068105f );
	const vec3 c5 = vec3( 4.776384997670288f, -13.74514537774601f, -65.35303263337234f );
	const vec3 c6 = vec3( -5.435455855934631f, 4.645852612178535f, 26.3124352495832f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes YlGnGn_r colormap with polynimal 6
vec3 YlGnGn_r ( float t ) {
	const vec3 c0 = vec3( 0.006153f, 0.269865f, 0.154795f );
	const vec3 c1 = vec3( -0.563452f, 1.218061f, 0.825586f );
	const vec3 c2 = vec3( 7.296193f, -2.560031f, -5.402727f );
	const vec3 c3 = vec3( -19.990101f, 12.478140f, 25.051507f );
	const vec3 c4 = vec3( 37.139815f, -26.377692f, -45.607642f );
	const vec3 c5 = vec3( -35.072408f, 24.166247f, 36.357837f );
	const vec3 c6 = vec3( 12.187661f, -8.203542f, -10.475316f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes YlGnBu_r colormap with polynimal 6
vec3 YlGnBu_r ( float t ) {
	const vec3 c0 = vec3( 0.016999f, 0.127718f, 0.329492f );
	const vec3 c1 = vec3( 1.571728f, 0.025897f, 2.853610f );
	const vec3 c2 = vec3( -4.414197f, 5.924816f, -11.635781f );
	const vec3 c3 = vec3( -12.438137f, -8.086194f, 34.584365f );
	const vec3 c4 = vec3( 67.131044f, -2.929808f, -58.635788f );
	const vec3 c5 = vec3( -82.372983f, 11.898509f, 47.184502f );
	const vec3 c6 = vec3( 31.515446f, -5.975157f, -13.820580f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

// makes YlGnBu_r colormap with polynimal 6
vec3 YlGnRd_r ( float t ) {
	const vec3 c0 = vec3( 0.501291f, 0.002062f, 0.146180f );
	const vec3 c1 = vec3( 1.930635f, -0.014549f, 0.382222f );
	const vec3 c2 = vec3( 0.252402f, -2.449429f, -6.385366f );
	const vec3 c3 = vec3( -10.884918f, 30.497903f, 29.134150f );
	const vec3 c4 = vec3( 18.654329f, -67.528678f, -54.909286f );
	const vec3 c5 = vec3( -12.193478f, 59.311181f, 49.311295f );
	const vec3 c6 = vec3( 2.736321f, -18.828760f, -16.894758f );
	return c0 + t * ( c1 + t * ( c2 + t * ( c3 + t * ( c4 + t * ( c5 + t * c6 ) ) ) ) );
}

#define AFMHOT		0
#define BLUESR		1
#define BONE		2
#define BUPUR		3
#define CIVIDIS		4
#define CMRMAP		5
#define COOLWARM	6
#define CUBEHELIX	7
#define GISTEARTH	8
#define HSV			9
#define INFERNO		10
#define INFERNO2	11
#define JET			12
#define MAGMA		13
#define OCEAN		14
#define PUBUR		15
#define RAINBOW		16
#define PURDR		17
#define RDYLBUR		18
#define SPECTRALR	19
#define TWILIGHT	20
#define TWILIGHT2	21
#define VIRIDIS		22
#define VIRIDIS2	23
#define YLGNGNR		24
#define YLGNBUR		25
#define YLGNRDR		26

vec4 refPalette ( float t, int idx ) {
	vec4 fragColor = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	switch ( idx ) {
		case AFMHOT:	fragColor.xyz = afmhot( t );			break;
		case BLUESR:	fragColor.xyz = Blues_r( t );			break;
		case BONE:		fragColor.xyz = Bone( t );				break;
		case BUPUR:		fragColor.xyz = BuPu_r( t );			break;
		case CIVIDIS:	fragColor.xyz = cividis( t );			break;
		case CMRMAP:	fragColor.xyz = CMRmap( t );			break;
		case COOLWARM:	fragColor.xyz = coolwarm( t );			break;
		case CUBEHELIX:	fragColor.xyz = cubehelix( t );			break;
		case GISTEARTH:	fragColor.xyz = gist_earth( t );		break;
		case HSV:		fragColor.xyz = hsv( t );				break;
		case INFERNO:	fragColor.xyz = inferno( t );			break;
		case INFERNO2:	fragColor.xyz = inferno2( t );			break;
		case JET:		fragColor.xyz = jet( t );				break;
		case MAGMA:		fragColor.xyz = magma( t );				break;
		case OCEAN:		fragColor.xyz = ocean( t );				break;
		case PUBUR:		fragColor.xyz = PuBu_r( t );			break;
		case RAINBOW:	fragColor.xyz = rainbow( t );			break;
		case PURDR:		fragColor.xyz = PuRd_r( t );			break;
		case RDYLBUR:	fragColor.xyz = RdYlBu_r( t );			break;
		case SPECTRALR:	fragColor.xyz = Spectral_r( t );		break;
		case TWILIGHT:	fragColor.xyz = twilight( t );			break;
		case TWILIGHT2:	fragColor.xyz = twilight_shifted( t );	break;
		case VIRIDIS:	fragColor.xyz = viridis( t );			break;
		case VIRIDIS2:	fragColor.xyz = viridis2( t );			break;
		case YLGNGNR:	fragColor.xyz = YlGnGn_r( t );			break;
		case YLGNBUR:	fragColor.xyz = YlGnBu_r( t );			break;
		case YLGNRDR:	fragColor.xyz = YlGnRd_r( t );			break;
		default:												break;
	};
	return fragColor;
}