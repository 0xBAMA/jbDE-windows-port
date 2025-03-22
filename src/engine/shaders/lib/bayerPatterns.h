// this will need some refinement, value ranges are weird...

uint bayer2[] = {
	0, 128,
	192, 64
};

uint getBayer2 ( ivec2 loc ) {
	return bayer2[ ( loc.x % 2 ) + 2 * ( loc.y % 2 ) ];
}

uint bayer4[] = {
	0,  8,  2,  10,	/* values begin scaled to the range 0..15 */
	12, 4,  14, 6,	/* so they need to be rescaled by 16 */
	3,  11, 1,  9,
	15, 7,  13, 5
};

uint getBayer4 ( ivec2 loc ) {
	return 16 * bayer4[ ( loc.x % 4 ) + 4 * ( loc.y % 4 ) ];
}

uint bayer8[] = {
	0, 32,  8, 40,  2, 34, 10, 42,   /* 8x8 Bayer ordered dithering  */
	48, 16, 56, 24, 50, 18, 58, 26,  /* pattern. Each input pixel */
	12, 44,  4, 36, 14, 46,  6, 38,  /* starts scaled to the 0..63 range */
	60, 28, 52, 20, 62, 30, 54, 22,  /* before looking in this table */
	3, 35, 11, 43,  1, 33,  9, 41,   /* to determine the action. */
	51, 19, 59, 27, 49, 17, 57, 25,
	15, 47,  7, 39, 13, 45,  5, 37,
	63, 31, 55, 23, 61, 29, 53, 21
};

uint getBayer8 ( ivec2 loc ) {
	return 4 * bayer8[ ( loc.x % 8 ) + 8 * ( loc.y % 8 ) ];
}
