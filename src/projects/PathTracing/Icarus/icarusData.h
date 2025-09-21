struct icarusState_t {
	// access to the texture manager
	textureManager_t * textureManager;

	// shaders
	GLuint CopyShader;
	GLuint AdamShader;
	GLuint PrepShader;
	GLuint DrawShader;
	GLuint ClearShader;

	// first pass on the pipeline
	GLuint RayClearShader;
	GLuint RayGenerateShader;
	GLuint RayIntersectShader_SDF;
	GLuint RayIntersectShader_Triangle;
	GLuint RayIntersectShader_TinyBVH;
	GLuint RayIntersectShader_Volume;
	GLuint RayShadeShader;
	GLuint WipeShader;

	// BVH management
	SoftRast modelLoader;
	//tinybvh::BVH bvh;
	//tinybvh::bvhvec4 *vertices;
	GLuint bvhNodeBuffer;
	GLuint indexBuffer;
	GLuint vertexBuffer;

	// dimensions and related info
	// uvec2 dimensions = uvec2( 1280, 720 );
	uvec2 dimensions = uvec2( 1920, 1080 );
	int numLevels;

	// perf settings
	const int numIntersectors = 4; // I'd rather get this straight from the header...
	uint32_t maxBounces	= 16;
	bool runSDF			= false;
	bool runTriangle	= false;
	bool runBVH			= true;
	bool runVolume		= false;

	// how are we generating primary ray locations
	#define UNIFORM		0
	#define GAUSSIAN	1
	#define FOCUSED		2
	#define SHUFFLED	3
	#define SHUFFOCUS	4
	#define FLEXTILE	5

	int offsetFeedMode = FLEXTILE;
	GLuint offsetsSSBO;
	GLuint intersectionScratchSSBO;
	bool forceUpdate = false;

	struct flexTileConfig_t {
		bool dirty = true; // need to rebuild the list

		int tileSize = 64;
		int samplesPerPixel = 256;
		int samplesRemaining = 0;

		int currentTile = 0;

		enum tileOrder_t { tile_inOrder = 0, tile_shuffled = 1 };
		tileOrder_t tileOrder = tile_shuffled;

		enum fillMode_t { fill_inOrder = 0, fill_shuffled = 1, fill_bayer = 2 };
		fillMode_t fillMode = fill_bayer;
	} flexTileConfig;

	uint32_t numRays = 2 << 18;

	// holding the rays
	GLuint raySSBO;

	// camera parameterization
	vec3 viewerPosition = vec3( 0.0f );
	vec3 basisX = vec3( 1.0f, 0.0f, 0.0f );
	vec3 basisY = vec3( 0.0f, 1.0f, 0.0f );
	vec3 basisZ = vec3( 0.0f, 0.0f, 1.0f );
	float FoV = 0.618f;
	int cameraMode = 0;
	float uvScaleFactor = 1.0f;
	int DoFBokehMode = 0;
	float DoFRadius = 0.001f;
	float DoFFocusDistance = 5.0f;
	float chromabScaleFactor = 0.006f;
};

// resize the mesh to fit in a unit cube, centered at the origin
void expandBBox ( vec3 &mins, vec3 &maxs, vec3 p ) {
	mins.x = std::min( mins.x, p.x );
	maxs.x = std::max( maxs.x, p.x );

	mins.y = std::min( mins.y, p.y );
	maxs.y = std::max( maxs.y, p.y );

	mins.z = std::min( mins.z, p.z );
	maxs.z = std::max( maxs.z, p.z );
}

void UnitCubeRefit( vector< vec4 > &pointPositions ) {
	vec3 mins = vec3(  1e9f );
	vec3 maxs = vec3( -1e9f );

	// determine the size of the bounding box
	for ( size_t i = 0; i < pointPositions.size(); i++ ) {
		expandBBox( mins, maxs, pointPositions[ i ] );
	}

	// figure out scaling
	vec3 spans = maxs - mins;
	vec3 center = ( mins + maxs ) / 2.0f;
	float largestDimension = std::max( std::max( spans.x, spans.y ), spans.z ) * 0.51f;

	// run through again, butting the bbox min up against the origin + scaling appropriately to just fit inside -1..1
	for ( size_t i = 0; i < pointPositions.size(); i++ ) {
		pointPositions[ i ].xyz() = 5.0f * ( pointPositions[ i ].xyz() - center ) / largestDimension;
	}
}

