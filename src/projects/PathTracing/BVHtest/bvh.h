#include "../../../engine/includes.h"

// drawing heavily from:
// https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/

const float MAX_DISTANCE = 1e30f;

// representing a ray
struct ray_t {
	vec3 origin = vec3( 0.0f );
	vec3 direction = vec3( 0.0f );

	float distance = MAX_DISTANCE;
	vec2 uv = vec2( -1.0f );
	uint32_t triangleIdx;
};

// triangle data class
struct triangle_t {
	int idx;

	vec3 vertex0, vertex1, vertex2;
	vec3 centroid;
	vec3 normal;

	// further data, vertex colors, texcoords, etc
	vec3 color0, color1, color2;
	vec3 texcoord0, texcoord1, texcoord2;

	// moller-trombore intersection
	void intersect ( ray_t &ray ) {
		const vec3 edge1 = vertex1 - vertex0;
		const vec3 edge2 = vertex2 - vertex0;
		const vec3 h = cross( ray.direction, edge2 );
		const float a = dot( edge1, h );
		if ( a > -0.0001f && a < 0.0001f ) return; // ray parallel to triangle
		const float f = 1.0f / a;
		const vec3 s = ray.origin - vertex0;
		const float u = f * dot( s, h );
		if ( u < 0.0f || u > 1.0f ) { // fail on bad barycentric coords
			return;
		}
		const vec3 q = cross( s, edge1 );
		const float v = f * dot( ray.direction, q );
		if ( v < 0.0f || u + v > 1.0f ) { // fail on bad barycentric coords
			return;
		}
		const float t = f * dot( edge2, q );
		if ( t > 0.0001f ) {
			ray.distance = std::min( ray.distance, t ); // positive distance hit
			if ( ray.distance == t ) { // if we updated the hit, update the corresponding uv
				ray.uv = vec2( u, v );
				ray.triangleIdx = idx;
			}
		}
	}
};

struct aabb_t {
	vec3 mins = vec3(  1e30f );
	vec3 maxs = vec3( -1e30f );

	void growToInclude ( vec3 p ) {
		// update bbox minimums, with this new point
		mins.x = std::min( mins.x, p.x );
		mins.y = std::min( mins.y, p.y );
		mins.z = std::min( mins.z, p.z );
		// and same for bbox maximums
		maxs.x = std::max( maxs.x, p.x );
		maxs.y = std::max( maxs.y, p.y );
		maxs.z = std::max( maxs.z, p.z );
	}

	// get the surface area ( is this half the surface area? seems like maybe... )
	float area () {
		vec3 e = maxs - mins; // box extent
		return e.x * e.y + e.y * e.z + e.z * e.x;
	}
};

// for bounding box intersections
// this is used for the recursive version
static inline bool IntersectAABB ( const ray_t &ray, const vec3 bboxmin, const vec3 bboxmax ) {
	float tx1 = ( bboxmin.x - ray.origin.x ) / ray.direction.x, tx2 = ( bboxmax.x - ray.origin.x ) / ray.direction.x;
	float tmin = std::min( tx1, tx2 ), tmax = std::max( tx1, tx2 );
	float ty1 = ( bboxmin.y - ray.origin.y) / ray.direction.y, ty2 = ( bboxmax.y - ray.origin.y ) / ray.direction.y;
	tmin = std::max( tmin, std::min( ty1, ty2 ) ), tmax = std::min( tmax, std::max( ty1, ty2 ) );
	float tz1 = ( bboxmin.z - ray.origin.z) / ray.direction.z, tz2 = ( bboxmax.z - ray.origin.z ) / ray.direction.z;
	tmin = std::max( tmin, std::min( tz1, tz2 ) ), tmax = std::min( tmax, std::max( tz1, tz2 ) );
	return tmax >= tmin && tmin < ray.distance && tmax > 0.0f;
}

