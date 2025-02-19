//----------------------------------------------------------------------------

// YUV, generic conversion
// ranges: Y=0..1, U=-uvmax.x..uvmax.x, V=-uvmax.x..uvmax.x

vec3 yuv_rgb (vec3 yuv, vec2 wbwr, vec2 uvmax) {
    vec2 br = yuv.x + yuv.yz * (1.0 - wbwr) / uvmax;
	float g = (yuv.x - dot(wbwr, br)) / (1.0 - wbwr.x - wbwr.y);
	return vec3(br.y, g, br.x);
}

vec3 rgb_yuv (vec3 rgb, vec2 wbwr, vec2 uvmax) {
	float y = wbwr.y*rgb.r + (1.0 - wbwr.x - wbwr.y)*rgb.g + wbwr.x*rgb.b;
    return vec3(y, uvmax * (rgb.br - y) / (1.0 - wbwr));
}

//----------------------------------------------------------------------------

// YUV, HDTV, gamma compressed, ITU-R BT.709
// ranges: Y=0..1, U=-0.436..0.436, V=-0.615..0.615

vec3 yuv_rgb (vec3 yuv) {
    return yuv_rgb(yuv, vec2(0.0722, 0.2126), vec2(0.436, 0.615));
}

vec3 rgb_yuv (vec3 rgb) {
    return rgb_yuv(rgb, vec2(0.0722, 0.2126), vec2(0.436, 0.615));
}

//----------------------------------------------------------------------------

// Y*b*r, generic conversion
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

vec3 ypbpr_rgb (vec3 ybr, vec2 kbkr) {
    return yuv_rgb(ybr, kbkr, vec2(0.5));
}
    
vec3 rgb_ypbpr (vec3 rgb, vec2 kbkr) {
    return rgb_yuv(rgb, kbkr, vec2(0.5));
}

//----------------------------------------------------------------------------

// YPbPr, analog, gamma compressed, HDTV
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YPbPr to RGB, after ITU-R BT.709
vec3 ypbpr_rgb (vec3 ypbpr) {
    return ypbpr_rgb(ypbpr, vec2(0.0722, 0.2126));
}

// RGB to YPbPr, after ITU-R BT.709
vec3 rgb_ypbpr (vec3 rgb) {
    return rgb_ypbpr(rgb, vec2(0.0722, 0.2126));
}

//----------------------------------------------------------------------------

// YPbPr, analog, gamma compressed, VGA, TV
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YPbPr to RGB, after ITU-R BT.601
vec3 ypbpr_rgb_bt601 (vec3 ypbpr) {
    return ypbpr_rgb(ypbpr, vec2(0.114, 0.299));
}

// RGB to YPbPr, after ITU-R BT.601
vec3 rgb_ypbpr_bt601 (vec3 rgb) {
    return rgb_ypbpr(rgb, vec2(0.114, 0.299));
}

//----------------------------------------------------------------------------

// in the original implementation, the factors and offsets are
// ypbpr * (219, 224, 224) + (16, 128, 128)

// YPbPr to YCbCr (analog to digital)
vec3 ypbpr_ycbcr (vec3 ypbpr) {
	return ypbpr * vec3(0.85546875,0.875,0.875) + vec3(0.0625, 0.5, 0.5);
}

// YCbCr to YPbPr (digital to analog)
vec3 ycbcr_ypbpr (vec3 ycbcr) {
	return (ycbcr - vec3(0.0625, 0.5, 0.5)) / vec3(0.85546875,0.875,0.875);
}

//----------------------------------------------------------------------------

// YCbCr, digital, gamma compressed
// ranges: Y=0..1, b=0..1, r=0..1

// YCbCr to RGB (generic)
vec3 ycbcr_rgb(vec3 ycbcr, vec2 kbkr) {
    return ypbpr_rgb(ycbcr_ypbpr(ycbcr), kbkr);
}
// RGB to YCbCr (generic)
vec3 rgb_ycbcr(vec3 rgb, vec2 kbkr) {
    return ypbpr_ycbcr(rgb_ypbpr(rgb, kbkr));
}
// YCbCr to RGB
vec3 ycbcr_rgb(vec3 ycbcr) {
    return ypbpr_rgb(ycbcr_ypbpr(ycbcr));
}
// RGB to YCbCr
vec3 rgb_ycbcr(vec3 rgb) {
    return ypbpr_ycbcr(rgb_ypbpr(rgb));
}

//----------------------------------------------------------------------------

// ITU-R BT.2020:
// YcCbcCrc, linear
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YcCbcCrc to RGB
vec3 yccbccrc_rgb(vec3 yccbccrc) {
	return ypbpr_rgb(yccbccrc, vec2(0.0593, 0.2627));
}

// RGB to YcCbcCrc
vec3 rgb_yccbccrc(vec3 rgb) {
	return rgb_ypbpr(rgb, vec2(0.0593, 0.2627));
}

//----------------------------------------------------------------------------

// YCoCg
// ranges: Y=0..1, Co=-0.5..0.5, Cg=-0.5..0.5

vec3 ycocg_rgb (vec3 ycocg) {
    vec2 br = vec2(-ycocg.y,ycocg.y) - ycocg.z;
    return ycocg.x + vec3(br.y, ycocg.z, br.x);
}

vec3 rgb_ycocg (vec3 rgb) {
    float tmp = 0.5*(rgb.r + rgb.b);
    float y = rgb.g + tmp;
    float Cg = rgb.g - tmp;
    float Co = rgb.r - rgb.b;
    return vec3(y, Co, Cg) * 0.5;
}

//----------------------------------------------------------------------------

vec3 yccbccrc_norm(vec3 ypbpr) {
    vec3 p = yccbccrc_rgb(ypbpr);
   	vec3 ro = yccbccrc_rgb(vec3(ypbpr.x, 0.0, 0.0));
    vec3 rd = normalize(p - ro);
    vec3 m = 1./rd;
    vec3 b = 0.5*abs(m)-m*(ro - 0.5);
    float tF = min(min(b.x,b.y),b.z);
    p = ro + rd * tF * max(abs(ypbpr.y),abs(ypbpr.z)) * 2.0;
	return rgb_yccbccrc(p); 
}

vec3 ycocg_norm(vec3 ycocg) {
    vec3 p = ycocg_rgb(ycocg);
   	vec3 ro = ycocg_rgb(vec3(ycocg.x, 0.0, 0.0));
    vec3 rd = normalize(p - ro);
    vec3 m = 1./rd;
    vec3 b = 0.5*abs(m)-m*(ro - 0.5);
    float tF = min(min(b.x,b.y),b.z);
    p = ro + rd * tF * max(abs(ycocg.y),abs(ycocg.z)) * 2.0;
	return rgb_ycocg(p); 
}

//----------------------------------------------------------------------------