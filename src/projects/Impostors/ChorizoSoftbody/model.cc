#include "model.h"

#include <iostream>
#include <fstream>
#include <string>

model::model() {
	auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
	auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();

	fnFractal->SetSource( fnSimplex );
	fnFractal->SetOctaveCount( 2 );

	fnGenerator = fnFractal;
}

model::~model() {}

void model::loadFramePoints() {
	// clear out data, so this can also be used as a reset
	nodes.clear();
	edges.clear();
	faces.clear();

	// assumes obj file without the annotations -
	// specifically carFrameWPanels.obj which has a few lines already removed

	std::ifstream infile( "../src/projects/SoftBodies/CPU/carFrameWPanels.obj" );
	std::vector< glm::vec3 > normals;

	// add the anchored points ( wheel control points )
	int offset = 3; // for 4 anchored wheel points, this is 3 - to eat up the off-by-one from the one-indexed OBJ format

	// add anchored wheel control points here
	addNode( &simParameters.anchoredNodeMass, glm::vec3( -0.6125f, -0.38f,  1.95f ),  true ); // front left
	addNode( &simParameters.anchoredNodeMass, glm::vec3(  0.6125f, -0.38f,  1.95f ),  true ); // front right
	addNode( &simParameters.anchoredNodeMass, glm::vec3( -0.7f,    -0.38f, -0.86f ),  true ); // back left
	addNode( &simParameters.anchoredNodeMass, glm::vec3(  0.7f,    -0.38f, -0.86f ),  true ); // back right

	while ( infile.peek() != EOF ) {
		std::string read;
		infile >> read;

		if ( read == "v" ) {
			float x, y, z;
			infile >> x >> y >> z;
			// add an unanchored node, with the configured node mass
			addNode( &simParameters.chassisNodeMass, glm::vec3( x, y, z ), false );
		} else if ( read == "vn" ) {
			float x, y, z;
			infile >> x >> y >> z;
			normals.push_back( glm::vec3( x, y, z ) );  // three floats determine the normal vector
		} else if ( read == "f" ) {
			char throwaway;
			int xIndex, yIndex, zIndex, nIndex;
			infile >> xIndex >> throwaway >> throwaway >> nIndex
					>> yIndex >> throwaway >> throwaway >> nIndex
					>> zIndex >> throwaway >> throwaway >> nIndex;
			// this needs the three node indices, as well as the normal from the normals list
			addFace( xIndex + offset, yIndex + offset, zIndex + offset, normals[ nIndex - 1 ] );
			// add the three edges of the triangle, since the obj export skips edges which are included in a face
			addEdge( xIndex + offset, yIndex + offset, CHASSIS );
			addEdge( zIndex + offset, yIndex + offset, CHASSIS );
			addEdge( zIndex + offset, xIndex + offset, CHASSIS );
		} else if ( read == "l" ) {
			int index1, index2;
			infile >> index1 >> index2;
			addEdge( index1 + offset, index2 + offset, CHASSIS ); // two node indices ( offset to match the list ), CHASSIS type
		}
	}

	// hardcoding this is yucky

// suspension edge
	// front left
	addEdge( 0, 25, SUSPENSION );
	addEdge( 0, 35, SUSPENSION );
	addEdge( 0, 37, SUSPENSION );
	addEdge( 0, 39, SUSPENSION );
	addEdge( 0, 41, SUSPENSION );
	addEdge( 0, 42, SUSPENSION );
	addEdge( 0, 43, SUSPENSION );
	addEdge( 0, 44, SUSPENSION );
	addEdge( 0, 45, SUSPENSION );
	// mirrored SUSPENSION_INBOARD edges
	addEdge( 0, 13, SUSPENSION_INBOARD );
	addEdge( 0, 15, SUSPENSION_INBOARD );
	addEdge( 0, 17, SUSPENSION_INBOARD );
	addEdge( 0, 19, SUSPENSION_INBOARD );
	addEdge( 0, 20, SUSPENSION_INBOARD );
	addEdge( 0, 21, SUSPENSION_INBOARD );
	addEdge( 0, 22, SUSPENSION_INBOARD );
	addEdge( 0, 23, SUSPENSION_INBOARD );
	addEdge( 0, 24, SUSPENSION_INBOARD );

	// front right
	addEdge( 1, 13, SUSPENSION );
	addEdge( 1, 15, SUSPENSION );
	addEdge( 1, 17, SUSPENSION );
	addEdge( 1, 19, SUSPENSION );
	addEdge( 1, 20, SUSPENSION );
	addEdge( 1, 21, SUSPENSION );
	addEdge( 1, 22, SUSPENSION );
	addEdge( 1, 23, SUSPENSION );
	addEdge( 1, 24, SUSPENSION );
	// mirrored SUSPENSION_INBOARD edges
	addEdge( 1, 35, SUSPENSION_INBOARD );
	addEdge( 1, 25, SUSPENSION_INBOARD );
	addEdge( 1, 37, SUSPENSION_INBOARD );
	addEdge( 1, 39, SUSPENSION_INBOARD );
	addEdge( 1, 41, SUSPENSION_INBOARD );
	addEdge( 1, 42, SUSPENSION_INBOARD );
	addEdge( 1, 43, SUSPENSION_INBOARD );
	addEdge( 1, 44, SUSPENSION_INBOARD );
	addEdge( 1, 45, SUSPENSION_INBOARD );

	// back left
	addEdge( 2, 26, SUSPENSION );
	addEdge( 2, 28, SUSPENSION );
	addEdge( 2, 29, SUSPENSION );
	addEdge( 2, 30, SUSPENSION );
	addEdge( 2, 31, SUSPENSION );
	addEdge( 2, 32, SUSPENSION );
	addEdge( 2, 36, SUSPENSION );
	addEdge( 2, 38, SUSPENSION );
	// mirrored SUSPENSION_INBOARD edges
	addEdge( 2, 4, SUSPENSION_INBOARD );
	addEdge( 2, 6, SUSPENSION_INBOARD );
	addEdge( 2, 7, SUSPENSION_INBOARD );
	addEdge( 2, 8, SUSPENSION_INBOARD );
	addEdge( 2, 9, SUSPENSION_INBOARD );
	addEdge( 2, 10, SUSPENSION_INBOARD );
	addEdge( 2, 14, SUSPENSION_INBOARD );
	addEdge( 2, 16, SUSPENSION_INBOARD );

	// back right
	addEdge( 3, 4, SUSPENSION );
	addEdge( 3, 6, SUSPENSION );
	addEdge( 3, 7, SUSPENSION );
	addEdge( 3, 8, SUSPENSION );
	addEdge( 3, 9, SUSPENSION );
	addEdge( 3, 10, SUSPENSION );
	addEdge( 3, 14, SUSPENSION );
	addEdge( 3, 16, SUSPENSION );
	// mirrored SUSPENSION_INBOARD edges
	addEdge( 3, 26, SUSPENSION_INBOARD );
	addEdge( 3, 28, SUSPENSION_INBOARD );
	addEdge( 3, 29, SUSPENSION_INBOARD );
	addEdge( 3, 30, SUSPENSION_INBOARD );
	addEdge( 3, 31, SUSPENSION_INBOARD );
	addEdge( 3, 32, SUSPENSION_INBOARD );
	addEdge( 3, 36, SUSPENSION_INBOARD );
	addEdge( 3, 38, SUSPENSION_INBOARD );
}