// this is used for the iterative version
static inline float IntersectAABB_i ( const ray_t& ray, const vec3 bboxmin, const vec3 bboxmax ) {
	float tx1 = ( bboxmin.x - ray.origin.x ) / ray.direction.x, tx2 = ( bboxmax.x - ray.origin.x ) / ray.direction.x;
	float tmin = std::min( tx1, tx2 ), tmax = std::max( tx1, tx2 );
	float ty1 = ( bboxmin.y - ray.origin.y ) / ray.direction.y, ty2 = ( bboxmax.y - ray.origin.y ) / ray.direction.y;
	tmin = std::max( tmin, std::min( ty1, ty2 ) ), tmax = std::min( tmax, std::max( ty1, ty2 ) );
	float tz1 = ( bboxmin.z - ray.origin.z ) / ray.direction.z, tz2 = ( bboxmax.z - ray.origin.z ) / ray.direction.z;
	tmin = std::max( tmin, std::min( tz1, tz2 ) ), tmax = std::min( tmax, std::max( tz1, tz2 ) );
	if ( tmax >= tmin && tmin < ray.distance && tmax > 0.0f ) {
		return tmin;
	} else {
		return MAX_DISTANCE;
	}
}

static inline const vec3 GetBarycentricCoords ( vec3 p0, vec3 p1, vec3 p2, vec3 P ) {
	vec3 s[ 2 ];
	for ( int i = 2; i--; ) {
		s[ i ][ 0 ] = p2[ i ] - p0[ i ];
		s[ i ][ 1 ] = p1[ i ] - p0[ i ];
		s[ i ][ 2 ] = p0[ i ] - P[ i ];
	}
	vec3 u = glm::cross( s[ 0 ], s[ 1 ] );
	if ( std::abs( u[ 2 ] ) > 1e-2f )	 	// If u[ 2 ] is zero then triangle ABC is degenerate
		return vec3( 1.0f - ( u.x + u.y ) / u.z, u.y / u.z, u.x / u.z );
	return vec3( -1.0f, 1.0f, 1.0f );	// in this case generate negative coordinates, it will be thrown away by the rasterizer
}

// bvh node class
struct bvhNode_t {
	vec3 aabbMin;
	uint32_t leftChild;
	vec3 aabbMax;
	uint32_t primitiveCount;
	bool isLeaf() { return primitiveCount > 0; }
};

// helper struct for binned SAH
struct bin_t {
	aabb_t bounds;
	int primitiveCount = 0;
};

// top level bvh class
struct bvh_t {
	std::vector< triangle_t > triangleList;
	std::vector< uint32_t > triangleIndices;

	// create acceleration structure of bvh nodes, from the list of triangles
	std::vector< bvhNode_t > bvhNodes;
	uint32_t rootNodeIdx = 0;
	uint32_t nodesUsed = 1;

	void Init () {
		triangleList.resize( 0 );
		triangleIndices.resize( 0 );

		bvhNodes.resize( 0 );
		rootNodeIdx = 0;
		nodesUsed = 1;
	}

	// load model triangles
	SoftRast s;
	aabb_t Load () {
		s.LoadModel( "../../SponzaRepack/sponza.obj", "../../SponzaRepack/" );
		// s.LoadModel( "../San_Miguel/san-miguel-low-poly.obj", "../San_Miguel/" );
		// s.LoadModel( "../birdOfPrey/birdOfPrey.obj", "../birdOfPrey/textures/" );

		// and then get the triangles from here
		triangleList.resize( 0 );

		const bool reportbbox = true;
		aabb_t modelbbox;

		for ( auto& triangle : s.triangles ) {
			triangle_t loaded;

			// vertex positions
			loaded.vertex0 = triangle.p0;
			loaded.vertex1 = triangle.p1;
			loaded.vertex2 = triangle.p2;

			if ( reportbbox == true ) {
				modelbbox.growToInclude( triangle.p0 );
				modelbbox.growToInclude( triangle.p1 );
				modelbbox.growToInclude( triangle.p2 );
			}

			// vertex texcoords
			loaded.texcoord0 = triangle.t0;
			loaded.texcoord1 = triangle.t1;
			loaded.texcoord2 = triangle.t2;

			// vertex normals in n0, n1, n2... not sure how to use that in a pathtracer
			loaded.normal = normalize( triangle.n0 + triangle.n1 + triangle.n2 );

			// need to know which index in the list this is
			loaded.idx = triangleList.size();

			// and push it onto the list
			triangleList.push_back( loaded );
		}

		// return the bounding box for the model
		return modelbbox;
	}