//void LoadBVH_ply ( icarusState_t &state ) {
//
//	unscopedTimer timer;
//
//	timer.tick();
//
//	// happly::PLYData plyDataIn( "../ply/hodou.ply" );
//	// happly::PLYData plyDataIn( "../ply/nippon_wandering_tokyo_nights_001_10m_pts.ply" );
//	// happly::PLYData plyDataIn( "../ply/tree_orange_big21.ply" );
//	happly::PLYData plyDataIn( "../ply/grass1.ply" );
//	// happly::PLYData plyDataIn( "../ply/every5.ply" );
//
//	vector< double > xValues = plyDataIn.getElement( "vertex" ).getProperty< double >( "x" );
//	vector< double > yValues = plyDataIn.getElement( "vertex" ).getProperty< double >( "y" );
//	vector< double > zValues = plyDataIn.getElement( "vertex" ).getProperty< double >( "z" );
//
//	const size_t numPoints = xValues.size();
//
//	vector< uint8_t > rValues = plyDataIn.getElement( "vertex" ).getProperty< uint8_t >( "red" );
//	vector< uint8_t > gValues = plyDataIn.getElement( "vertex" ).getProperty< uint8_t >( "green" );
//	vector< uint8_t > bValues = plyDataIn.getElement( "vertex" ).getProperty< uint8_t >( "blue" );
//
//	// vector< uint8_t > rValues; rValues.resize( numPoints, 254 );
//	// vector< uint8_t > gValues; gValues.resize( numPoints, 254 );
//	// vector< uint8_t > bValues; bValues.resize( numPoints, 254 );
//
//	// processing positions to put them into a bounded range
//	vector< vec4 > plySpherePreProcess;
//	vector< vec3 > plySpherePreProcessColors;
//	plySpherePreProcess.reserve( numPoints );
//	plySpherePreProcessColors.reserve( numPoints );
//
//	rng reject = rng( 0.0f, 1.0f );
//	rng radiusGen = rng( 0.0003f, 0.001f );
//	for( size_t i = 0; i < numPoints; i++ ) {
//		// if ( reject() < 0.01f ) { // decimating
//			plySpherePreProcess.push_back( vec4( vec3( float( xValues[ i ] ), float( yValues[ i ] ), float( zValues[ i ] ) ).zxy(), radiusGen() ) );
//			plySpherePreProcessColors.push_back( vec3( float( rValues[ i ] ) / 255.0f, float( gValues[ i ] ) / 255.0f, float( bValues[ i ] ) / 255.0f ) );
//		// }
//	}
//	UnitCubeRefit( plySpherePreProcess );
//
//	timer.tock();
//	cout << "ply loading done in " << timer.timeCPU << "ms" << newline;
//
//	timer.tick();
//
//	const int numTriangles = numPoints;
//
//	// allocate space for that many verts
//	state.vertices = ( tinybvh::bvhvec4 * ) malloc( 3 * numTriangles * sizeof( tinybvh::bvhvec4 ) );
//
//	// copy from the loader to the bvh's list
//	// const float radius = 0.0008f;
//	// const float radius = 0.0008f;
//	for ( size_t i = 0; i < numPoints; i++ ) {
//		const int baseIdx = 3 * i;
//
//		// add a triangle to the BVH that will generate the same bounding box as the sphere
//		const float radius = plySpherePreProcess[ i ].w;
//		state.vertices[ baseIdx + 0 ].x = plySpherePreProcess[ i ].x + radius;
//		state.vertices[ baseIdx + 0 ].y = plySpherePreProcess[ i ].y;
//		state.vertices[ baseIdx + 0 ].z = plySpherePreProcess[ i ].z + radius;
//
//		state.vertices[ baseIdx + 1 ].x = plySpherePreProcess[ i ].x - radius;
//		state.vertices[ baseIdx + 1 ].y = plySpherePreProcess[ i ].y + radius;
//		state.vertices[ baseIdx + 1 ].z = plySpherePreProcess[ i ].z;
//
//		state.vertices[ baseIdx + 2 ].x = plySpherePreProcess[ i ].x;
//		state.vertices[ baseIdx + 2 ].y = plySpherePreProcess[ i ].y - radius;
//		state.vertices[ baseIdx + 2 ].z = plySpherePreProcess[ i ].z - radius;
//	}
//
//	// build the bvh from the list of triangles
//	state.bvh.Build( state.vertices, numTriangles );
//	state.bvh.Convert( tinybvh::BVH::WALD_32BYTE, tinybvh::BVH::VERBOSE );
//	state.bvh.Refit( tinybvh::BVH::VERBOSE );
//
//	state.bvh.Convert( tinybvh::BVH::WALD_32BYTE, tinybvh::BVH::AILA_LAINE );
//
//	// then have to build the buffer that's going to give us sphere positions + info ( radius, color, etc )
//	vector< vec4 > plySphereData; // xyz, w is radius... then rgb, w is material id
//	plySphereData.reserve( 2 * numPoints );
//	for ( size_t i = 0; i < numPoints; i++ ) {
//		plySphereData.push_back( plySpherePreProcess[ i ] );
//		plySphereData.push_back( vec4( plySpherePreProcessColors[ i ], 0.0f ) );
//	}
//
//	timer.tock();
//	cout << "BVH build done in " << timer.timeCPU << "ms" << newline;
//
//
//	// // testing rays against it
//	// Image_4U test( 5000, 5000 );
//	// for ( uint32_t y = 0; y < test.Height(); y++ ) {
//	// 	for ( uint32_t x = 0; x < test.Width(); x++ ) {
//	// 		color_4U col;
//
//	// 		// tinybvh::bvhvec3 O( RangeRemap( x + 0.5f, 0.0f, test.Width(), -1.0f, 1.0f ), RangeRemap( y + 0.5f, 0.0f, test.Height(), -1.0f, 1.0f ), 1.0f );
//	// 		tinybvh::bvhvec3 O( 1.0f, RangeRemap( y + 0.5f, 0.0f, test.Width(), -1.0f, 1.0f ), RangeRemap( x + 0.5f, 0.0f, test.Height(), -1.0f, 1.0f ) );
//	// 		tinybvh::bvhvec3 D( -1.0f, 0.0f, 0.0f );
//	// 		tinybvh::Ray ray( O, D );
//	// 		uint steps = uint( state.bvh.Intersect( ray ) );
//	// 		printf( "%d %d: nearest intersection: %f (found in %i traversal steps).\n", x, y, ray.hit.t, steps );
//
//	// 		// if ( ray.hit.t < 1e5f ) {
//	// 		// 	// cout << "hit" << endl;
//	// 		// 	col = color_4U( { ( uint8_t ) ( 255 * ( ray.hit.t / 2.0f ) ), ( uint8_t ) steps * 3u, ( uint8_t ) 0, ( uint8_t ) 255 } );
//	// 		// } else {
//	// 		// 	col = color_4U( { ( uint8_t ) 16, ( uint8_t ) steps * 3u, ( uint8_t ) 0, ( uint8_t ) 255 } );
//	// 		// }
//
//	// 		// col = color_4U( { ( uint8_t ) ( ( ray.hit.t < 1e5f ) ? ( 255 * ( ray.hit.t / 2.0f ) ) : steps * 3u ), ( uint8_t ) steps * 3u, ( uint8_t ) 0, 255 } );
//
//	// 		float stepTerm = ( steps * 3.f ) / 255.0f;
//	// 		vec3 color = mix( plySphereData[ 2 * ray.hit.prim + 1 ].rgb(), vec3( stepTerm ), vec3( 0.5f ) ) * 255.0f;
//	// 		col = color_4U( { ( uint8_t ) color.r, ( uint8_t ) color.g, ( uint8_t ) color.b, 255 } );
//	// 		test.SetAtXY( x, y, col );
//	// 	}
//	// }
//
//	// test.Save( "test.png" ); 
//
//	// BVH Nodes buffer
//	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.bvhNodeBuffer );
//	glBufferData( GL_SHADER_STORAGE_BUFFER, state.bvh.usedAltNodes * sizeof( tinybvh::BVH::BVHNodeAlt ), ( GLvoid * ) state.bvh.altNode, GL_DYNAMIC_COPY );
//	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, state.bvhNodeBuffer );
//	glObjectLabel( GL_BUFFER, state.bvhNodeBuffer, -1, string( "BVH Nodes" ).c_str() );
//
//	// BVH vertex indices buffer
//	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.indexBuffer );
//	glBufferData( GL_SHADER_STORAGE_BUFFER, state.bvh.idxCount * sizeof( unsigned ), ( GLvoid * ) state.bvh.triIdx, GL_DYNAMIC_COPY );
//	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, state.indexBuffer );
//	glObjectLabel( GL_BUFFER, state.indexBuffer, -1, string( "BVH Vertex Indices" ).c_str() );
//
//	// specific geometry data buffer
//	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.vertexBuffer );
//	glBufferData( GL_SHADER_STORAGE_BUFFER, state.bvh.triCount * 2 * sizeof( tinybvh::bvhvec4 ), ( GLvoid * ) plySphereData.data(), GL_DYNAMIC_COPY );
//	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 5, state.vertexBuffer );
//	glObjectLabel( GL_BUFFER, state.vertexBuffer, -1, string( "BVH Geometry Data" ).c_str() );
//}