float model::getGroundPoint( float x, float y ) {
	return fnGenerator->GenSingle2D( x * scale, y * scale + noiseOffset, 42069 ) * simParameters.noiseAmplitudeScale - 0.4f;
}

void model::SingleThreadSoftbodyUpdate() {
	for ( auto& n : nodes ) {
		if ( !n.anchored ) {
			glm::vec3 forceAccumulator = glm::vec3( 0.0f );
			float k = 0.0f;
			float d = 0.0f;
			//get your forces from all the connections - accumulate in forceAccumulator vector
			for ( auto& e : n.edges ) {
				switch ( e.type ) {
					case CHASSIS:
						k = simParameters.chassisKConstant;
						d = simParameters.chassisDamping;
						break;
					case SUSPENSION:
					case SUSPENSION_INBOARD:
						k = simParameters.suspensionKConstant;
						d = simParameters.suspensionDamping;
						break;
					// case TIRE:
					//tbd, maybe
					// break;
				}
				//get positions of the two nodes involved
				glm::vec3 myPosition = n.oldPosition;
				glm::vec3 otherPosition = nodes[ e.node2 ].anchored ?
				nodes[ e.node2 ].position :    // use new position for anchored nodes ( position is up to date )
				nodes[ e.node2 ].oldPosition;  // use old position for unanchored nodes ( old value is what you use )

				//less than 1 is shorter, greater than 1 is longer than base length
				float springRatio = glm::distance( myPosition, otherPosition ) / e.baseLength;

				//spring force
				forceAccumulator += -k * glm::normalize( myPosition - otherPosition ) * ( springRatio - 1 ); //should this be normalized or no?
				//force += -k * ( myPosition - otherPosition ) * ( springRatio - 1 );

				forceAccumulator -= d * n.oldVelocity;                           //damping force
			}
			forceAccumulator += ( *n.mass ) * glm::vec3( 0.0f, -simParameters.gravity, 0.0f ); // add gravity
			glm::vec3 acceleration = forceAccumulator / ( *n.mass );                           // get the resulting acceleration
			n.velocity = n.oldVelocity + acceleration * simParameters.timeScale;    // compute the new velocity
			n.position = n.oldPosition + n.velocity * simParameters.timeScale;      // get the new position
		}
	}
}