	void UpdateNodeBounds ( uint32_t nodeIndex ) {
		// calculate the bounds for a BVH node, based on contained primitives
		bvhNode_t &node = bvhNodes[ nodeIndex ];

		aabb_t bbox;
		for ( uint32_t i = 0; i < node.primitiveCount; i++ ) {
			triangle_t &leafTriangle = triangleList[ triangleIndices[ node.leftChild + i ] ];

			bbox.growToInclude( leafTriangle.vertex0 );
			bbox.growToInclude( leafTriangle.vertex1 );
			bbox.growToInclude( leafTriangle.vertex2 );
		}

		node.aabbMax = bbox.maxs;
		node.aabbMin = bbox.mins;
	}

	float FindBestSplitPlane ( bvhNode_t& node, int& axis, float& splitPos ) {
		float bestCost = 1e30f;

		// for the three axes, x,y,z ( optimization opportunity here, only considering the longest axis )
		for ( int a = 0; a < 3; a++ ) {

			// iterate through the list of triangles
			float boundsMin = 1e30f, boundsMax = -1e30f;
			for ( uint32_t i = 0; i < node.primitiveCount; i++ ) {
				// find the bounds for the primitive centroids of the primitives in the node
				triangle_t& triangle = triangleList[ triangleIndices[ node.leftChild + i ] ];
				boundsMin = min( boundsMin, triangle.centroid[ a ] );
				boundsMax = max( boundsMax, triangle.centroid[ a ] );
			}

			// if the bounds are degenerate, continue
			if ( boundsMin == boundsMax ) {
				continue;
			}

			// otherwise, we start the binning
			constexpr int NUM_BINS = 8;
			bin_t bin[ NUM_BINS ];

			// precompute a scale factor based on number of bins
			float scale = NUM_BINS / ( boundsMax - boundsMin );

			// iterating through the primitives again
			for ( uint32_t i = 0; i < node.primitiveCount; i++ ) {
				triangle_t& triangle = triangleList[ triangleIndices[ node.leftChild + i ] ];

				// figure out what bin it falls into
				int bindex = min( NUM_BINS - 1, ( int )( ( triangle.centroid[ a ] - boundsMin ) * scale ) );

				// increment bin count
				bin[ bindex ].primitiveCount++;

				// grow the bounds for that bin, to include all three vertices of this primitive
				bin[ bindex ].bounds.growToInclude( triangle.vertex0 );
				bin[ bindex ].bounds.growToInclude( triangle.vertex1 );
				bin[ bindex ].bounds.growToInclude( triangle.vertex2 );
			}

			// left and right sweeps through the bins
			float leftArea[ NUM_BINS - 1 ], rightArea[ NUM_BINS - 1 ];
			int leftCount[ NUM_BINS - 1 ], rightCount[ NUM_BINS - 1 ];
			aabb_t leftBox, rightBox;
			int leftSum = 0, rightSum = 0;
			for ( int i = 0; i < NUM_BINS - 1; i++ ) {
				// left starts at 0 and goes up
				leftSum += bin[ i ].primitiveCount;
				leftCount[ i ] = leftSum;
				leftBox.growToInclude( bin[ i ].bounds.mins );
				leftBox.growToInclude( bin[ i ].bounds.maxs );
				leftArea[ i ] = leftBox.area();

				// right starts at NUM_BINS-1 and goes down
				rightSum += bin[ NUM_BINS - 1 - i ].primitiveCount;
				rightCount[ NUM_BINS - 2 - i ] = rightSum;
				rightBox.growToInclude( bin[ NUM_BINS - 1 - i ].bounds.mins );
				rightBox.growToInclude( bin[ NUM_BINS - 1 - i ].bounds.maxs );
				rightArea[ NUM_BINS - 2 - i ] = rightBox.area();
			}

			// calculating SAH cost for each direction
			scale = ( boundsMax - boundsMin ) / NUM_BINS;
			for ( int i = 0; i < NUM_BINS - 1; i++ ) {
				float planeCost = leftCount[ i ] * leftArea[ i ] + rightCount[ i ] * rightArea[ i ];
				// update best cost etc while iterating
				if ( planeCost < bestCost ) {
					axis = a;
					splitPos = boundsMin + scale * ( i + 1 );
					bestCost = planeCost;
				}
			}
		}

		return bestCost;
	}

