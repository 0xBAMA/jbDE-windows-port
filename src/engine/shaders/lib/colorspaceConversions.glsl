// this could do with a cleanup pass at some point in the future

// Define saturation macro, if not already user-defined
#ifndef saturate
#define saturate(v) clamp(v, 0, 1)
#endif

// Constants
const float HCV_EPSILON = 1e-10;
const float HSL_EPSILON = 1e-10;
const float HCY_EPSILON = 1e-10;

const float SRGB_GAMMA = 1.0 / 2.2;
const float SRGB_INVERSE_GAMMA = 2.2;
const float SRGB_ALPHA = 0.055;

// Used to convert from linear RGB to XYZ space
const mat3 RGB_2_XYZ = ( mat3(
	0.4124564, 0.2126729, 0.0193339,
	0.3575761, 0.7151522, 0.1191920,
	0.1804375, 0.0721750, 0.9503041
) );

// Used to convert from XYZ to linear RGB space
const mat3 XYZ_2_RGB = ( mat3(
	3.2404542, -0.9692660, 0.0556434,
	-1.5371385, 1.8760108, -0.2040259,
	-0.4985314, 0.0415560, 1.0572252
) );

const vec3 LUMA_COEFFS = vec3(0.2126, 0.7152, 0.0722);

// Returns the luminance of a !! linear !! rgb color
float get_luminance(vec3 rgb)
{
	return dot(LUMA_COEFFS, rgb);
}

// Converts a linear rgb color to a srgb color (approximated, but fast)
vec3 rgb_to_srgb_approx(vec3 rgb)
{
	return pow(rgb, vec3(SRGB_GAMMA));
}

// Converts a srgb color to a rgb color (approximated, but fast)
vec3 srgb_to_rgb_approx(vec3 srgb)
{
	return pow(srgb, vec3(SRGB_INVERSE_GAMMA));
}

// Converts a single linear channel to srgb
float linear_to_srgb(float channel)
{
	if (channel <= 0.0031308)
		return 12.92 * channel;
	else
		return (1.0 + SRGB_ALPHA) * pow(channel, 1.0 / 2.4) - SRGB_ALPHA;
}

// Converts a single srgb channel to rgb
float srgb_to_linear(float channel)
{
	if (channel <= 0.04045)
		return channel / 12.92;
	else
		return pow((channel + SRGB_ALPHA) / (1.0 + SRGB_ALPHA), 2.4);
}

// Converts a linear rgb color to a srgb color (exact, not approximated)
vec3 rgb_to_srgb(vec3 rgb)
{
	return vec3(
		linear_to_srgb(rgb.r),
		linear_to_srgb(rgb.g),
		linear_to_srgb(rgb.b));
}

// Converts a srgb color to a linear rgb color (exact, not approximated)
vec3 srgb_to_rgb(vec3 srgb)
{
	return vec3(
		srgb_to_linear(srgb.r),
		srgb_to_linear(srgb.g),
		srgb_to_linear(srgb.b));
}

// Converts a color from linear RGB to XYZ space
vec3 rgb_to_xyz(vec3 rgb)
{
	return RGB_2_XYZ * rgb;
}

// Converts a color from XYZ to linear RGB space
vec3 xyz_to_rgb(vec3 xyz)
{
	return XYZ_2_RGB * xyz;
}

// Converts a color from XYZ to xyY space (Y is luminosity)
vec3 xyz_to_xyY(vec3 xyz)
{
	float Y = xyz.y;
	float x = xyz.x / (xyz.x + xyz.y + xyz.z);
	float y = xyz.y / (xyz.x + xyz.y + xyz.z);
	return vec3(x, y, Y);
}

// Converts a color from xyY space to XYZ space
vec3 xyY_to_xyz(vec3 xyY)
{
	float Y = xyY.z;
	float x = Y * xyY.x / xyY.y;
	float z = Y * (1.0 - xyY.x - xyY.y) / xyY.y;
	return vec3(x, Y, z);
}

// Converts a color from linear RGB to xyY space
vec3 rgb_to_xyY(vec3 rgb)
{
	vec3 xyz = rgb_to_xyz(rgb);
	return xyz_to_xyY(xyz);
}

// Converts a color from xyY space to linear RGB
vec3 xyY_to_rgb(vec3 xyY)
{
	vec3 xyz = xyY_to_xyz(xyY);
	return xyz_to_rgb(xyz);
}

// Converts a value from linear RGB to HCV (Hue, Chroma, Value)
vec3 rgb_to_hcv(vec3 rgb)
{
	// Based on work by Sam Hocevar and Emil Persson
	vec4 P = (rgb.g < rgb.b) ? vec4(rgb.bg, -1.0, 2.0 / 3.0) : vec4(rgb.gb, 0.0, -1.0 / 3.0);
	vec4 Q = (rgb.r < P.x) ? vec4(P.xyw, rgb.r) : vec4(rgb.r, P.yzx);
	float C = Q.x - min(Q.w, Q.y);
	float H = abs((Q.w - Q.y) / (6 * C + HCV_EPSILON) + Q.z);
	return vec3(H, C, Q.x);
}

// Converts from pure Hue to linear RGB
vec3 hue_to_rgb(float hue)
{
	float R = abs(hue * 6 - 3) - 1;
	float G = 2 - abs(hue * 6 - 2);
	float B = 2 - abs(hue * 6 - 4);
	return saturate(vec3(R, G, B));
}

// Converts from HSV to linear RGB
vec3 hsv_to_rgb(vec3 hsv)
{
	vec3 rgb = hue_to_rgb(hsv.x);
	return ((rgb - 1.0) * hsv.y + 1.0) * hsv.z;
}

// Converts from HSL to linear RGB
vec3 hsl_to_rgb(vec3 hsl)
{
	vec3 rgb = hue_to_rgb(hsl.x);
	float C = (1 - abs(2 * hsl.z - 1)) * hsl.y;
	return (rgb - 0.5) * C + hsl.z;
}

// Converts from HCY to linear RGB
vec3 hcy_to_rgb(vec3 hcy)
{
	const vec3 HCYwts = vec3(0.299, 0.587, 0.114);
	vec3 RGB = hue_to_rgb(hcy.x);
	float Z = dot(RGB, HCYwts);
	if (hcy.z < Z)
	{
		hcy.y *= hcy.z / Z;
	}
	else if (Z < 1)
	{
		hcy.y *= (1 - hcy.z) / (1 - Z);
	}
	return (RGB - Z) * hcy.y + hcy.z;
}

// Converts from linear RGB to HSV
vec3 rgb_to_hsv(vec3 rgb)
{
	vec3 HCV = rgb_to_hcv(rgb);
	float S = HCV.y / (HCV.z + HCV_EPSILON);
	return vec3(HCV.x, S, HCV.z);
}

// Converts from linear rgb to HSL
vec3 rgb_to_hsl(vec3 rgb)
{
	vec3 HCV = rgb_to_hcv(rgb);
	float L = HCV.z - HCV.y * 0.5;
	float S = HCV.y / (1 - abs(L * 2 - 1) + HSL_EPSILON);
	return vec3(HCV.x, S, L);
}

// Converts from rgb to hcy (Hue, Chroma, Luminance)
vec3 rgb_to_hcy(vec3 rgb)
{
	const vec3 HCYwts = vec3(0.299, 0.587, 0.114);
	// Corrected by David Schaeffer
	vec3 HCV = rgb_to_hcv(rgb);
	float Y = dot(rgb, HCYwts);
	float Z = dot(hue_to_rgb(HCV.x), HCYwts);
	if (Y < Z)
	{
		HCV.y *= Z / (HCY_EPSILON + Y);
	}
	else
	{
		HCV.y *= (1 - Z) / (HCY_EPSILON + 1 - Y);
	}
	return vec3(HCV.x, HCV.y, Y);
}

// RGB to YCbCr, ranges [0, 1]
vec3 rgb_to_ycbcr(vec3 rgb)
{
	float y = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;
	float cb = (rgb.b - y) * 0.565;
	float cr = (rgb.r - y) * 0.713;
	return vec3(y, cb, cr);
}

// YCbCr to RGB
vec3 ycbcr_to_rgb(vec3 yuv)
{
	return vec3(
		yuv.x + 1.403 * yuv.z,
		yuv.x - 0.344 * yuv.y - 0.714 * yuv.z,
		yuv.x + 1.770 * yuv.y);
}