void model::CachePreviousValues () {
	for ( auto& n : nodes )
		if ( !n.anchored ) {
			n.oldPosition = n.position;
			n.oldVelocity = n.velocity;
		}
}

void model::Update ( const int numUpdates ) {
	// offset the noise over time
	noiseOffset += 0.001f * simParameters.noiseSpeed;

	// sample terrain surface height at the wheel points
	nodes[ 0 ].position.y = getGroundPoint( nodes[ 0 ].position.x, nodes[ 0 ].position.z ) / scale + wheelDiameter;
	nodes[ 1 ].position.y = getGroundPoint( nodes[ 1 ].position.x, nodes[ 1 ].position.z ) / scale + wheelDiameter;
	nodes[ 2 ].position.y = getGroundPoint( nodes[ 2 ].position.x, nodes[ 2 ].position.z ) / scale + wheelDiameter;
	nodes[ 3 ].position.y = getGroundPoint( nodes[ 3 ].position.x, nodes[ 3 ].position.z ) / scale + wheelDiameter;

	// back up velocities and positions in the 'old' values

	// single threaded update structure
	auto tstart = std::chrono::high_resolution_clock::now();
	for ( int i = 0; i < numUpdates; i++ ){
		CachePreviousValues();
		SingleThreadSoftbodyUpdate();
	}
	// cout << "singlethread update " << numUpdates << "x " << std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now() - tstart ).count() / 1000.0f << "us\n";
}

void model::addNode( float* mass, glm::vec3 position, bool anchored ) {
	node n;
	n.mass = mass;
	n.anchored = anchored;
	n.position = n.oldPosition = position;
	n.velocity = n.oldVelocity = glm::vec3( 0.0f );
	nodes.push_back( n );
}

void model::addEdge( int nodeIndex1, int nodeIndex2, edgeType type ) {
	edge e;
	e.node1 = nodeIndex1;
	e.node2 = nodeIndex2;
	e.type = type;
	e.baseLength = glm::distance( nodes[ e.node1 ].position, nodes[ e.node2 ].position );
	edges.push_back( e );

	// add an edge for the first node
	nodes[ e.node1 ].edges.push_back( e );

	// add an edge for the second node - swap first and second node
	e.node1 = nodeIndex2;
	e.node2 = nodeIndex1;
	nodes[ e.node1 ].edges.push_back( e );
}

// parameters tbd - probably just the
void model::addFace( int nodeIndex1, int nodeIndex2, int nodeIndex3, glm::vec3 normal ) {
	face f;
	f.node1 = nodeIndex1;
	f.node2 = nodeIndex2;
	f.node3 = nodeIndex3;

	// TODO: add normals

	faces.push_back( f );
}