	float CalculateNodeCost ( bvhNode_t& node ) {
		vec3 e = node.aabbMax - node.aabbMin; // extent of the node
		float surfaceArea = e.x * e.y + e.y * e.z + e.z * e.x;
		return node.primitiveCount * surfaceArea;
	}

	float EvaluateSAH ( bvhNode_t& node, int axis, float pos ) {
		// determine triangle counts and bounds for this split candidate
		aabb_t leftBox, rightBox;
		int leftCount = 0, rightCount = 0;
		for( uint32_t i = 0; i < node.primitiveCount; i++ ) {
			triangle_t& triangle = triangleList[ triangleIndices[ node.leftChild + i ] ];
			if ( triangle.centroid[ axis ] < pos ) {
				leftCount++;
				leftBox.growToInclude( triangle.vertex0 );
				leftBox.growToInclude( triangle.vertex1 );
				leftBox.growToInclude( triangle.vertex2 );
			} else {
				rightCount++;
				rightBox.growToInclude( triangle.vertex0 );
				rightBox.growToInclude( triangle.vertex1 );
				rightBox.growToInclude( triangle.vertex2 );
			}
		}
		float cost = leftCount * leftBox.area() + rightCount * rightBox.area();
		return ( cost > 0.0f ) ? cost : 1e30f;
	}

	void Subdivide ( uint32_t nodeIndex ) {
		// ref current node
		bvhNode_t &currentNode = bvhNodes[ nodeIndex ];

		// we can go ahead and kill it if we at a leaf node
		if ( currentNode.primitiveCount <= 2 ) return;

		int splitAxis = 0;
		float splitPos = 0.0f;

		// surface area heuristic to pick the split axis and the position
		int bestAxis;
		float bestPos;
		float bestCost = FindBestSplitPlane( currentNode, bestAxis, bestPos );

		// we can early out if we aren't improving
		float noSplitPos = CalculateNodeCost( currentNode );
		if ( bestCost >= noSplitPos ) return;

		splitAxis = bestAxis;
		splitPos = bestPos;

		// divide the group into two halves along this axis, partition by putting the split primitives at the end of the list
		int i = currentNode.leftChild;
		int j = i + currentNode.primitiveCount - 1;

		// iterate through, and place triangles based on centroid position relative to the dividing line
		while ( i <= j ) {
			if ( triangleList[ triangleIndices[ i ] ].centroid[ splitAxis ] < splitPos ) {
				i++;
			} else {
				std::swap( triangleIndices[ i ], triangleIndices[ j-- ] );
			}
		}

		// because these are child nodes, we know they are subsets of this parent node, interesting
			// this enables that in-place partitioning, which is pretty cool

		// where is the partition?
		const uint32_t leftCount = i - currentNode.leftChild;

		// abort the split if either of the sides are empty
		if ( leftCount == 0 || leftCount == currentNode.primitiveCount ) return;

		// otherwise we're creating child nodes for each half ( note use of contiguous nodes, here )
		int leftChildIdx = nodesUsed++;
		int rightChildIdx = nodesUsed++;

		// ...so I guess you can always get the right child index back, by looking at base idx + primitive count
		bvhNodes[ leftChildIdx ].leftChild = currentNode.leftChild;
		bvhNodes[ leftChildIdx ].primitiveCount = leftCount;

		bvhNodes[ rightChildIdx ].leftChild = i;
		bvhNodes[ rightChildIdx ].primitiveCount = currentNode.primitiveCount - leftCount;

		// not tracking the isLeaf bool, means primitiveCount has to be used to id leaves vs interior nodes
		currentNode.primitiveCount = 0; // so setting this now interior node to zero to indicate this isn't a leaf
		currentNode.leftChild = leftChildIdx; // and the child there - note that right child can always be recovered with +1 ( contiguous )

		// update the bounds for the child nodes
		UpdateNodeBounds( leftChildIdx );
		UpdateNodeBounds( rightChildIdx );

		// recurse into each of the child nodes, from this
		Subdivide( leftChildIdx );
		Subdivide( rightChildIdx );
	}