//void LoadBVH ( icarusState_t &state ) {
//
//// this works, loads 200k triangles - stanford tyranosaurus - good test model for the BVH stuff
//	// state.modelLoader.LoadModel( "../tyra.obj", "../" );
//	// state.modelLoader.LoadModel( "../f-16/f-16.obj", "../f-16" );
//	// state.modelLoader.LoadModel( "../holy-imperial-battleship-with-pkmc/source/BB_CA18_White_19_25_1.obj", "../holy-imperial-battleship-with-pkmc/textures/" );
//	// state.modelLoader.LoadModel( "../hall.obj", "../" );
//	// state.modelLoader.LoadModel( "../Pipework/Pipes.obj", "../" );
//	state.modelLoader.LoadModel( "../city.obj", "../" );
//	// state.modelLoader.LoadModel( "../lens2high.obj", "../" );
//	// state.modelLoader.LoadModel( "../corner.obj", "../" );
//	// state.modelLoader.LoadModel( "../hangarCity.obj", "../" );
//	// state.modelLoader.LoadModel( "../building-scan-no-1-interior/source/batiment1/batiment1_lowpoly.obj", "../" );
//	// state.modelLoader.LoadModel( "../wwii-battery-mont-canisy-normandie/source/3DModel/3DModel.obj", "../" );
//	// state.modelLoader.LoadModel( "../destroyed-warehouse-in-kaarina-finland/source/Destroyed_warehouse_in_Kaarina_Finland_SF/Destroyed_warehouse_in_Kaarina_Finland_SF.obj", "../" );
//
//	cout << endl << "Model has " << state.modelLoader.triangles.size() << " tris" << endl;
//
//	// put it in some known span
//	state.modelLoader.UnitCubeRefit();
//
//	// const int numRandomTriangles = 500000;
//	const int numRandomTriangles = 0;
//	const int numTriangles = state.modelLoader.triangles.size() + numRandomTriangles;
//
//	// allocate space for that many verts
//	state.vertices = ( tinybvh::bvhvec4 * ) malloc( 3 * numTriangles * sizeof( tinybvh::bvhvec4 ) );
//
//	// copy from the loader to the bvh's list
//	for ( size_t i = 0; i < state.modelLoader.triangles.size(); i++ ) {
//		const int baseIdx = 3 * i;
//
//		// add the triangles to the BVH
//		state.vertices[ baseIdx + 0 ].x = state.modelLoader.triangles[ i ].p0.x;
//		state.vertices[ baseIdx + 0 ].y = state.modelLoader.triangles[ i ].p0.y;
//		state.vertices[ baseIdx + 0 ].z = state.modelLoader.triangles[ i ].p0.z;
//
//		state.vertices[ baseIdx + 1 ].x = state.modelLoader.triangles[ i ].p1.x;
//		state.vertices[ baseIdx + 1 ].y = state.modelLoader.triangles[ i ].p1.y;
//		state.vertices[ baseIdx + 1 ].z = state.modelLoader.triangles[ i ].p1.z;
//
//		state.vertices[ baseIdx + 2 ].x = state.modelLoader.triangles[ i ].p2.x;
//		state.vertices[ baseIdx + 2 ].y = state.modelLoader.triangles[ i ].p2.y;
//		state.vertices[ baseIdx + 2 ].z = state.modelLoader.triangles[ i ].p2.z;
//
//		// pass color in .a
//		state.vertices[ baseIdx + 0 ].w = state.modelLoader.triangles[ i ].c0.r;
//		state.vertices[ baseIdx + 1 ].w = state.modelLoader.triangles[ i ].c0.g;
//		state.vertices[ baseIdx + 2 ].w = state.modelLoader.triangles[ i ].c0.b;
//	}
//
//	// const int baseOffset = state.modelLoader.triangles.size();
//	// rngN radiusDistribution = rngN( 4.0f, 0.3f );
//	// rng spanDistribution = rng( 0.0f, 2.0f * pi );
//	// rng colorDistribution = rng( 0.0f, 1.0f );
//	// rngN fineDistribution = rngN( 0.3f, 0.1f );
//
//	// int baseIdx = baseOffset;
//
//	// for ( int ring = 0; ring < 10; ring++ ){
//	// 	palette::PickRandomPalette( true );
//	// 	for ( int i = 0; i < numRandomTriangles / 10.0f; i++ ) {
//
//	// 		const float radius = radiusDistribution();
//	// 		const float span = spanDistribution();
//	// 		vec3 basePoint = vec3( radius * cos( span ), radius * sin( span ), colorDistribution() + 2.0f * ring );
//
//	// 		// add the triangles to the BVH
//	// 		state.vertices[ baseIdx + 0 ].x = basePoint.x + fineDistribution();
//	// 		state.vertices[ baseIdx + 0 ].y = basePoint.y + fineDistribution();
//	// 		state.vertices[ baseIdx + 0 ].z = basePoint.z + fineDistribution();
//
//	// 		state.vertices[ baseIdx + 1 ].x = basePoint.x + fineDistribution();
//	// 		state.vertices[ baseIdx + 1 ].y = basePoint.y + fineDistribution();
//	// 		state.vertices[ baseIdx + 1 ].z = basePoint.z + fineDistribution();
//
//	// 		state.vertices[ baseIdx + 2 ].x = basePoint.x + fineDistribution();
//	// 		state.vertices[ baseIdx + 2 ].y = basePoint.y + fineDistribution();
//	// 		state.vertices[ baseIdx + 2 ].z = basePoint.z + fineDistribution();
//
//	// 		// pass color in .a
//	// 		// const vec3 color = vec3( colorDistribution(), colorDistribution(), colorDistribution() );
//	// 		const vec3 color =  palette::paletteRef( colorDistribution() );
//	// 		state.vertices[ baseIdx + 0 ].w = color.r;
//	// 		state.vertices[ baseIdx + 1 ].w = color.g;
//	// 		state.vertices[ baseIdx + 2 ].w = color.b;
//
//	// 		baseIdx += 3;
//	// 	}
//	// }
//
//	// build the bvh from the list of triangles
//	state.bvh.Build( state.vertices, numTriangles );
//	state.bvh.Convert( tinybvh::BVH::WALD_32BYTE, tinybvh::BVH::VERBOSE );
//	state.bvh.Refit( tinybvh::BVH::VERBOSE );
//
//	// // testing rays against it
//	// Image_4U test( 5000, 5000 );
//	// for ( uint32_t y = 0; y < test.Height(); y++ ) {
//	// 	for ( uint32_t x = 0; x < test.Width(); x++ ) {
//	// 		color_4U col;
//
//	// 		// tinybvh::bvhvec3 O( RangeRemap( x + 0.5f, 0.0f, test.Width(), -1.0f, 1.0f ), RangeRemap( y + 0.5f, 0.0f, test.Height(), -1.0f, 1.0f ), 1.0f );
//	// 		tinybvh::bvhvec3 O( 1.0f, RangeRemap( y + 0.5f, 0.0f, test.Width(), -1.0f, 1.0f ), RangeRemap( x + 0.5f, 0.0f, test.Height(), -1.0f, 1.0f ) );
//	// 		tinybvh::bvhvec3 D( -1.0f, 0.0f, 0.0f );
//	// 		tinybvh::Ray ray( O, D );
//	// 		uint steps = uint( state.bvh.Intersect( ray ) );
//	// 		// printf( "%d %d: nearest intersection: %f (found in %i traversal steps).\n", x, y, ray.hit.t, steps );
//
//	// 		// if ( ray.hit.t < 1e5f ) {
//	// 		// 	// cout << "hit" << endl;
//	// 		// 	col = color_4U( { ( uint8_t ) ( 255 * ( ray.hit.t / 2.0f ) ), ( uint8_t ) steps * .f;, ( uint8_t ) 0, ( uint8_t ) 255 } );
//	// 		// } else {
//	// 		// 	col = color_4U( { ( uint8_t ) 16, ( uint8_t ) steps * 3u, ( uint8_t ) 0, ( uint8_t ) 255 } );
//	// 		// }
//	// 		col = color_4U( { ( uint8_t ) ( ( ray.hit.t < 1e5f ) ? ( 255 * ( ray.hit.t / 2.f ) ) : steps * 3u ), ( uint8_t ) steps * 3u, ( uint8_t ) 0, 255 } );
//	// 		test.SetAtXY( x, y, col );
//	// 	}
//	// }
//
//	// test.Save( "test.png" );
//
//	state.bvh.Convert( tinybvh::BVH::WALD_32BYTE, tinybvh::BVH::AILA_LAINE );
//	// // create OpenCL buffers for the BVH data calculated by tiny_state.bvh.h
//	// tinyocl::Buffer gpuNodes( state.bvh.usedAltNodes * sizeof( BVH::BVHNodeAlt ), state.bvh.altNode );
//	// tinyocl::Buffer idxData( state.bvh.idxCount * sizeof( unsigned ), state.bvh.triIdx );
//	// tinyocl::Buffer triData( state.bvh.triCount * 3 * sizeof( tinybvh::bvhvec4 ), triangles );
//
//	// BVH Nodes buffer
//	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.bvhNodeBuffer );
//	glBufferData( GL_SHADER_STORAGE_BUFFER, state.bvh.usedAltNodes * sizeof( tinybvh::BVH::BVHNodeAlt ), ( GLvoid * ) state.bvh.altNode, GL_DYNAMIC_COPY );
//	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, state.bvhNodeBuffer );
//	glObjectLabel( GL_BUFFER, state.bvhNodeBuffer, -1, string( "BVH Nodes" ).c_str() );
//
//	// BVH vertex indices buffer
//	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.indexBuffer );
//	glBufferData( GL_SHADER_STORAGE_BUFFER, state.bvh.idxCount * sizeof( unsigned ), ( GLvoid * ) state.bvh.triIdx, GL_DYNAMIC_COPY );
//	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, state.indexBuffer );
//	glObjectLabel( GL_BUFFER, state.indexBuffer, -1, string( "BVH Vertex Indices" ).c_str() );
//
//	// triangle data
//	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.vertexBuffer );
//	glBufferData( GL_SHADER_STORAGE_BUFFER, state.bvh.triCount * 3 * sizeof( tinybvh::bvhvec4 ), ( GLvoid * ) state.vertices, GL_DYNAMIC_COPY );
//	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 5, state.vertexBuffer );
//	glObjectLabel( GL_BUFFER, state.vertexBuffer, -1, string( "BVH Vertex Data" ).c_str() );
//}