// Fill out the matrix of conversions by converting to rgb first and
// then to the desired color space.

// To srgb
vec3 xyz_to_srgb(vec3 xyz) { return rgb_to_srgb(xyz_to_rgb(xyz)); }
vec3 xyY_to_srgb(vec3 xyY) { return rgb_to_srgb(xyY_to_rgb(xyY)); }
vec3 hue_to_srgb(float hue) { return rgb_to_srgb(hue_to_rgb(hue)); }
vec3 hsv_to_srgb(vec3 hsv) { return rgb_to_srgb(hsv_to_rgb(hsv)); }
vec3 hsl_to_srgb(vec3 hsl) { return rgb_to_srgb(hsl_to_rgb(hsl)); }
vec3 hcy_to_srgb(vec3 hcy) { return rgb_to_srgb(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_srgb(vec3 yuv) { return rgb_to_srgb(ycbcr_to_rgb(yuv)); }

// To xyz
vec3 srgb_to_xyz(vec3 srgb) { return rgb_to_xyz(srgb_to_rgb(srgb)); }
vec3 hue_to_xyz(float hue) { return rgb_to_xyz(hue_to_rgb(hue)); }
vec3 hsv_to_xyz(vec3 hsv) { return rgb_to_xyz(hsv_to_rgb(hsv)); }
vec3 hsl_to_xyz(vec3 hsl) { return rgb_to_xyz(hsl_to_rgb(hsl)); }
vec3 hcy_to_xyz(vec3 hcy) { return rgb_to_xyz(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_xyz(vec3 yuv) { return rgb_to_xyz(ycbcr_to_rgb(yuv)); }

// To xyY
vec3 srgb_to_xyY(vec3 srgb) { return rgb_to_xyY(srgb_to_rgb(srgb)); }
vec3 hue_to_xyY(float hue) { return rgb_to_xyY(hue_to_rgb(hue)); }
vec3 hsv_to_xyY(vec3 hsv) { return rgb_to_xyY(hsv_to_rgb(hsv)); }
vec3 hsl_to_xyY(vec3 hsl) { return rgb_to_xyY(hsl_to_rgb(hsl)); }
vec3 hcy_to_xyY(vec3 hcy) { return rgb_to_xyY(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_xyY(vec3 yuv) { return rgb_to_xyY(ycbcr_to_rgb(yuv)); }

// To HCV
vec3 srgb_to_hcv(vec3 srgb) { return rgb_to_hcv(srgb_to_rgb(srgb)); }
vec3 xyz_to_hcv(vec3 xyz) { return rgb_to_hcv(xyz_to_rgb(xyz)); }
vec3 xyY_to_hcv(vec3 xyY) { return rgb_to_hcv(xyY_to_rgb(xyY)); }
vec3 hue_to_hcv(float hue) { return rgb_to_hcv(hue_to_rgb(hue)); }
vec3 hsv_to_hcv(vec3 hsv) { return rgb_to_hcv(hsv_to_rgb(hsv)); }
vec3 hsl_to_hcv(vec3 hsl) { return rgb_to_hcv(hsl_to_rgb(hsl)); }
vec3 hcy_to_hcv(vec3 hcy) { return rgb_to_hcv(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_hcv(vec3 yuv) { return rgb_to_hcy(ycbcr_to_rgb(yuv)); }

// To HSV
vec3 srgb_to_hsv(vec3 srgb) { return rgb_to_hsv(srgb_to_rgb(srgb)); }
vec3 xyz_to_hsv(vec3 xyz) { return rgb_to_hsv(xyz_to_rgb(xyz)); }
vec3 xyY_to_hsv(vec3 xyY) { return rgb_to_hsv(xyY_to_rgb(xyY)); }
vec3 hue_to_hsv(float hue) { return rgb_to_hsv(hue_to_rgb(hue)); }
vec3 hsl_to_hsv(vec3 hsl) { return rgb_to_hsv(hsl_to_rgb(hsl)); }
vec3 hcy_to_hsv(vec3 hcy) { return rgb_to_hsv(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_hsv(vec3 yuv) { return rgb_to_hsv(ycbcr_to_rgb(yuv)); }

// To HSL
vec3 srgb_to_hsl(vec3 srgb) { return rgb_to_hsl(srgb_to_rgb(srgb)); }
vec3 xyz_to_hsl(vec3 xyz) { return rgb_to_hsl(xyz_to_rgb(xyz)); }
vec3 xyY_to_hsl(vec3 xyY) { return rgb_to_hsl(xyY_to_rgb(xyY)); }
vec3 hue_to_hsl(float hue) { return rgb_to_hsl(hue_to_rgb(hue)); }
vec3 hsv_to_hsl(vec3 hsv) { return rgb_to_hsl(hsv_to_rgb(hsv)); }
vec3 hcy_to_hsl(vec3 hcy) { return rgb_to_hsl(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_hsl(vec3 yuv) { return rgb_to_hsl(ycbcr_to_rgb(yuv)); }

// To HCY
vec3 srgb_to_hcy(vec3 srgb) { return rgb_to_hcy(srgb_to_rgb(srgb)); }
vec3 xyz_to_hcy(vec3 xyz) { return rgb_to_hcy(xyz_to_rgb(xyz)); }
vec3 xyY_to_hcy(vec3 xyY) { return rgb_to_hcy(xyY_to_rgb(xyY)); }
vec3 hue_to_hcy(float hue) { return rgb_to_hcy(hue_to_rgb(hue)); }
vec3 hsv_to_hcy(vec3 hsv) { return rgb_to_hcy(hsv_to_rgb(hsv)); }
vec3 hsl_to_hcy(vec3 hsl) { return rgb_to_hcy(hsl_to_rgb(hsl)); }
vec3 ycbcr_to_hcy(vec3 yuv) { return rgb_to_hcy(ycbcr_to_rgb(yuv)); }

// YCbCr
vec3 srgb_to_ycbcr(vec3 srgb) { return rgb_to_ycbcr(srgb_to_rgb(srgb)); }
vec3 xyz_to_ycbcr(vec3 xyz) { return rgb_to_ycbcr(xyz_to_rgb(xyz)); }
vec3 xyY_to_ycbcr(vec3 xyY) { return rgb_to_ycbcr(xyY_to_rgb(xyY)); }
vec3 hue_to_ycbcr(float hue) { return rgb_to_ycbcr(hue_to_rgb(hue)); }
vec3 hsv_to_ycbcr(vec3 hsv) { return rgb_to_ycbcr(hsv_to_rgb(hsv)); }
vec3 hsl_to_ycbcr(vec3 hsl) { return rgb_to_ycbcr(hsl_to_rgb(hsl)); }
vec3 hcy_to_ycbcr(vec3 hcy) { return rgb_to_ycbcr(hcy_to_rgb(hcy)); }

// end tobspr color space conversions

//----------------------------------------------------------------------------
// these come from: https://www.shadertoy.com/view/4dcSRN

// YUV, generic conversion
// ranges: Y=0..1, U=-uvmax.x..uvmax.x, V=-uvmax.x..uvmax.x

vec3 yuv_rgb(vec3 yuv, vec2 wbwr, vec2 uvmax)
{
	vec2 br = yuv.x + yuv.yz * (1.0 - wbwr) / uvmax;
	float g = (yuv.x - dot(wbwr, br)) / (1.0 - wbwr.x - wbwr.y);
	return vec3(br.y, g, br.x);
}

vec3 rgb_yuv(vec3 rgb, vec2 wbwr, vec2 uvmax)
{
	float y = wbwr.y * rgb.r + (1.0 - wbwr.x - wbwr.y) * rgb.g + wbwr.x * rgb.b;
	return vec3(y, uvmax * (rgb.br - y) / (1.0 - wbwr));
}

//----------------------------------------------------------------------------

// YUV, HDTV, gamma compressed, ITU-R BT.709
// ranges: Y=0..1, U=-0.436..0.436, V=-0.615..0.615

vec3 yuv_rgb(vec3 yuv)
{
	return yuv_rgb(yuv, vec2(0.0722, 0.2126), vec2(0.436, 0.615));
}

vec3 rgb_yuv(vec3 rgb)
{
	return rgb_yuv(rgb, vec2(0.0722, 0.2126), vec2(0.436, 0.615));
}

//----------------------------------------------------------------------------

// Y*b*r, generic conversion
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

vec3 ypbpr_rgb(vec3 ybr, vec2 kbkr)
{
	return yuv_rgb(ybr, kbkr, vec2(0.5));
}

vec3 rgb_ypbpr(vec3 rgb, vec2 kbkr)
{
	return rgb_yuv(rgb, kbkr, vec2(0.5));
}

//----------------------------------------------------------------------------

// YPbPr, analog, gamma compressed, HDTV
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YPbPr to RGB, after ITU-R BT.709
vec3 ypbpr_rgb(vec3 ypbpr)
{
	return ypbpr_rgb(ypbpr, vec2(0.0722, 0.2126));
}

// RGB to YPbPr, after ITU-R BT.709
vec3 rgb_ypbpr(vec3 rgb)
{
	return rgb_ypbpr(rgb, vec2(0.0722, 0.2126));
}

//----------------------------------------------------------------------------

// YPbPr, analog, gamma compressed, VGA, TV
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YPbPr to RGB, after ITU-R BT.601
vec3 ypbpr_rgb_bt601(vec3 ypbpr)
{
	return ypbpr_rgb(ypbpr, vec2(0.114, 0.299));
}

// RGB to YPbPr, after ITU-R BT.601
vec3 rgb_ypbpr_bt601(vec3 rgb)
{
	return rgb_ypbpr(rgb, vec2(0.114, 0.299));
}

//----------------------------------------------------------------------------

// in the original implementation, the factors and offsets are
// ypbpr * (219, 224, 224) + (16, 128, 128)

// YPbPr to YCbCr (analog to digital)
vec3 ypbpr_ycbcr(vec3 ypbpr)
{
	return ypbpr * vec3(0.85546875, 0.875, 0.875) + vec3(0.0625, 0.5, 0.5);
}

// YCbCr to YPbPr (digital to analog)
vec3 ycbcr_ypbpr(vec3 ycbcr)
{
	return (ycbcr - vec3(0.0625, 0.5, 0.5)) / vec3(0.85546875, 0.875, 0.875);
}

//----------------------------------------------------------------------------

// YCbCr, digital, gamma compressed
// ranges: Y=0..1, b=0..1, r=0..1

// YCbCr to RGB (generic)
vec3 ycbcr_rgb(vec3 ycbcr, vec2 kbkr)
{
	return ypbpr_rgb(ycbcr_ypbpr(ycbcr), kbkr);
}
// RGB to YCbCr (generic)
vec3 rgb_ycbcr(vec3 rgb, vec2 kbkr)
{
	return ypbpr_ycbcr(rgb_ypbpr(rgb, kbkr));
}
// YCbCr to RGB
vec3 ycbcr_rgb(vec3 ycbcr)
{
	return ypbpr_rgb(ycbcr_ypbpr(ycbcr));
}
// RGB to YCbCr
vec3 rgb_ycbcr(vec3 rgb)
{
	return ypbpr_ycbcr(rgb_ypbpr(rgb));
}

//----------------------------------------------------------------------------

// ITU-R BT.2020:
// YcCbcCrc, linear
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YcCbcCrc to RGB
vec3 yccbccrc_rgb(vec3 yccbccrc)
{
	return ypbpr_rgb(yccbccrc, vec2(0.0593, 0.2627));
}

// RGB to YcCbcCrc
vec3 rgb_yccbccrc(vec3 rgb)
{
	return rgb_ypbpr(rgb, vec2(0.0593, 0.2627));
}

//----------------------------------------------------------------------------

// YCoCg
// ranges: Y=0..1, Co=-0.5..0.5, Cg=-0.5..0.5

vec3 ycocg_rgb(vec3 ycocg)
{
	vec2 br = vec2(-ycocg.y, ycocg.y) - ycocg.z;
	return ycocg.x + vec3(br.y, ycocg.z, br.x);
}

vec3 rgb_ycocg(vec3 rgb)
{
	float tmp = 0.5 * (rgb.r + rgb.b);
	float y = rgb.g + tmp;
	float Cg = rgb.g - tmp;
	float Co = rgb.r - rgb.b;
	return vec3(y, Co, Cg) * 0.5;
}

//----------------------------------------------------------------------------

vec3 yccbccrc_norm(vec3 ypbpr)
{
	vec3 p = yccbccrc_rgb(ypbpr);
	vec3 ro = yccbccrc_rgb(vec3(ypbpr.x, 0.0, 0.0));
	vec3 rd = normalize(p - ro);
	vec3 m = 1. / rd;
	vec3 b = 0.5 * abs(m) - m * (ro - 0.5);
	float tF = min(min(b.x, b.y), b.z);
	p = ro + rd * tF * max(abs(ypbpr.y), abs(ypbpr.z)) * 2.0;
	return rgb_yccbccrc(p);
}

vec3 ycocg_norm(vec3 ycocg)
{
	vec3 p = ycocg_rgb(ycocg);
	vec3 ro = ycocg_rgb(vec3(ycocg.x, 0.0, 0.0));
	vec3 rd = normalize(p - ro);
	vec3 m = 1. / rd;
	vec3 b = 0.5 * abs(m) - m * (ro - 0.5);
	float tF = min(min(b.x, b.y), b.z);
	p = ro + rd * tF * max(abs(ycocg.y), abs(ycocg.z)) * 2.0;
	return rgb_ycocg(p);
}

//----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////
// B C H
///////////////////////////////////////////////////////////////////////
// from: https://www.shadertoy.com/view/lsVGz1
vec3 rgb2DEF(vec3 _col)
{
	mat3 XYZ; // Adobe RGB (1998)
	XYZ[0] = vec3(0.5767309, 0.1855540, 0.1881852);
	XYZ[1] = vec3(0.2973769, 0.6273491, 0.0752741);
	XYZ[2] = vec3(0.0270343, 0.0706872, 0.9911085);
	mat3 DEF;
	DEF[0] = vec3(0.2053, 0.7125, 0.4670);
	DEF[1] = vec3(1.8537, -1.2797, -0.4429);
	DEF[2] = vec3(-0.3655, 1.0120, -0.6104);
	vec3 xyz = _col.rgb * XYZ;
	vec3 def = xyz * DEF;
	return def;
}

vec3 def2RGB(vec3 _def)
{
	mat3 XYZ;
	XYZ[0] = vec3(0.6712, 0.4955, 0.1540);
	XYZ[1] = vec3(0.7061, 0.0248, 0.5223);
	XYZ[2] = vec3(0.7689, -0.2556, -0.8645);
	mat3 RGB; // Adobe RGB (1998)
	RGB[0] = vec3(2.0413690, -0.5649464, -0.3446944);
	RGB[1] = vec3(-0.9692660, 1.8760108, 0.0415560);
	RGB[2] = vec3(0.0134474, -0.1183897, 1.0154096);
	vec3 xyz = _def * XYZ;
	vec3 rgb = xyz * RGB;
	return rgb;
}
float getB(vec3 _def)
{
	float b = sqrt((_def.r * _def.r) + (_def.g * _def.g) + (_def.b * _def.b));
	return b;
}
float getC(vec3 _def)
{
	vec3 def_D = vec3(1., 0., 0.);
	float C = atan(length(cross(_def, def_D)), dot(_def, def_D));
	return C;
}
float getH(vec3 _def)
{
	vec3 def_E_axis = vec3(0., 1., 0.);
	float H = atan(_def.z, _def.y) - atan(def_E_axis.z, def_E_axis.y);
	return H;
}
// RGB 2 BCH
vec3 rgb2BCH(vec3 _col)
{
	vec3 DEF = rgb2DEF(_col);
	float B = getB(DEF);
	float C = getC(DEF);
	float H = getH(DEF);
	return vec3(B, C, H);
}
// BCH 2 RGB
vec3 bch2RGB(vec3 _bch)
{
	vec3 def;
	def.x = _bch.x * cos(_bch.y);
	def.y = _bch.x * sin(_bch.y) * cos(_bch.z);
	def.z = _bch.x * sin(_bch.y) * sin(_bch.z);
	vec3 rgb = def2RGB(def);
	return rgb;
}

// BRIGHTNESS
vec3 Brightness(vec3 _col, float _f)
{
	vec3 BCH = rgb2BCH(_col);
	vec3 b3 = vec3(BCH.x, BCH.x, BCH.x);
	float x = pow((_f + 1.) / 2., 2.);
	x = _f;
	_col = _col + (b3 * x) / 3.;
	return _col;
}

// CONTRAST
// simple contrast
// needs neighboring brightness values for higher accuracy
vec3 Contrast(vec3 _col, float _f)
{
	vec3 def = rgb2DEF(_col);
	float B = getB(def);
	float C = getC(def);
	float H = getH(def);
	B = B * pow(B * (1. - C), _f);
	def.x = B * cos(C);
	def.y = B * sin(C) * cos(H);
	def.z = B * sin(C) * sin(H);
	_col.rgb = def2RGB(def);
	return _col;
}

vec3 Hue(vec3 _col, float _f)
{
	vec3 BCH = rgb2BCH(_col);
	BCH.z += _f * 3.1459 * 2.;
	BCH = bch2RGB(BCH);
	return BCH;
}

vec3 Saturation(vec3 _col, float _f)
{
	vec3 BCH = rgb2BCH(_col);
	BCH.y *= (_f + 1.);
	BCH = bch2RGB(BCH);
	return BCH;
}

///////////////////////////////////////////////////////////////////////
// chromamax colorspace from : https://www.shadertoy.com/view/3lS3Wy
// note that this one uses a four channel represntation
#define max3(a) max(a.x, max(a.y, a.z))

vec4 rgb2cm(vec3 rgb)
{
	float maximum = max3(rgb);
	return vec4(rgb / max(maximum, 1e-32) - maximum, exp2(-maximum));
}

vec3 cm2rgb(vec4 cm)
{
	float maximum = -log2(cm.a);
	return clamp(cm.rgb + maximum, 0.0, 1.0) * maximum;
}

///////////////////////////////////////////////////////////////////////
// OKLAB implementation from: https://www.shadertoy.com/view/ttcyRS
// with The MIT License
// Copyright © 2020 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Optimized linear-rgb color mix in oklab space, useful
// when our software operates in rgb space but we still
// we want to have intuitive color mixing.
//
// Now, when mixing linear rgb colors in oklab space, the
// linear transform from cone to Lab space and back can be
// omitted, saving three 3x3 transformation per blend!
//
// oklab was invented by Björn Ottosson: https://bottosson.github.io/posts/oklab
//
// More oklab on Shadertoy: https://www.shadertoy.com/view/WtccD7

// vec3 oklab_mix( vec3 colA, vec3 colB, float h )
// {
//     // https://bottosson.github.io/posts/oklab
//     const mat3 kCONEtoLMS = mat3(
//          0.4121656120,  0.2118591070,  0.0883097947,
//          0.5362752080,  0.6807189584,  0.2818474174,
//          0.0514575653,  0.1074065790,  0.6302613616);
//     const mat3 kLMStoCONE = mat3(
//          4.0767245293, -1.2681437731, -0.0041119885,
//         -3.3072168827,  2.6093323231, -0.7034763098,
//          0.2307590544, -0.3411344290,  1.7068625689);

//     // rgb to cone (arg of pow can't be negative)
//     vec3 lmsA = pow( kCONEtoLMS*colA, vec3(1.0/3.0) );
//     vec3 lmsB = pow( kCONEtoLMS*colB, vec3(1.0/3.0) );
//    // lerp
//    vec3 lms = mix( lmsA, lmsB, h );
//    // gain in the middle (no oaklab anymore, but looks better?)
// // lms *= 1.0+0.2*h*(1.0-h);
//    // cone to rgb
// return kLMStoCONE*(lms*lms*lms);
// }

//////////////////////////////////////////////////////////////////////
// alternative public domain implementation from https://www.shadertoy.com/view/WtccD7
//////////////////////////////////////////////////////////////////////

// oklab transform and inverse from
// https://bottosson.github.io/posts/oklab/

vec3 oklab_from_linear_srgb(vec3 c)
{
	const mat3 invB = mat3(0.4121656120, 0.2118591070, 0.0883097947,
						   0.5362752080, 0.6807189584, 0.2818474174,
						   0.0514575653, 0.1074065790, 0.6302613616);
	const mat3 invA = mat3(0.2104542553, 1.9779984951, 0.0259040371,
						   0.7936177850, -2.4285922050, 0.7827717662,
						   -0.0040720468, 0.4505937099, -0.8086757660);
	vec3 lms = invB * c;
	return invA * (sign(lms) * pow(abs(lms), vec3(0.3333333333333)));
}

vec3 linear_srgb_from_oklab(vec3 c)
{
	const mat3 fwdA = mat3(1.0000000000, 1.0000000000, 1.0000000000,
						   0.3963377774, -0.1055613458, -0.0894841775,
						   0.2158037573, -0.0638541728, -1.2914855480);
	const mat3 fwdB = mat3(4.0767245293, -1.2681437731, -0.0041119885,
						   -3.3072168827, 2.6093323231, -0.7034763098,
						   0.2307590544, -0.3411344290, 1.7068625689);
	vec3 lms = fwdA * c;
	return fwdB * (lms * lms * lms);
}

// Copyright(c) 2021 Björn Ottosson
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this softwareand associated documentation files(the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions :
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define M_PI 3.1415926535897932384626433832795

float cbrt(float x)
{
	return sign(x) * pow(abs(x), 1.0f / 3.0f);
}

float srgb_transfer_function(float a)
{
	return .0031308f >= a ? 12.92f * a : 1.055f * pow(a, .4166666666666667f) - .055f;
}

float srgb_transfer_function_inv(float a)
{
	return .04045f < a ? pow((a + .055f) / 1.055f, 2.4f) : a / 12.92f;
}

vec3 linear_srgb_to_oklab(vec3 c)
{
	float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
	float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
	float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

	float l_ = cbrt(l);
	float m_ = cbrt(m);
	float s_ = cbrt(s);

	return vec3(
		0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
		1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
		0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_);
}

vec3 oklab_to_linear_srgb(vec3 c)
{
	float l_ = c.x + 0.3963377774f * c.y + 0.2158037573f * c.z;
	float m_ = c.x - 0.1055613458f * c.y - 0.0638541728f * c.z;
	float s_ = c.x - 0.0894841775f * c.y - 1.2914855480f * c.z;

	float l = l_ * l_ * l_;
	float m = m_ * m_ * m_;
	float s = s_ * s_ * s_;

	return vec3(
		+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
		-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
		-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s);
}

// Finds the maximum saturation possible for a given hue that fits in sRGB
// Saturation here is defined as S = C/L
// a and b must be normalized so a^2 + b^2 == 1
float compute_max_saturation(float a, float b)
{
	// Max saturation will be when one of r, g or b goes below zero.

	// Select different coefficients depending on which component goes below zero first
	float k0, k1, k2, k3, k4, wl, wm, ws;

	if (-1.88170328f * a - 0.80936493f * b > 1.f)
	{
		// Red component
		k0 = +1.19086277f;
		k1 = +1.76576728f;
		k2 = +0.59662641f;
		k3 = +0.75515197f;
		k4 = +0.56771245f;
		wl = +4.0767416621f;
		wm = -3.3077115913f;
		ws = +0.2309699292f;
	}
	else if (1.81444104f * a - 1.19445276f * b > 1.f)
	{
		// Green component
		k0 = +0.73956515f;
		k1 = -0.45954404f;
		k2 = +0.08285427f;
		k3 = +0.12541070f;
		k4 = +0.14503204f;
		wl = -1.2684380046f;
		wm = +2.6097574011f;
		ws = -0.3413193965f;
	}
	else
	{
		// Blue component
		k0 = +1.35733652f;
		k1 = -0.00915799f;
		k2 = -1.15130210f;
		k3 = -0.50559606f;
		k4 = +0.00692167f;
		wl = -0.0041960863f;
		wm = -0.7034186147f;
		ws = +1.7076147010f;
	}

	// Approximate max saturation using a polynomial:
	float S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;

	// Do one step Halley's method to get closer
	// this gives an error less than 10e6, except for some blue hues where the dS/dh is close to infinite
	// this should be sufficient for most applications, otherwise do two/three steps

	float k_l = +0.3963377774f * a + 0.2158037573f * b;
	float k_m = -0.1055613458f * a - 0.0638541728f * b;
	float k_s = -0.0894841775f * a - 1.2914855480f * b;

	{
		float l_ = 1.f + S * k_l;
		float m_ = 1.f + S * k_m;
		float s_ = 1.f + S * k_s;

		float l = l_ * l_ * l_;
		float m = m_ * m_ * m_;
		float s = s_ * s_ * s_;

		float l_dS = 3.f * k_l * l_ * l_;
		float m_dS = 3.f * k_m * m_ * m_;
		float s_dS = 3.f * k_s * s_ * s_;

		float l_dS2 = 6.f * k_l * k_l * l_;
		float m_dS2 = 6.f * k_m * k_m * m_;
		float s_dS2 = 6.f * k_s * k_s * s_;

		float f = wl * l + wm * m + ws * s;
		float f1 = wl * l_dS + wm * m_dS + ws * s_dS;
		float f2 = wl * l_dS2 + wm * m_dS2 + ws * s_dS2;

		S = S - f * f1 / (f1 * f1 - 0.5f * f * f2);
	}

	return S;
}

// finds L_cusp and C_cusp for a given hue
// a and b must be normalized so a^2 + b^2 == 1
vec2 find_cusp(float a, float b)
{
	// First, find the maximum saturation (saturation S = C/L)
	float S_cusp = compute_max_saturation(a, b);

	// Convert to linear sRGB to find the first point where at least one of r,g or b >= 1:
	vec3 rgb_at_max = oklab_to_linear_srgb(vec3(1, S_cusp * a, S_cusp * b));
	float L_cusp = cbrt(1.f / max(max(rgb_at_max.r, rgb_at_max.g), rgb_at_max.b));
	float C_cusp = L_cusp * S_cusp;

	return vec2(L_cusp, C_cusp);
}

// Finds intersection of the line defined by
// L = L0 * (1 - t) + t * L1;
// C = t * C1;
// a and b must be normalized so a^2 + b^2 == 1
float find_gamut_intersection(float a, float b, float L1, float C1, float L0, vec2 cusp)
{
	// Find the intersection for upper and lower half seprately
	float t;
	if (((L1 - L0) * cusp.y - (cusp.x - L0) * C1) <= 0.f)
	{
		// Lower half

		t = cusp.y * L0 / (C1 * cusp.x + cusp.y * (L0 - L1));
	}
	else
	{
		// Upper half

		// First intersect with triangle
		t = cusp.y * (L0 - 1.f) / (C1 * (cusp.x - 1.f) + cusp.y * (L0 - L1));

		// Then one step Halley's method
		{
			float dL = L1 - L0;
			float dC = C1;

			float k_l = +0.3963377774f * a + 0.2158037573f * b;
			float k_m = -0.1055613458f * a - 0.0638541728f * b;
			float k_s = -0.0894841775f * a - 1.2914855480f * b;

			float l_dt = dL + dC * k_l;
			float m_dt = dL + dC * k_m;
			float s_dt = dL + dC * k_s;

			// If higher accuracy is required, 2 or 3 iterations of the following block can be used:
			{
				float L = L0 * (1.f - t) + t * L1;
				float C = t * C1;

				float l_ = L + C * k_l;
				float m_ = L + C * k_m;
				float s_ = L + C * k_s;

				float l = l_ * l_ * l_;
				float m = m_ * m_ * m_;
				float s = s_ * s_ * s_;

				float ldt = 3.f * l_dt * l_ * l_;
				float mdt = 3.f * m_dt * m_ * m_;
				float sdt = 3.f * s_dt * s_ * s_;

				float ldt2 = 6.f * l_dt * l_dt * l_;
				float mdt2 = 6.f * m_dt * m_dt * m_;
				float sdt2 = 6.f * s_dt * s_dt * s_;

				float r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s - 1.f;
				float r1 = 4.0767416621f * ldt - 3.3077115913f * mdt + 0.2309699292f * sdt;
				float r2 = 4.0767416621f * ldt2 - 3.3077115913f * mdt2 + 0.2309699292f * sdt2;yyyy

				float u_r = r1 / (r1 * r1 - 0.5f * r * r2);
				float t_r = -r * u_r;

				float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s - 1.f;
				float g1 = -1.2684380046f * ldt + 2.6097574011f * mdt - 0.3413193965f * sdt;
				float g2 = -1.2684380046f * ldt2 + 2.6097574011f * mdt2 - 0.3413193965f * sdt2;

				float u_g = g1 / (g1 * g1 - 0.5f * g * g2);
				float t_g = -g * u_g;

				float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s - 1.f;
				float b1 = -0.0041960863f * ldt - 0.7034186147f * mdt + 1.7076147010f * sdt;
				float b2 = -0.0041960863f * ldt2 - 0.7034186147f * mdt2 + 1.7076147010f * sdt2;

				float u_b = b1 / (b1 * b1 - 0.5f * b * b2);
				float t_b = -b * u_b;

				t_r = u_r >= 0.f ? t_r : 10000.f;
				t_g = u_g >= 0.f ? t_g : 10000.f;
				t_b = u_b >= 0.f ? t_b : 10000.f;

				t += min(t_r, min(t_g, t_b));
			}
		}
	}

	return t;
}

float find_gamut_intersection(float a, float b, float L1, float C1, float L0)
{
	// Find the cusp of the gamut triangle
	vec2 cusp = find_cusp(a, b);

	return find_gamut_intersection(a, b, L1, C1, L0, cusp);
}

float toe(float x)
{
	float k_1 = 0.206f;
	float k_2 = 0.03f;
	float k_3 = (1.f + k_1) / (1.f + k_2);
	return 0.5f * (k_3 * x - k_1 + sqrt((k_3 * x - k_1) * (k_3 * x - k_1) + 4.f * k_2 * k_3 * x));
}

float toe_inv(float x)
{
	float k_1 = 0.206f;
	float k_2 = 0.03f;
	float k_3 = (1.f + k_1) / (1.f + k_2);
	return (x * x + k_1 * x) / (k_3 * (x + k_2));
}

vec2 to_ST(vec2 cusp)
{
	float L = cusp.x;
	float C = cusp.y;
	return vec2(C / L, C / (1.f - L));
}

// Returns a smooth approximation of the location of the cusp
// This polynomial was created by an optimization process
// It has been designed so that S_mid < S_max and T_mid < T_max
vec2 get_ST_mid(float a_, float b_)
{
	float S = 0.11516993f + 1.f / (+7.44778970f + 4.15901240f * b_ + a_ * (-2.19557347f + 1.75198401f * b_ + a_ * (-2.13704948f - 10.02301043f * b_ + a_ * (-4.24894561f + 5.38770819f * b_ + 4.69891013f * a_))));

	float T = 0.11239642f + 1.f / (+1.61320320f - 0.68124379f * b_ + a_ * (+0.40370612f + 0.90148123f * b_ + a_ * (-0.27087943f + 0.61223990f * b_ + a_ * (+0.00299215f - 0.45399568f * b_ - 0.14661872f * a_))));

	return vec2(S, T);
}

vec3 get_Cs(float L, float a_, float b_)
{
	vec2 cusp = find_cusp(a_, b_);

	float C_max = find_gamut_intersection(a_, b_, L, 1.f, L, cusp);
	vec2 ST_max = to_ST(cusp);

	// Scale factor to compensate for the curved part of gamut shape:
	float k = C_max / min((L * ST_max.x), (1.f - L) * ST_max.y);

	float C_mid;
	{
		vec2 ST_mid = get_ST_mid(a_, b_);

		// Use a soft minimum function, instead of a sharp triangle shape to get a smooth value for chroma.
		float C_a = L * ST_mid.x;
		float C_b = (1.f - L) * ST_mid.y;
		C_mid = 0.9f * k * sqrt(sqrt(1.f / (1.f / (C_a * C_a * C_a * C_a) + 1.f / (C_b * C_b * C_b * C_b))));
	}

	float C_0;
	{
		// for C_0, the shape is independent of hue, so vec2 are constant. Values picked to roughly be the average values of vec2.
		float C_a = L * 0.4f;
		float C_b = (1.f - L) * 0.8f;

		// Use a soft minimum function, instead of a sharp triangle shape to get a smooth value for chroma.
		C_0 = sqrt(1.f / (1.f / (C_a * C_a) + 1.f / (C_b * C_b)));
	}

	return vec3(C_0, C_mid, C_max);
}

vec3 okhsl_to_srgb(vec3 hsl)
{
	float h = hsl.x;
	float s = hsl.y;
	float l = hsl.z;

	if (l == 1.0f)
	{
		return vec3(1.f, 1.f, 1.f);
	}

	else if (l == 0.f)
	{
		return vec3(0.f, 0.f, 0.f);
	}

	float a_ = cos(2.f * M_PI * h);
	float b_ = sin(2.f * M_PI * h);
	float L = toe_inv(l);

	vec3 cs = get_Cs(L, a_, b_);
	float C_0 = cs.x;
	float C_mid = cs.y;
	float C_max = cs.z;

	float mid = 0.8f;
	float mid_inv = 1.25f;

	float C, t, k_0, k_1, k_2;

	if (s < mid)
	{
		t = mid_inv * s;

		k_1 = mid * C_0;
		k_2 = (1.f - k_1 / C_mid);

		C = t * k_1 / (1.f - k_2 * t);
	}
	else
	{
		t = (s - mid) / (1.f - mid);

		k_0 = C_mid;
		k_1 = (1.f - mid) * C_mid * C_mid * mid_inv * mid_inv / C_0;
		k_2 = (1.f - (k_1) / (C_max - C_mid));

		C = k_0 + t * k_1 / (1.f - k_2 * t);
	}

	vec3 rgb = oklab_to_linear_srgb(vec3(L, C * a_, C * b_));
	return vec3(
		srgb_transfer_function(rgb.r),
		srgb_transfer_function(rgb.g),
		srgb_transfer_function(rgb.b));
}

vec3 srgb_to_okhsl(vec3 rgb)
{
	vec3 lab = linear_srgb_to_oklab(vec3(
		srgb_transfer_function_inv(rgb.r),
		srgb_transfer_function_inv(rgb.g),
		srgb_transfer_function_inv(rgb.b)));

	float C = sqrt(lab.y * lab.y + lab.z * lab.z);
	float a_ = lab.y / C;
	float b_ = lab.z / C;

	float L = lab.x;
	float h = 0.5f + 0.5f * atan(-lab.z, -lab.y) / M_PI;

	vec3 cs = get_Cs(L, a_, b_);
	float C_0 = cs.x;
	float C_mid = cs.y;
	float C_max = cs.z;

	// Inverse of the interpolation in okhsl_to_srgb:

	float mid = 0.8f;
	float mid_inv = 1.25f;

	float s;
	if (C < C_mid)
	{
		float k_1 = mid * C_0;
		float k_2 = (1.f - k_1 / C_mid);

		float t = C / (k_1 + k_2 * C);
		s = t * mid;
	}
	else
	{
		float k_0 = C_mid;
		float k_1 = (1.f - mid) * C_mid * C_mid * mid_inv * mid_inv / C_0;
		float k_2 = (1.f - (k_1) / (C_max - C_mid));

		float t = (C - k_0) / (k_1 + k_2 * (C - k_0));
		s = mid + (1.f - mid) * t;
	}

	float l = toe(L);
	return vec3(h, s, l);
}

vec3 okhsv_to_srgb(vec3 hsv)
{
	float h = hsv.x;
	float s = hsv.y;
	float v = hsv.z;

	float a_ = cos(2.f * M_PI * h);
	float b_ = sin(2.f * M_PI * h);

	vec2 cusp = find_cusp(a_, b_);
	vec2 ST_max = to_ST(cusp);
	float S_max = ST_max.x;
	float T_max = ST_max.y;
	float S_0 = 0.5f;
	float k = 1.f - S_0 / S_max;

	// first we compute L and V as if the gamut is a perfect triangle:

	// L, C when v==1:
	float L_v = 1.f - s * S_0 / (S_0 + T_max - T_max * k * s);
	float C_v = s * T_max * S_0 / (S_0 + T_max - T_max * k * s);

	float L = v * L_v;
	float C = v * C_v;

	// then we compensate for both toe and the curved top part of the triangle:
	float L_vt = toe_inv(L_v);
	float C_vt = C_v * L_vt / L_v;

	float L_new = toe_inv(L);
	C = C * L_new / L;
	L = L_new;

	vec3 rgb_scale = oklab_to_linear_srgb(vec3(L_vt, a_ * C_vt, b_ * C_vt));
	float scale_L = cbrt(1.f / max(max(rgb_scale.r, rgb_scale.g), max(rgb_scale.b, 0.f)));

	L = L * scale_L;
	C = C * scale_L;

	vec3 rgb = oklab_to_linear_srgb(vec3(L, C * a_, C * b_));
	return vec3(
		srgb_transfer_function(rgb.r),
		srgb_transfer_function(rgb.g),
		srgb_transfer_function(rgb.b));
}

vec3 srgb_to_okhsv(vec3 rgb)
{
	vec3 lab = linear_srgb_to_oklab(vec3(
		srgb_transfer_function_inv(rgb.r),
		srgb_transfer_function_inv(rgb.g),
		srgb_transfer_function_inv(rgb.b)));

	float C = sqrt(lab.y * lab.y + lab.z * lab.z);
	float a_ = lab.y / C;
	float b_ = lab.z / C;

	float L = lab.x;
	float h = 0.5f + 0.5f * atan(-lab.z, -lab.y) / M_PI;

	vec2 cusp = find_cusp(a_, b_);
	vec2 ST_max = to_ST(cusp);
	float S_max = ST_max.x;
	float T_max = ST_max.y;
	float S_0 = 0.5f;
	float k = 1.f - S_0 / S_max;

	// first we find L_v, C_v, L_vt and C_vt

	float t = T_max / (C + L * T_max);
	float L_v = t * L;
	float C_v = t * C;

	float L_vt = toe_inv(L_v);
	float C_vt = C_v * L_vt / L_v;

	// we can then use these to invert the step that compensates for the toe and the curved top part of the triangle:
	vec3 rgb_scale = oklab_to_linear_srgb(vec3(L_vt, a_ * C_vt, b_ * C_vt));
	float scale_L = cbrt(1.f / max(max(rgb_scale.r, rgb_scale.g), max(rgb_scale.b, 0.f)));

	L = L / scale_L;
	C = C / scale_L;

	C = C * toe(L) / L;
	L = toe(L);

	// we can now compute v and s:

	float v = L / L_v;
	float s = (S_0 + T_max) * C_v / ((T_max * S_0) + T_max * k * C_v);

	return vec3(h, s, v);
}

// 1: smoothstep, 2: smootherstep
#define SMOOTH 2

vec3 mul3(in mat3 m, in vec3 v)
{
	return vec3(
		dot(v, m[0]),
		dot(v, m[1]),
		dot(v, m[2]));
}

// Adapted from https://bottosson.github.io/posts/oklab/
// The commented code is the original code followed by GLSL adaptation.
vec3 srgb2oklab(vec3 c)
{
	// float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
	// float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
	// float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

	// The matrix to multiply by.
	mat3 m1 = mat3(
		0.4122214708, 0.5363325363, 0.0514459929,
		0.2119034982, 0.6806995451, 0.1073969566,
		0.0883024619, 0.2817188376, 0.6299787005);

	vec3 lms = mul3(m1, c);

	// float l_ = cbrtf(l);
	// float m_ = cbrtf(m);
	// float s_ = cbrtf(s);

	// Equivalent to cbrt().
	lms = pow(lms, vec3(1. / 3.));

	// return {
	//     0.2104542553f*l_ + 0.7936177850f*m_ - 0.0040720468f*s_,
	//     1.9779984951f*l_ - 2.4285922050f*m_ + 0.4505937099f*s_,
	//     0.0259040371f*l_ + 0.7827717662f*m_ - 0.8086757660f*s_,
	// };

	mat3 m2 = mat3(
		+0.2104542553, +0.7936177850, -0.0040720468,
		+1.9779984951, -2.4285922050, +0.4505937099,
		+0.0259040371, +0.7827717662, -0.8086757660);

	return mul3(m2, lms);
}

// Same as above.
vec3 oklab2srgb(vec3 c)
{
	// float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
	// float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
	// float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;

	// We have 1. as the first column since the code doesn't
	// have an argument to multiply by for cL.
	mat3 m1 = mat3(
		1.0000000000, +0.3963377774, +0.2158037573,
		1.0000000000, -0.1055613458, -0.0638541728,
		1.0000000000, -0.0894841775, -1.2914855480);

	// We need to convert the `struct Lab` c variable into `vec3`.
	vec3 lms = mul3(m1, c);

	// float l = l_*l_*l_;
	// float m = m_*m_*m_;
	// float s = s_*s_*s_;

	lms = lms * lms * lms;

	// return {
	// 	+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
	// 	-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
	// 	-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
	// };

	// this is essentially the m1 from the code before just inverted.
	mat3 m2 = mat3(
		+4.0767416621, -3.3077115913, +0.2309699292,
		-1.2684380046, +2.6097574011, -0.3413193965,
		-0.0041960863, -0.7034186147, +1.7076147010);
	return mul3(m2, lms);
}

// universal lab -> lch conversion.
vec3 lab2lch(in vec3 c)
{
	return vec3(
		c.x,
		sqrt((c.y * c.y) + (c.z * c.z)),
		atan(c.z, c.y));
}

// universal lch -> lab conversion
vec3 lch2lab(in vec3 c)
{
	return vec3(
		c.x,
		c.y * cos(c.z),
		c.y * sin(c.z));
}

// shortucts
vec3 srgb2oklch(in vec3 c)
{
	// return lab2lch(srgb2oklab(c));
	return lab2lch(srgb2oklab(c));
}

vec3 oklch2srgb(in vec3 c)
{
	return oklab2srgb(lch2lab(c));
}

//*************************************************
/*
HSLUV-GLSL v4.2
HSLUV is a human-friendly alternative to HSL. ( http://www.hsluv.org )
GLSL port by William Malo ( https://github.com/williammalo )
Put this code in your fragment shader.
*/

vec3 hsluv_intersectLineLine(vec3 line1x, vec3 line1y, vec3 line2x, vec3 line2y)
{
	return (line1y - line2y) / (line2x - line1x);
}

vec3 hsluv_distanceFromPole(vec3 pointx, vec3 pointy)
{
	return sqrt(pointx * pointx + pointy * pointy);
}

vec3 hsluv_lengthOfRayUntilIntersect(float theta, vec3 x, vec3 y)
{
	vec3 len = y / (sin(theta) - x * cos(theta));
	if (len.r < 0.0)
	{
		len.r = 1000.0;
	}
	if (len.g < 0.0)
	{
		len.g = 1000.0;
	}
	if (len.b < 0.0)
	{
		len.b = 1000.0;
	}
	return len;
}

float hsluv_maxSafeChromaForL(float L)
{
	mat3 m2 = mat3(
		3.2409699419045214, -0.96924363628087983, 0.055630079696993609,
		-1.5373831775700935, 1.8759675015077207, -0.20397695888897657,
		-0.49861076029300328, 0.041555057407175613, 1.0569715142428786);
	float sub0 = L + 16.0;
	float sub1 = sub0 * sub0 * sub0 * .000000641;
	float sub2 = sub1 > 0.0088564516790356308 ? sub1 : L / 903.2962962962963;

	vec3 top1 = (284517.0 * m2[0] - 94839.0 * m2[2]) * sub2;
	vec3 bottom = (632260.0 * m2[2] - 126452.0 * m2[1]) * sub2;
	vec3 top2 = (838422.0 * m2[2] + 769860.0 * m2[1] + 731718.0 * m2[0]) * L * sub2;

	vec3 bounds0x = top1 / bottom;
	vec3 bounds0y = top2 / bottom;

	vec3 bounds1x = top1 / (bottom + 126452.0);
	vec3 bounds1y = (top2 - 769860.0 * L) / (bottom + 126452.0);

	vec3 xs0 = hsluv_intersectLineLine(bounds0x, bounds0y, -1.0 / bounds0x, vec3(0.0));
	vec3 xs1 = hsluv_intersectLineLine(bounds1x, bounds1y, -1.0 / bounds1x, vec3(0.0));

	vec3 lengths0 = hsluv_distanceFromPole(xs0, bounds0y + xs0 * bounds0x);
	vec3 lengths1 = hsluv_distanceFromPole(xs1, bounds1y + xs1 * bounds1x);

	return min(lengths0.r,
			   min(lengths1.r,
				   min(lengths0.g,
					   min(lengths1.g,
						   min(lengths0.b,
							   lengths1.b)))));
}

float hsluv_maxChromaForLH(float L, float H)
{

	float hrad = radians(H);

	mat3 m2 = mat3(
		3.2409699419045214, -0.96924363628087983, 0.055630079696993609,
		-1.5373831775700935, 1.8759675015077207, -0.20397695888897657,
		-0.49861076029300328, 0.041555057407175613, 1.0569715142428786);
	float sub1 = pow(L + 16.0, 3.0) / 1560896.0;
	float sub2 = sub1 > 0.0088564516790356308 ? sub1 : L / 903.2962962962963;

	vec3 top1 = (284517.0 * m2[0] - 94839.0 * m2[2]) * sub2;
	vec3 bottom = (632260.0 * m2[2] - 126452.0 * m2[1]) * sub2;
	vec3 top2 = (838422.0 * m2[2] + 769860.0 * m2[1] + 731718.0 * m2[0]) * L * sub2;

	vec3 bound0x = top1 / bottom;
	vec3 bound0y = top2 / bottom;

	vec3 bound1x = top1 / (bottom + 126452.0);
	vec3 bound1y = (top2 - 769860.0 * L) / (bottom + 126452.0);

	vec3 lengths0 = hsluv_lengthOfRayUntilIntersect(hrad, bound0x, bound0y);
	vec3 lengths1 = hsluv_lengthOfRayUntilIntersect(hrad, bound1x, bound1y);

	return min(lengths0.r,
			   min(lengths1.r,
				   min(lengths0.g,
					   min(lengths1.g,
						   min(lengths0.b,
							   lengths1.b)))));
}

float hsluv_fromLinear(float c)
{
	return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}
vec3 hsluv_fromLinear(vec3 c)
{
	return vec3(hsluv_fromLinear(c.r), hsluv_fromLinear(c.g), hsluv_fromLinear(c.b));
}

float hsluv_toLinear(float c)
{
	return c > 0.04045 ? pow((c + 0.055) / (1.0 + 0.055), 2.4) : c / 12.92;
}

vec3 hsluv_toLinear(vec3 c)
{
	return vec3(hsluv_toLinear(c.r), hsluv_toLinear(c.g), hsluv_toLinear(c.b));
}

float hsluv_yToL(float Y)
{
	return Y <= 0.0088564516790356308 ? Y * 903.2962962962963 : 116.0 * pow(Y, 1.0 / 3.0) - 16.0;
}

float hsluv_lToY(float L)
{
	return L <= 8.0 ? L / 903.2962962962963 : pow((L + 16.0) / 116.0, 3.0);
}

vec3 xyzToRgb(vec3 tuple)
{
	const mat3 m = mat3(
		3.2409699419045214, -1.5373831775700935, -0.49861076029300328,
		-0.96924363628087983, 1.8759675015077207, 0.041555057407175613,
		0.055630079696993609, -0.20397695888897657, 1.0569715142428786);

	return hsluv_fromLinear(tuple * m);
}

vec3 rgbToXyz(vec3 tuple)
{
	const mat3 m = mat3(
		0.41239079926595948, 0.35758433938387796, 0.18048078840183429,
		0.21263900587151036, 0.71516867876775593, 0.072192315360733715,
		0.019330818715591851, 0.11919477979462599, 0.95053215224966058);
	return hsluv_toLinear(tuple) * m;
}

vec3 xyzToLuv(vec3 tuple)
{
	float X = tuple.x;
	float Y = tuple.y;
	float Z = tuple.z;

	float L = hsluv_yToL(Y);

	float div = 1. / dot(tuple, vec3(1, 15, 3));

	return vec3(
			   1.,
			   (52. * (X * div) - 2.57179),
			   (117. * (Y * div) - 6.08816)) *
		   L;
}

vec3 luvToXyz(vec3 tuple)
{
	float L = tuple.x;

	float U = tuple.y / (13.0 * L) + 0.19783000664283681;
	float V = tuple.z / (13.0 * L) + 0.468319994938791;

	float Y = hsluv_lToY(L);
	float X = 2.25 * U * Y / V;
	float Z = (3. / V - 5.) * Y - (X / 3.);

	return vec3(X, Y, Z);
}

vec3 luvToLch(vec3 tuple)
{
	float L = tuple.x;
	float U = tuple.y;
	float V = tuple.z;

	float C = length(tuple.yz);
	float H = degrees(atan(V, U));
	if (H < 0.0)
	{
		H = 360.0 + H;
	}

	return vec3(L, C, H);
}

vec3 lchToLuv(vec3 tuple)
{
	float hrad = radians(tuple.b);
	return vec3(
		tuple.r,
		cos(hrad) * tuple.g,
		sin(hrad) * tuple.g);
}

vec3 hsluvToLch(vec3 tuple)
{
	tuple.g *= hsluv_maxChromaForLH(tuple.b, tuple.r) * .01;
	return tuple.bgr;
}

vec3 lchToHsluv(vec3 tuple)
{
	tuple.g /= hsluv_maxChromaForLH(tuple.r, tuple.b) * .01;
	return tuple.bgr;
}

vec3 hpluvToLch(vec3 tuple)
{
	tuple.g *= hsluv_maxSafeChromaForL(tuple.b) * .01;
	return tuple.bgr;
}

vec3 lchToHpluv(vec3 tuple)
{
	tuple.g /= hsluv_maxSafeChromaForL(tuple.r) * .01;
	return tuple.bgr;
}

// LCH
vec3 lchToRgb(vec3 tuple)
{
	return xyzToRgb(luvToXyz(lchToLuv(tuple)));
}

vec3 rgbToLch(vec3 tuple)
{
	return luvToLch(xyzToLuv(rgbToXyz(tuple)));
}

// HSLUV
vec3 hsluvToRgb(vec3 tuple)
{
	return lchToRgb(hsluvToLch(tuple));
}

vec3 rgbToHsluv(vec3 tuple)
{
	return lchToHsluv(rgbToLch(tuple));
}

// HPLUV
vec3 hpluvToRgb(vec3 tuple)
{
	return lchToRgb(hpluvToLch(tuple));
}

vec3 rgbToHpluv(vec3 tuple)
{
	return lchToHpluv(rgbToLch(tuple));
}

// LUV
vec3 luvToRgb(vec3 tuple)
{
	return xyzToRgb(luvToXyz(tuple));
}

vec3 rgbToLuv(vec3 tuple)
{
	return xyzToLuv(rgbToXyz(tuple));
}

// do some #define statements to make the below switch statements more legible
#define RGB			0
#define SRGB		1
#define XYZ			2
#define XYY			3
#define HSV			4
#define HSL			5
#define HCY			6
#define YPBPR		7
#define YPBPR601	8
#define YCBCR1		9
#define YCBCR2		10
#define YCCBCCRC	11
#define YCOCG		12
#define BCH			13
#define CHROMAMAX	14
#define OKLAB		15
#define OKHSL		16
#define OKHSV		17
#define OKLCH		18
#define LCH			19
#define HSLUV		20
#define HPLUV		21
#define LUV			22

vec4 convert ( uvec4 value, int spaceswitch ) {
	vec4 converted = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	vec4 base_rgbval = vec4( value ) / 255.0f;
	switch ( spaceswitch ) {
		case RGB:		converted = base_rgbval; break;
		case SRGB:		converted.rgb = rgb_to_srgb( base_rgbval.rgb ); break;
		case XYZ:		converted.rgb = rgb_to_xyz( base_rgbval.rgb ); break;
		case XYY:		converted.rgb = rgb_to_xyY( base_rgbval.rgb ); break;
		case HSV:		converted.rgb = rgb_to_hsv( base_rgbval.rgb ); break;
		case HSL:		converted.rgb = rgb_to_hsl( base_rgbval.rgb ); break;
		case HCY:		converted.rgb = rgb_to_hcy( base_rgbval.rgb ); break;
		case YPBPR:		converted.rgb = rgb_ypbpr( base_rgbval.rgb ); break;
		case YPBPR601:	converted.rgb = rgb_ypbpr_bt601( base_rgbval.rgb ); break;
		case YCBCR1:	converted.rgb = rgb_to_ycbcr( base_rgbval.rgb ); break;
		case YCBCR2:	converted.rgb = rgb_ycbcr( base_rgbval.rgb ); break;
		case YCCBCCRC:	converted.rgb = rgb_yccbccrc( base_rgbval.rgb ); break;
		case YCOCG:		converted.rgb = rgb_ycocg( base_rgbval.rgb ); break;
		case BCH:		converted.rgb = rgb2BCH( base_rgbval.rgb ); break;
		case CHROMAMAX:	converted.rgba = rgb2cm( base_rgbval.rgb ); break;
		case OKLAB:		converted.rgb = oklab_from_linear_srgb( base_rgbval.rgb ); break;
		case OKHSL:		converted.rgb = srgb_to_okhsl( base_rgbval.rgb ); break;
		case OKHSV:		converted.rgb = srgb_to_okhsv( base_rgbval.rgb ); break;
		case OKLCH:		converted.rgb = srgb2oklch( base_rgbval.rgb ); break;
		case LCH:		converted.rgb = rgbToLch( base_rgbval.rgb ); break;
		case HSLUV:		converted.rgb = rgbToHsluv( base_rgbval.rgb ); break;
		case HPLUV:		converted.rgb = rgbToHpluv( base_rgbval.rgb ); break;
		case LUV:		converted.rgb = rgbToLuv( base_rgbval.rgb ); break;
		default:	break;
	}
	return converted;
}

// returns a uvec4 which is ready to be written as 8-bit RGBA
uvec4 convertBack ( vec4 value, int spaceswitch ) {
	uvec4 converted = uvec4( 0, 0, 0, 255 );
	switch ( spaceswitch ) {
		case RGB:		converted = uvec4(vec3( value * 255.0f ), 255 ); break;
		case SRGB:		converted.rgb = uvec3( srgb_to_rgb( value.rgb ) * 255 ); break;
		case XYZ:		converted.rgb = uvec3( xyz_to_rgb( value.rgb ) * 255 ); break;
		case XYY:		converted.rgb = uvec3( xyY_to_rgb( value.rgb ) * 255 ); break;
		case HSV:		converted.rgb = uvec3( hsv_to_rgb( value.rgb ) * 255 ); break;
		case HSL:		converted.rgb = uvec3( hsl_to_rgb( value.rgb ) * 255 ); break;
		case HCY:		converted.rgb = uvec3( hcy_to_rgb( value.rgb ) * 255 ); break;
		case YPBPR:		converted.rgb = uvec3( ypbpr_rgb( value.rgb ) * 255 ); break;
		case YPBPR601:	converted.rgb = uvec3( ypbpr_rgb_bt601( value.rgb ) * 255 ); break;
		case YCBCR1:	converted.rgb = uvec3( ycbcr_to_rgb( value.rgb ) * 255 ); break;
		case YCBCR2:	converted.rgb = uvec3( ycbcr_rgb( value.rgb ) * 255 ); break;
		case YCCBCCRC:	converted.rgb = uvec3( yccbccrc_rgb( value.rgb ) * 255 ); break;
		case YCOCG:		converted.rgb = uvec3( ycocg_rgb( value.rgb ) * 255 ); break;
		case BCH:		converted.rgb = uvec3( bch2RGB( value.rgb ) * 255 ); break;
		case CHROMAMAX:	converted.rgb = uvec3( cm2rgb( value.rgba ) * 255 ); break;
		case OKLAB:		converted.rgb = uvec3( linear_srgb_from_oklab( value.rgb ) * 255 ); break;
		case OKHSL:		converted.rgb = uvec3( okhsl_to_srgb( value.rgb ) * 255 ); break;
		case OKHSV:		converted.rgb = uvec3( okhsv_to_srgb( value.rgb ) * 255 ); break;
		case OKLCH:		converted.rgb = uvec3( oklch2srgb( value.rgb ) * 255 ); break;
		case LCH:		converted.rgb = uvec3( lchToRgb( value.rgb ) * 255 ); break;
		case HSLUV:		converted.rgb = uvec3( hsluvToRgb( value.rgb ) * 255 ); break;
		case HPLUV:		converted.rgb = uvec3( hpluvToRgb( value.rgb ) * 255 ); break;
		case LUV:		converted.rgb = uvec3( luvToRgb( value.rgb ) * 255 ); break;
		default: break;
	}
	return converted;
}