	void BuildTree () {
		// maximum possible size is 2N + 1 nodes, where N is the number of triangles
			// ( N leaves have N/2 parents, N/4 granparents, etc... )
		bvhNodes.resize( triangleList.size() * 2 - 1 );

		// precompute the centroids of all the triangles
		for ( auto& triangle : triangleList ) {
			triangle.centroid = vec3( triangle.vertex0 + triangle.vertex1 + triangle.vertex2 ) / 3.0f;
		}

		// populate the index buffer, initially just in order
		triangleIndices.resize( triangleList.size() );
		for ( uint32_t i = 0; i < triangleIndices.size(); i++ ) {
			triangleIndices[ i ] = i;
		}

		// not fully understanding this piece, yet
		bvhNode_t &root = bvhNodes[ rootNodeIdx ];
		root.leftChild = 0;
		root.primitiveCount = triangleList.size();

		// updating node bounds, for the root node - so it's the bounding box for entire model
		UpdateNodeBounds( rootNodeIdx );

		// subdivide from the root node, recursively
		Subdivide( rootNodeIdx );
	}

	// naiive traversal for comparison
	void naiiveTraversal ( ray_t &ray ) {
		// return information at hit location
		for ( uint32_t i = 0; i < triangleList.size(); i++ ) {
			triangleList[ i ].intersect( ray );
		}
	}

	float rayComplexityCounter = 0.0f;
	void RayComplexityCounterReset () {
		rayComplexityCounter = 0.0f;
	}
	float GetRayComplexityCounter () {
		return rayComplexityCounter;
	}

	void recursiveTraversal ( ray_t &ray, const uint32_t nodeIdx ) {
		bvhNode_t& node = bvhNodes[ nodeIdx ];
		if ( !IntersectAABB( ray, node.aabbMin, node.aabbMax ) ) {
			return; // bounds are not intersected, no further checking required on this branch
		}

		if ( node.isLeaf() ) {
			// leaf nodes have one or more primitives to check against
			for ( uint32_t i = 0; i < node.primitiveCount; i++ ) {
				rayComplexityCounter++;
				triangleList[ triangleIndices[ node.leftChild + i ] ].intersect( ray );
			}
		} else {
			rayComplexityCounter++;
			// we need to recurse both of the child nodes
			recursiveTraversal( ray, node.leftChild );
			recursiveTraversal( ray, node.leftChild + 1 ); // making use of the contiguous nodes
		}
	}

	void iterativeTraversal ( ray_t& ray ) {
		bvhNode_t* node = &bvhNodes[ rootNodeIdx ], *stack[ 64 ];
		uint32_t stackPtr = 0;
		while ( 1 ) {
			if ( node->isLeaf() ) {
				for ( uint32_t i = 0; i < node->primitiveCount; i++ )
					triangleList[ triangleIndices[ node->leftChild + i ] ].intersect( ray );
				if ( stackPtr == 0 ) break; else node = stack[ --stackPtr ];
				continue;
			}
			bvhNode_t* child1 = &bvhNodes[ node->leftChild ];
			bvhNode_t* child2 = &bvhNodes[ node->leftChild + 1 ];
			float dist1 = IntersectAABB_i( ray, child1->aabbMin, child1->aabbMax );
			float dist2 = IntersectAABB_i( ray, child2->aabbMin, child2->aabbMax );
			if ( dist1 > dist2 ) { std::swap( dist1, dist2 ); std::swap( child1, child2 ); }
			if ( dist1 == MAX_DISTANCE ) {
				if ( stackPtr == 0 ) {
					break;
				} else {
					node = stack[ --stackPtr ];
				}
			} else {
				node = child1;
				if ( dist2 != MAX_DISTANCE ) stack[ stackPtr++ ] = child2;
			}
		}
	}

	// test ray against acceleration structure
	void acceleratedTraversal ( ray_t &ray ) {
		// recursiveTraversal( ray, rootNodeIdx );
		iterativeTraversal( ray );
	}
};

// probably a wrapper class for the renderer, here, is easiest
struct testRenderer_t {

	// holding scene data
	bvh_t accelerationStructure;