void LoadVoraldoModel ( icarusState_t &state ) {
	Image_4U loadedImage;
	loadedImage.Load( "tree.png" );

	textureOptions_t opts;
	opts.width			= 256;
	opts.height			= 256;
	opts.depth			= 256;
	opts.dataType		= GL_RGBA8UI;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_3D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	opts.initialData	= ( void * ) loadedImage.GetImageDataBasePtr();

	state.textureManager->Add( "Voraldo Model", opts );
}

// =============================================================================================================
// Initialization
void CompileShaders ( icarusState_t &state ) {

	const string basePath = string( "../src/projects/PathTracing/Icarus/shaders/" );

	state.CopyShader 			= computeShader( basePath + "copy.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.CopyShader, -1, string( "Copy" ).c_str() );

	state.AdamShader			= computeShader( basePath + "adam.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.AdamShader, -1, string( "Adam" ).c_str() );

	state.PrepShader			= computeShader( basePath + "prep.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.PrepShader, -1, string( "Prep" ).c_str() );

	state.DrawShader			= computeShader( basePath + "draw.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.DrawShader, -1, string( "Draw" ).c_str() );

	state.ClearShader			= computeShader( basePath + "clear.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.ClearShader, -1, string( "Clear" ).c_str() );


	// ray generation, intersection, and shading
	state.RayClearShader				= computeShader( basePath + "rayClear.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayClearShader, -1, string( "Ray Clear" ).c_str() );

	state.RayGenerateShader				= computeShader( basePath + "rayGenerate.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayGenerateShader, -1, string( "Ray Generate" ).c_str() );

	state.RayIntersectShader_SDF		= computeShader( basePath + "rayIntersect_SDF.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayIntersectShader_SDF, -1, string( "Ray Intersect (SDF)" ).c_str() );

	state.RayIntersectShader_Triangle	= computeShader( basePath + "rayIntersect_Triangle.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayIntersectShader_Triangle, -1, string( "Ray Intersect (Triangle)" ).c_str() );

	state.RayIntersectShader_TinyBVH	= computeShader( basePath + "rayIntersect_TinyBVH.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayIntersectShader_TinyBVH, -1, string( "Ray Intersect (TinyBVH)" ).c_str() );

	state.RayIntersectShader_Volume		= computeShader( basePath + "rayIntersect_Volume.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayIntersectShader_Volume, -1, string( "Ray Intersect (Volume)" ).c_str() );

	state.RayShadeShader				= computeShader( basePath + "rayShade.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayShadeShader, -1, string( "Ray Shade/Bounce" ).c_str() );

}

void AllocateBuffers ( icarusState_t &state ) {
	// allocate the ray buffer
	glGenBuffers( 1, &state.offsetsSSBO );		// pixel offsets
	glGenBuffers( 1, &state.raySSBO );		// ray state front and back buffer(s)
	glGenBuffers( 1, &state.intersectionScratchSSBO ); // intersection structs
	glGenBuffers( 1, &state.bvhNodeBuffer );
	glGenBuffers( 1, &state.indexBuffer );
	glGenBuffers( 1, &state.vertexBuffer );

	// allocate space for ray offsets - 2x ints * state.numRays offsets per frame
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.offsetsSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 2 * sizeof( GLuint ) * state.numRays, NULL, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, state.offsetsSSBO );
	glObjectLabel( GL_BUFFER, state.offsetsSSBO, -1, string( "Pixel Offsets" ).c_str() );

	// allocate space for the ray state structs, state.numRays of them - this is going to change a lot, as I figure out what needs to happen
		// currently 64 bytes, see rayState.h.glsl
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.raySSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 64 * state.numRays, NULL, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, state.raySSBO );
	glObjectLabel( GL_BUFFER, state.raySSBO, -1, string( "Ray State Buffer" ).c_str() );

	// scratch memory for the ray state structs
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.intersectionScratchSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 64 * state.numIntersectors * state.numRays, NULL, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, state.intersectionScratchSSBO );
	glObjectLabel( GL_BUFFER, state.intersectionScratchSSBO, -1, string( "Intersection Buffer" ).c_str() );

	// buffers for the BVH
	// LoadBVH( state );
	// LoadBVH_ply( state );

	// setup for the Voraldo model in the volume renderer
	// LoadVoraldoModel( state );
}

void AllocateTextures ( icarusState_t &state ) {
	static bool firstTime = true;
	if ( !firstTime ) {
		// todo: delete the existing textures, first
		state.textureManager->Remove( "Output Buffer" );
		state.textureManager->Remove( "R Tally Image" );
		state.textureManager->Remove( "G Tally Image" );
		state.textureManager->Remove( "B Tally Image" );
		state.textureManager->Remove( "Sample Count" );
		state.textureManager->Remove( "Adam" );
	}
	firstTime = false;

	state.flexTileConfig.dirty = true;

	textureOptions_t opts;

	// image to hold tonemapped results
	opts.width			= state.dimensions.x;
	opts.height			= state.dimensions.y;
	opts.dataType		= GL_RGBA32F;
	opts.minFilter		= GL_LINEAR;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	state.textureManager->Add( "Output Buffer", opts );

	// atomic tally images, for the renderer to write to
	opts.dataType		= GL_R32UI;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	state.textureManager->Add( "R Tally Image", opts );
	state.textureManager->Add( "G Tally Image", opts );
	state.textureManager->Add( "B Tally Image", opts );
	state.textureManager->Add( "Sample Count", opts );

	// Adam's buffers
	uint32_t adamBufferSize = nextPowerOfTwo( std::max( state.dimensions.x, state.dimensions.y ) );
	// cout << "adam says... " << adamBufferSize << endl;
	int w = adamBufferSize;
	int h = adamBufferSize;

	// creating a square image
	opts = textureOptions_t();
	opts.textureType = GL_TEXTURE_2D;
	opts.width = w;
	opts.height = h;
	opts.dataType = GL_RGBA32F; // type tbd... may need to be ints, for atomic operations? Not sure what that's going to look like, in practical terms
	state.textureManager->Add( "Adam", opts );

	Image_4U zeroesU( w, h );
	Image_4F zeroesF( w, h );

	// creating the mip chain
	int level = 0;
	while ( h >= 1 ) {
		h /= 2; w /= 2; level++;

		glBindTexture( GL_TEXTURE_2D, state.textureManager->Get( "Adam" ) );
		glTexImage2D( GL_TEXTURE_2D, level, GL_RGBA32F, w, h, 0, getFormat( GL_RGBA32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );
	}

	// set the number of levels that are in the texture
	state.numLevels = level;
}

ivec2 GetNextOffset ( icarusState_t &state ) {
	ivec2 offset;
	switch ( state.offsetFeedMode ) {
		case UNIFORM: {
			// uniform random
			static rng offsetGen = rng( 0.0f, 1.0f );
			offset = ivec2( offsetGen() * state.dimensions.x, offsetGen() * state.dimensions.y );
			break;
		}

		case GAUSSIAN: {
			// center-biased random, creates a foveated look
				// probably easiest to rejection sample for off-screen offsets
			static rngN offsetGen = rngN( 0.5f, 0.15f );
			do {
				offset = ivec2( offsetGen() * state.dimensions.x, offsetGen() * state.dimensions.y );
			} while ( offset.x >= state.dimensions.x || offset.x < 0 || offset.y >= state.dimensions.y || offset.y < 0 );
			break;
		}

		case FOCUSED: {
			const int updateRate = 8;
			static int count = state.numRays;
			static ivec2 loc = state.dimensions / 2u;
			// static ivec2 loc = ivec2( 0 );
			if ( !( count-- ) ) {
				// set a new sample location
				count = state.numRays / updateRate;
	
				// uniformly distributed update
				static rng offsetGen = rng( 0.0f, 1.0f );
				loc = ivec2( offsetGen() * state.dimensions.x, offsetGen() * state.dimensions.y );
	
				// // gaussian distribution
				// static rngN offsetGen = rngN( 0.5f, 0.2f );
				// do {
				// 	loc = ivec2( offsetGen() * state.dimensions.x, offsetGen() * state.dimensions.y );
				// } while ( loc.x >= state.dimensions.x || loc.x < 0 || loc.y >= state.dimensions.y || loc.y < 0 );
			}
			offset = loc;
			break;
		}

		case SHUFFLED: {
			// list that ensures you will touch every pixel before repeating
			static vector< ivec2 > offsets;
			// generate new list of offsets
			if ( offsets.size() == 0 || state.forceUpdate ) {
				offsets.clear();
				state.forceUpdate = false;

				for ( uint32_t x = 0; x < state.dimensions.x; x++ ) {
					for ( uint32_t y = 0; y < state.dimensions.y; y++ ) {
						offsets.push_back( ivec2( x, y ) );
					}
				}
				// shuffle it around a bit
				static auto rng = std::default_random_engine {};
				std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
				std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
				std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
			}

			// pop one off the list, return that
			// offset = offsets[ offsets.size() - 1 ];
			// offsets.pop_back();

			// just go through the list over and over
			static size_t idx = 0;
			idx = ( idx + 1 ) % offsets.size();
			offset = offsets[ idx ];

			break;
		}

		case SHUFFOCUS: {
			// list that ensures you will touch every pixel before repeating
			static vector< ivec2 > offsets;

			// generate new list of offsets
			if ( offsets.size() == 0 || state.forceUpdate ) {
				offsets.clear();
				state.forceUpdate = false;

				const ivec2 blockSize = ivec2( 64, 64 );
				for ( uint32_t y = 0; y < state.dimensions.y; y += blockSize.y ) {
				for ( uint32_t x = 0; x < state.dimensions.x; x += blockSize.x ) {
					vector< ivec2 > blockOffsets; // shuffle within 64x64 blocks
					for ( uint32_t yI = 0; yI < blockSize.y; yI++ ) {
					for ( uint32_t xI = 0; xI < blockSize.x; xI++ ) {
						blockOffsets.push_back( ivec2( xI, yI ) );
					}}
					static auto rng = std::default_random_engine {};
					std::shuffle( std::begin( blockOffsets ), std::end( blockOffsets ), rng );
					for ( auto& o : blockOffsets ) {
						offsets.push_back( ivec2( x, y ) + o );
					}
					blockOffsets.clear();
				}}
			}

			const int updateRate = 2 << 2;
			static int count = state.numRays;
			static ivec2 loc = state.dimensions / 2u;
			if ( !( count ) ) {
				// set a new sample location
				count = state.numRays / updateRate;

				// just go through the list over and over
				static size_t idx = 0;
				idx = ( idx + 1 ) % offsets.size();
				loc = offsets[ idx ];
			}
			offset = loc;
			count--;
			break;
		}

		case FLEXTILE: {
			static vector< ivec2 > offsets;

			if ( state.flexTileConfig.dirty ) { // rebuild the list
				offsets.clear();
				state.flexTileConfig.currentTile = 0;
				state.flexTileConfig.dirty = false;

				vector< ivec2 > baseTileOffsets;
				for ( int y = 0; y < state.dimensions.y; y += state.flexTileConfig.tileSize ) {
					for ( int x = 0; x < state.dimensions.x; x += state.flexTileConfig.tileSize ) {
						baseTileOffsets.push_back( ivec2( x, y ) );
					}
				}

				if ( state.flexTileConfig.tileOrder == icarusState_t::flexTileConfig_t::tileOrder_t::tile_shuffled ) {
					static auto rng = std::default_random_engine {};
					std::shuffle( std::begin( baseTileOffsets ), std::end( baseTileOffsets ), rng );
					std::shuffle( std::begin( baseTileOffsets ), std::end( baseTileOffsets ), rng );
				}

				// now constructing the actual list of offsets, from that base list of tiles
					// todo: add bounds checking
				for ( auto& tile : baseTileOffsets ) {
					switch ( state.flexTileConfig.fillMode ) {
						case icarusState_t::flexTileConfig_t::fillMode_t::fill_inOrder: {
							for ( int y = 0; y < state.flexTileConfig.tileSize; y++ ) {
								for ( int x = 0; x < state.flexTileConfig.tileSize; x++ ) {
									// add the offsets to the list
									ivec2 p = tile + ivec2( x, y );
									if ( p.x >= 0 && p.y >= 0 && p.x < state.dimensions.x && p.y < state.dimensions.y ) {
										offsets.push_back( p );
									}
								}
							}
							break;
						}

						case icarusState_t::flexTileConfig_t::fillMode_t::fill_shuffled: {
							// add the offsets to the list, then shuffle
							vector < ivec2 > tileOffsets;
							for ( int y = 0; y < state.flexTileConfig.tileSize; y++ ) {
								for ( int x = 0; x < state.flexTileConfig.tileSize; x++ ) {
									ivec2 p = tile + ivec2( x, y );
									if ( p.x >= 0 && p.y >= 0 && p.x < state.dimensions.x && p.y < state.dimensions.y ) {
										offsets.push_back( p );
									}
								}
							}
							static auto rng = std::default_random_engine {};
							std::shuffle( std::begin( tileOffsets ), std::end( tileOffsets ), rng );
							std::shuffle( std::begin( tileOffsets ), std::end( tileOffsets ), rng );
							for ( auto& tileOffset : tileOffsets ) {
								offsets.push_back( tileOffset );
							}
							break;
						}

						case icarusState_t::flexTileConfig_t::fillMode_t::fill_bayer: {
							// add the offsets to the list
							vector < uint8_t > bayerData = BayerData( 8 ); // might break for tiles < 8, idk
							for ( uint8_t i = 0; i < 64; i++ ) {
								for ( int y = 0; y < state.flexTileConfig.tileSize; y++ ) {
									for ( int x = 0; x < state.flexTileConfig.tileSize; x++ ) {
										if ( bayerData[ ( x % 8 ) + 8 * ( y % 8 ) ] == i ) { // marking this as the next offset
											ivec2 p = tile + ivec2( x, y );
											if ( p.x >= 0 && p.y >= 0 && p.x < state.dimensions.x && p.y < state.dimensions.y ) {
												offsets.push_back( tile + ivec2( x, y ) );
											}
										}
									}
								}
							}
							break;
						}
					}
				}
			}

			static ivec2 loc = ivec2( 0 );
			if ( state.flexTileConfig.samplesRemaining == 0 ) {
				// get a new offset from the list, increment the offset for next time
				state.flexTileConfig.samplesRemaining = state.flexTileConfig.samplesPerPixel;
				loc = offsets[ state.flexTileConfig.currentTile ];
				state.flexTileConfig.currentTile++;
				state.flexTileConfig.currentTile %= offsets.size();
				// cout << "new offset set " << loc.x << " " << loc.y << endl;
			}

			// give the next offset off the list
			offset = loc;
			state.flexTileConfig.samplesRemaining--;
			break;
		}

		default: {
			offset = ivec2( 0 );
			break;
		}
	}

	return offset;
}

void AdamUpdate ( icarusState_t &state ) {

	// do the initial averaging, into Adam mip 0
	GLuint shader = state.CopyShader;
	glUseProgram( shader );

	glUniform2i( glGetUniformLocation( shader, "dims" ), state.dimensions.x, state.dimensions.y );
	
	state.textureManager->BindImageForShader( "R Tally Image", "rTally", shader, 0 );
	state.textureManager->BindImageForShader( "G Tally Image", "gTally", shader, 1 );
	state.textureManager->BindImageForShader( "B Tally Image", "bTally", shader, 2 );
	state.textureManager->BindImageForShader( "Sample Count", "count", shader, 3 );
	state.textureManager->BindImageForShader( "Adam", "adam", shader, 4 );

	glDispatchCompute( ( state.dimensions.x + 15 ) / 16, ( state.dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

	// the propagate that data back up through the mips
	shader = state.AdamShader;
	glUseProgram( shader );

	// this seems wrong...
	int w = ( 1 << state.numLevels ) / 4;
	int h = ( 1 << state.numLevels ) / 4;

	for ( int n = 0; n < state.numLevels - 1; n++ ) { // for num mips minus 1

		glUniform2i( glGetUniformLocation( shader, "dims" ), 2 * w, 2 * h );

		// bind the appropriate levels for N and N+1 (starting with N=0... to N=...? )
		state.textureManager->BindImageForShader( "Adam", "adamN", shader, 0, n );
		state.textureManager->BindImageForShader( "Adam", "adamNPlusOne", shader, 1, n + 1 );

		// this seems to be the most effective groupsize, got it down from ~4.7ms to ~0.7ms
		glDispatchCompute( ( w + 15 ) / 16, ( h + 15 ) / 16, 1 ); w /= 2; h /= 2;
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
}

void PostProcess ( icarusState_t &state ) {
	const GLuint shader = state.PrepShader;
	glUseProgram( shader );

	// tbd... prepping an output image from the Adam representation
		// currently, just a passthrough, but will do the tonemapping and etc

	// output buffer holding the prepped color
	state.textureManager->BindImageForShader( "Output Buffer", "outputBuffer", shader, 0 );

	// Adam buffers
	state.textureManager->BindTexForShader( "Adam", "adam", shader, 1 );

	glDispatchCompute( ( state.dimensions.x + 15 ) / 16, ( state.dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void DrawViewer ( icarusState_t &state, viewerState_t &viewerState ) {
	const GLuint shader = state.DrawShader;
	glUseProgram( shader );

	// use this for some time varying seeding type thing, maybe
	glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
	glUniform2f( glGetUniformLocation( shader, "offset" ), viewerState.offset.x, viewerState.offset.y );
	glUniform1f( glGetUniformLocation( shader, "scale" ), viewerState.scale );

	state.textureManager->BindImageForShader( "Accumulator", "accumulator", shader, 0 );
	state.textureManager->BindTexForShader( "Output Buffer", "outputBuffer", shader, 1 );

	glDispatchCompute( ( viewerState.viewerSize.x + 15 ) / 16, ( viewerState.viewerSize.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void ClearAccumulators ( icarusState_t &state ) {
	const GLuint shader = state.ClearShader;
	glUseProgram( shader );

	state.textureManager->BindImageForShader( "R Tally Image", "rTally", shader, 0 );
	state.textureManager->BindImageForShader( "G Tally Image", "gTally", shader, 1 );
	state.textureManager->BindImageForShader( "B Tally Image", "bTally", shader, 2 );
	state.textureManager->BindImageForShader( "Sample Count", "count", shader, 3 );
	state.textureManager->BindTexForShader( "Adam", "adam", shader, 4 );

	glDispatchCompute( ( state.dimensions.x + 15 ) / 16, ( state.dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void Screenshot ( icarusState_t &state ) {

	const bool fullDepth = false;
	const bool srgbConvert = true;
	const string label = "Output Buffer";

	const GLuint tex = state.textureManager->Get( label );
	uvec2 dims = state.textureManager->GetDimensions( label );
	std::vector< float > imageBytesToSave;
	imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
	glBindTexture( GL_TEXTURE_2D, tex );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
	Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
	if ( srgbConvert == true ) {
		screenshot.RGBtoSRGB();
	}
	screenshot.SaturateAlpha();
	const string filename = string( "Icarus-" ) + timeDateString() + string( fullDepth ? ".exr" : ".png" );
	screenshot.Save( filename, fullDepth ? Image_4F::backend::TINYEXR : Image_4F::backend::LODEPNG );
}

void SystemUpdate ( icarusState_t &state, inputHandler_t &input ) {
	if ( input.getState4( KEY_R ) == KEYSTATE_RISING ) {
		ClearAccumulators( state );
	}

	if ( input.getState4( KEY_T ) == KEYSTATE_RISING ) {
		Screenshot( state );
	}

	if ( input.getState4( KEY_Y ) == KEYSTATE_RISING ) {
		CompileShaders( state );
	}
}

void CameraUpdate ( icarusState_t &state, inputHandler_t &input ) {
	const bool shift = input.getState( KEY_LEFT_SHIFT ) || input.getState( KEY_RIGHT_SHIFT );
	const bool control = input.getState( KEY_LEFT_CTRL ) || input.getState( KEY_RIGHT_CTRL );

	const float scalar = shift ? 0.1f : ( control ? 0.0005f : 0.02f );

	if ( input.getState( KEY_W ) ) {
		glm::quat rot = glm::angleAxis( scalar, state.basisX ); // basisX is the axis, therefore remains untransformed
		state.basisY = ( rot * vec4( state.basisY, 0.0f ) ).xyz();
		state.basisZ = ( rot * vec4( state.basisZ, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_S ) ) {
		glm::quat rot = glm::angleAxis( -scalar, state.basisX );
		state.basisY = ( rot * vec4( state.basisY, 0.0f ) ).xyz();
		state.basisZ = ( rot * vec4( state.basisZ, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_A ) ) {
		glm::quat rot = glm::angleAxis( -scalar, state.basisY ); // same as above, but basisY is the axis
		state.basisX = ( rot * vec4( state.basisX, 0.0f ) ).xyz();
		state.basisZ = ( rot * vec4( state.basisZ, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_D ) ) {
		glm::quat rot = glm::angleAxis( scalar, state.basisY );
		state.basisX = ( rot * vec4( state.basisX, 0.0f ) ).xyz();
		state.basisZ = ( rot * vec4( state.basisZ, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_Q ) ) {
		glm::quat rot = glm::angleAxis( scalar, state.basisZ ); // and again for basisZ
		state.basisX = ( rot * vec4( state.basisX, 0.0f ) ).xyz();
		state.basisY = ( rot * vec4( state.basisY, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_E ) ) {
		glm::quat rot = glm::angleAxis( -scalar, state.basisZ );
		state.basisX = ( rot * vec4( state.basisX, 0.0f ) ).xyz();
		state.basisY = ( rot * vec4( state.basisY, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_UP ) )		state.viewerPosition += scalar * state.basisZ;
	if ( input.getState( KEY_DOWN ) )		state.viewerPosition -= scalar * state.basisZ;
	if ( input.getState( KEY_RIGHT ) )		state.viewerPosition += scalar * state.basisX;
	if ( input.getState( KEY_LEFT ) )		state.viewerPosition -= scalar * state.basisX;
	if ( input.getState( KEY_PAGEDOWN ) )	state.viewerPosition += scalar * state.basisY;
	if ( input.getState( KEY_PAGEUP ) )	state.viewerPosition -= scalar * state.basisY;
	if ( input.getState( KEY_EQUALS ) )	state.FoV = state.FoV - 0.1f * scalar; // zoom in
	if ( input.getState( KEY_MINUS ) )		state.FoV = state.FoV + 0.1f * scalar; // zoom out
}

void RayUpdate ( icarusState_t &state ) {
	{ // update the buffer containing the pixel offsets
		std::vector< uvec2 > offsets( state.numRays, uvec2( 0 ) );
		for ( uint32_t i = 0; i < state.numRays; i++ ) {
			offsets[ i ] = GetNextOffset( state );
		}
		// send the data
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.offsetsSSBO );
		glBufferData( GL_SHADER_STORAGE_BUFFER, 2 * sizeof( GLuint ) * state.numRays, ( GLvoid * ) &offsets[ 0 ], GL_DYNAMIC_COPY );
	}

	{ // use the offsets to generate rays (first shader)
		const GLuint shader = state.RayGenerateShader;
		glUseProgram( shader );

		// need to update the stuff related to the rendering
		glUniform1f( glGetUniformLocation( shader, "FoV" ), state.FoV );
		glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( state.basisX ) );
		glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( state.basisY ) );
		glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( state.basisZ ) );
		glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( state.viewerPosition ) );
		glUniform2f( glGetUniformLocation( shader, "imageDimensions" ), state.dimensions.x, state.dimensions.y );

		static uint32_t frameNumber = 0;
		frameNumber++;
		glUniform1i( glGetUniformLocation( shader, "frameNumber" ), frameNumber );
		glUniform1i( glGetUniformLocation( shader, "cameraMode" ), state.cameraMode );
		glUniform1f( glGetUniformLocation( shader, "uvScaleFactor" ), state.uvScaleFactor );
		glUniform1i( glGetUniformLocation( shader, "DoFBokehMode" ), state.DoFBokehMode );
		glUniform1f( glGetUniformLocation( shader, "DoFRadius" ), state.DoFRadius );
		glUniform1f( glGetUniformLocation( shader, "DoFFocusDistance" ), state.DoFFocusDistance );
		glUniform1f( glGetUniformLocation( shader, "chromabScaleFactor" ), state.chromabScaleFactor );

		// blue noise texture
		static rngi noiseOffsetGen( 0, 512 );
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffsetGen(), noiseOffsetGen() );
		state.textureManager->BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );

		// fixed size, x times 256 = state.numRays
		glDispatchCompute( state.numRays / 256, 1, 1 );
		glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
	}

	// looping for bounces
	for ( uint32_t i = 0; i < state.maxBounces; i++ ) {

		{ // set initial state for all intersection structs
			const GLuint shader = state.RayClearShader;
			glUseProgram( shader );
			glDispatchCompute( state.numRays / 256, 1, 1 );
			glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
		}

		if ( state.runSDF ) { // intersect those rays with SDF geo
			const GLuint shader = state.RayIntersectShader_SDF;
			glUseProgram( shader );
			glDispatchCompute( state.numRays / 256, 1, 1 );
		}

		if ( state.runTriangle ) { // intersect those rays with a triangle
			const GLuint shader = state.RayIntersectShader_Triangle;
			glUseProgram( shader );
			glDispatchCompute( state.numRays / 256, 1, 1 );
		}

		//if ( state.runBVH ) { // intersect those rays with a triangle
		//	const GLuint shader = state.RayIntersectShader_TinyBVH;
		//	glUseProgram( shader );
		//	glDispatchCompute( state.numRays / 256, 1, 1 );
		//}

		if ( state.runVolume ) { // intersect those rays with the volume
			const GLuint shader = state.RayIntersectShader_Volume;
			glUseProgram( shader );
			// state.textureManager->BindImageForShader( "Voraldo Model", "VoraldoModel", shader, 5 );
			glDispatchCompute( state.numRays / 256, 1, 1 );
		}

		{ // do the shading on the intersection results (third shader)
			const GLuint shader = state.RayShadeShader;
			glUseProgram( shader );

			// this pass writes colors... I think it makes sense to also write depth, but only on the first bounce
			state.textureManager->BindImageForShader( "R Tally Image", "rTally", shader, 0 );
			state.textureManager->BindImageForShader( "G Tally Image", "gTally", shader, 1 );
			state.textureManager->BindImageForShader( "B Tally Image", "bTally", shader, 2 );
			state.textureManager->BindImageForShader( "Sample Count", "count", shader, 3 );

			glDispatchCompute( state.numRays / 256, 1, 1 );

			// make sure image writes complete
			glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}
}