	// image data, to display
	Image_4F imageBuffer;
	bool imageBufferDirty; // does this image need to be resent to the GPU?

	// setup the worker threads
	static constexpr int NUM_THREADS = 1;
	std::thread threads[ NUM_THREADS + 1 ];

	static constexpr uint32_t REPORT_DELAY = 32; 				// reporter thread sleep duration, in ms
	static constexpr uint32_t PROGRESS_INDICATOR_STOPS = 69; // cli spaces to take up

	static constexpr float scaleFactor = 0.618f;
	static constexpr uint32_t X_IMAGE_DIM = 300 * scaleFactor;
	static constexpr uint32_t Y_IMAGE_DIM = 200 * scaleFactor;
	static constexpr uint32_t TILESIZE_XY = 4;
	static constexpr uint32_t NUM_SAMPLES = 4;
	// bool threadsShouldDie = false;

	std::atomic< uint32_t > tileIndexCounter{ 0 };
	std::atomic< uint32_t > tileFinishCounter{ 0 };
	uint32_t totalTileCount = std::ceil( X_IMAGE_DIM / TILESIZE_XY ) * ( std::ceil( Y_IMAGE_DIM / TILESIZE_XY ) + 1 );

	// camera parameterization could use work
	const vec3 eyeLocation = vec3( -100.0f, 600.0f, 0.0f );
	rng jitter = rng( 0.0f, 1.0f );
	rng centeredJitter = rng( -1.0f, 1.0f );

	testRenderer_t () {
		// create the preview image - probably eventually use alpha channel to send traversal depth kind of stats
		imageBuffer = Image_4F( X_IMAGE_DIM, Y_IMAGE_DIM );
		imageBuffer.SaturateAlpha();
	}

	// accelerated traversal, for comparison
	void acceleratedTraversal () {

		// so we are going to want to do some more setup for the camera, maybe
		for ( int id = 0; id <= NUM_THREADS; id++ ) {
			threads[ id ] = ( id == NUM_THREADS ) ? std::thread( // reporter thread
				[ this, id ] () {
					const auto tstart = std::chrono::system_clock::now();
					while ( true ) { // report timing
						// show status - break on 100% completion
						cout << "\r\033[K";
						cout << "["; //  [=====....................] where equals shows progress
						const float frac = float( tileFinishCounter ) / float( totalTileCount );
						int numFill = std::floor( PROGRESS_INDICATOR_STOPS * frac ) - 1;
						for( int i = 0; i <= numFill; i++ ) cout << "=";
						for( uint32_t i = 0; i < PROGRESS_INDICATOR_STOPS - numFill; i++ ) cout << ".";
						cout << "]" << std::flush;
						cout << "[" << std::setw( 3 ) << 100.0 * frac << "% " << std::flush;

						cout << std::setw( 7 ) << std::showpoint << std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now() - tstart ).count() / 1000.0 << " sec]" << std::flush;

						if( tileFinishCounter >= totalTileCount ){
							const float seconds = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now() - tstart ).count() / 1000.0;
							cout << "\r\033[K[" << std::string( PROGRESS_INDICATOR_STOPS + 1, '=' ) << "] " << seconds << " sec"<< endl;
							break;
						}

						// sleep for some amount of time before showing again
						std::this_thread::sleep_for( std::chrono::milliseconds( REPORT_DELAY ) );
					}
				}
			) : std::thread( // worker threads
				[ this, id ] () {
					// distribution of work is tile based
					while ( true ) {
						// solve for x and y from the index
						unsigned long long index = tileIndexCounter.fetch_add( 1 );
						if ( index >= totalTileCount ) break;

						int numTilesX = int( std::ceil( float( X_IMAGE_DIM ) / float( TILESIZE_XY ) ) );

						const int tile_x_index = index % numTilesX;
						const int tile_y_index = ( index / numTilesX );

						const int tile_base_x = tile_x_index * TILESIZE_XY;
						const int tile_base_y = tile_y_index * TILESIZE_XY;

						for ( uint32_t y = tile_base_y; y < tile_base_y + TILESIZE_XY; y++ )
						for ( uint32_t x = tile_base_x; x < tile_base_x + TILESIZE_XY; x++ ) {

							const float numSamples = NUM_SAMPLES;
							accelerationStructure.RayComplexityCounterReset();

							vec3 color = vec3( 0.0f );
							float d = MAX_DISTANCE;

							for ( uint32_t i = 0; i < NUM_SAMPLES; i++ ) {
								const float xRemap = RangeRemap( x + jitter(), 0, imageBuffer.Width(), 10.0f, -10.0f );
								const float yRemap = RangeRemap( y + jitter(), 0, imageBuffer.Height(), 10.0f, -10.0f );

								// test a ray against the triangles
								ray_t ray;
								// ray.origin = 100.0f * vec3( xRemap * ( float( imageBuffer.Width() ) / float( imageBuffer.Height() ) ), yRemap, 0.0f ) + eyeLocation;
								ray.origin = 40.0f * vec3( xRemap * ( float( imageBuffer.Width() ) / float( imageBuffer.Height() ) ), yRemap, 0.0f ) + eyeLocation;
								// ray.direction = normalize( vec3( 1.0f, 0.0f, 0.25f ) );
								ray.direction = normalize( vec3( 0.0f, 0.0f, 1.0f ) );

								accelerationStructure.acceleratedTraversal( ray );

								if ( ray.distance < MAX_DISTANCE ) {
									triangle_t triangle = accelerationStructure.triangleList[ ray.triangleIdx ];
									vec3 barycentricCoords = GetBarycentricCoords( triangle.vertex0, triangle.vertex1, triangle.vertex2, ray.origin + ray.distance * ray.direction );

									// computing a lighting term
									const vec3 lightSize = vec3( 1.0f, 1.0f, 1.0f );
									const vec3 lightPosition = vec3( 0.0f + lightSize.x * centeredJitter(), 500.0f + lightSize.y * centeredJitter(), 0.0f + lightSize.z * centeredJitter() );
									ray_t lightRay;
									lightRay.origin = ray.origin + ray.distance * ray.direction + 0.01f * triangle.normal;
									lightRay.direction = normalize( lightPosition - lightRay.origin );
									vec3 lightTerm = vec3( 0.01f );

									accelerationStructure.acceleratedTraversal( lightRay );
									const float dLight = distance( lightRay.origin, lightPosition );
									if ( lightRay.distance >= dLight ) {
									// if ( lightRay.distance < 20.0f ) {
									// if ( distance( lightRay.origin, vec3( 300.0f ) ) < 200.0f ) {

										vec3 lightColor = palette::paletteRef( RemapRange( lightPosition.z, -lightSize.z, lightSize.z, 0.0f, 1.0f ) );
										lightTerm = vec3( 100.0f / std::pow( dLight, 1.2f ) ) * lightColor;
										// lightTerm = 1.0f;
									}

									// going to have to move this into the traversal, if I want to support alpha testing
									vec2 interpolatedTC =
										barycentricCoords.x * triangle.texcoord0.xy() +
										barycentricCoords.y * triangle.texcoord1.xy() +
										barycentricCoords.z * triangle.texcoord2.xy();

									int texturePick = triangle.texcoord0.z * 2;

									// color = accelerationStructure.s.TexRef( glm::mod( vec2( interpolatedTC.x, 1.0f - interpolatedTC.y ), vec2( 1.0f ) ), texturePick ).rgb();
									color += lightTerm * accelerationStructure.s.TexRef( glm::mod( interpolatedTC, vec2( 1.0f ) ), texturePick ).rgb() / numSamples;
									// color = triangle.normal;

									d = ray.distance;
									// d = dLight;

									// color = ray.origin + ray.distance * ray.direction;
								}
							}

							color_4F val;
							val[ red ] = color.r;
							val[ green ] = color.g;
							val[ blue ] = color.b;
							// val[ alpha ] = accelerationStructure.GetRayComplexityCounter() * ( hitSamples / numSamples );
							val[ alpha ] = d;
							imageBuffer.SetAtXY( x, y, val );
						}
						tileFinishCounter.fetch_add( 1 );
					}
				}
			);
		}
		for ( int id = 0; id <= NUM_THREADS; id++ ) {
			threads[ id ].join();
		}
	}
};