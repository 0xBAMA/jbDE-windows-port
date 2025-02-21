#ifndef MODEL
#define MODEL

#pragma once

#include "../../../engine/includes.h"

// default colors to use
namespace softbodyColors {
	const glm::vec4 black =  glm::vec4( 0.00f, 0.00f, 0.00f, 1.00f );
	const glm::vec4 blue =   glm::vec4( 0.50f, 0.00f, 0.00f, 1.00f );
	const glm::vec4 red =    glm::vec4( 0.00f, 0.00f, 0.50f, 1.00f );
	const glm::vec4 tan =    glm::vec4( 0.45f, 0.35f, 0.22f, 1.00f );
	const glm::vec4 yellow = glm::vec4( 0.43f, 0.36f, 0.11f, 1.00f );
	const glm::vec4 brown =  glm::vec4( 0.30f, 0.20f, 0.10f, 1.00f );
	const glm::vec4 green =  glm::vec4( 0.25f, 0.28f, 0.00f, 1.00f );
	const glm::vec4 steel =  glm::vec4( 0.15f, 0.24f, 0.26f, 1.00f );
	const glm::vec4 G0 =     glm::vec4( 0.35f, 0.23f, 0.04f, 1.00f );
	const glm::vec4 G1 =     glm::vec4( 0.42f, 0.46f, 0.14f, 1.00f );
	const glm::vec4 BG =     glm::vec4( 0.40f, 0.30f, 0.10f, 1.00f );
};

constexpr int numThreads = 12;          // worker threads for the update
enum threadState {
	WORKING,                              // thread is engaged in computation
	WAITING,                              // thread has finished the work
	QUIT
};

enum edgeType {
	CHASSIS,                              // chassis member
	SUSPENSION,                           // suspension member
	SUSPENSION1                           // inboard suspension member
};

struct edge {
	edgeType type;                        // references global values of k, damping values
	float length, baseLength;             // current and initial edge length, used to determine compression / tension state
	int node1, node2;                     // indices the nodes on either end of the edge
};

struct face {
	int node1, node2, node3;              // the three points making up the triangle
	glm::vec3 normal;                     // surface normal for the triangle
};

struct node {
	float* mass;                          // pointer to mass of node ( easy runtime update )
	bool anchored;                        // anchored nodes are control points
	glm::vec3 position, oldPosition;      // current and previous position values
	glm::vec3 velocity, oldVelocity;      // current and previous velocity values
	std::vector< edge > edges;            // edges in which this node takes part
};

// consolidate simulation parameters
struct simParameterPack {
	bool  runSimulation       = true;     // toggle per frame update
	float timeScale           = 0.003f;   // amount of time that passes per sim tick
	float gravity             = -8.0f;    // scales the contribution of force of gravity

	float noiseAmplitudeScale = 0.065f;   // scalar on the noise amplitude
	float noiseSpeed          = 8.6f;     // how quickly the noise offset increases

	float chassisKConstant    = 14000.0f; // hooke's law spring constant for chassis edges
	float chassisDamping      = 51.5f;    // damping factor for chassis edges
	float chassisNodeMass     = 3.0f;     // mass of a chassis node
	float anchoredNodeMass    = 0.0f;     // mass of an anchored node

	float suspensionKConstant = 9000.0f;  // hooke's law spring constant for suspension edges
	float suspensionDamping   = 32.4f;    // damping factor for suspension edges
};

// consolidate display parameters
struct displayParameterPack {
	bool tensionColorOnly     = false;    // outlines are colored red in compression, blue in tension ( default )
	bool showChassisNodes     = true;     // show points associated with the chassis nodes
	bool showChassisEdges     = true;     // show lines associated with the chassis edges
	bool showSuspensionEdges  = true;     // show the lines associated with suspension edges
	bool showChassisFaces     = true;     // add this to the model, triangles using the same nodes as the edges

	float depthColorScale     = 1.0f;     // adjust the weight of the depth coloring
	float chassisRescaleAmnt  = 0.995f;   // scales the polygons, to interfere with the lines less

	float wheelDiameter       = 0.2f;    // offset from the noise read, per wheel

	glm::vec4 outlineColor    = softbodyColors::black;    // the highlight color if tensionColor is off
	glm::vec4 compColor       = softbodyColors::red;      // the highlight color of the edges in compression ( tensionColor mode )
	glm::vec4 tensColor       = softbodyColors::blue;     // the highlight color of the edges in tension ( tensionColor mode )

	glm::vec4 faceColor       = softbodyColors::green;    // color of the chassis faces
	glm::vec4 chassisColor    = softbodyColors::steel;    // color of the chassis members
	glm::vec4 suspColor       = softbodyColors::yellow;   // color of the suspension members
	glm::vec4 susp1Color      = softbodyColors::brown;    // color of the inboard suspension members

	glm::vec4 groundLow       = softbodyColors::G0;       // color of the ground at lowest point
	glm::vec4 groundHigh      = softbodyColors::G1;       // color of the ground at highest point
	glm::vec4 background      = softbodyColors::BG;       // OpenGL clear color

	float scale               = 0.4f;     // scales the frame points, about zero
};

struct drawParameterPack {
// VBO indexing
	// nodes
	GLuint nodesBase;
	GLuint nodesNum;
	// edges
	GLuint edgesBase;
	GLuint edgesNum;
	// faces
	GLuint facesBase;
	GLuint facesNum;
	// ground
	GLuint groundBase;
	GLuint groundNum;

	// lines scaling
	float lineScale           = 6.0f;
	float outlineRatio        = 1.3f;

	// point scaling
	float pointScale          = 16.0f;
};

class model {
public:
	model();
	~model();

	int nodeSelect = 0;

	// graph init
	void loadFramePoints();               // populate graph with nodes and edges
	void GPUSetup();                      // set up VAO, VBO, shaders

	// pass new GPU data
	void passNewGPUData();                // update vertex data
	void updateUniforms();                // update uniform variables

	// update functions for model
	void Update( /* threadID */ );        // single threaded update - add threadID for threaded update

	// show the model
	void Display();                       // render the latest vertex data with the simGeometryShader

	// to query sim completion

	void colorModeSelect( int mode );     // the set of drawing colors to use

	// simulation and display parameter structs
	simParameterPack simParameters;
	displayParameterPack displayParameters;
	drawParameterPack drawParameters;

	// OpenGL Data Handles
	GLuint simGeometryVAO;
	GLuint simGeometryVBO;
	GLuint simGeometryShader;
	GLuint bodyPanelShader;

	orientTrident * trident;

private:
	// called from loadFramePoints
	void addNode( float* mass, glm::vec3 position, bool anchored );
	void addEdge( int nodeIndex1, int nodeIndex2, edgeType type );
	void addFace( int nodeIndex1, int nodeIndex2, int nodeIndex3, glm::vec3 normal );

	// sim / display data
	std::vector< node > nodes;
	std::vector< edge > edges;
	std::vector< face > faces;

	// back up current values to previous values
	void CachePreviousValues();

	// keeping the state of each thread
	threadState workerState[ numThreads ];
	std::thread workerThreads[ numThreads ];
	void EnableAllWorkers();
	void MultiThreadUpdateFunc( int index );
	// std::function< void( int ) > MultiThreadUpdateFunc = []() ;


	// std::atomic<unsigned int> indexcrement{ 0 }; // used to get new thread ids
	// std::thread updateThreads[ numThreads ] {[=](){
	// 	// while ( workerState[ myThreadIndex ] != QUIT ) {
	// 	// 	if ( workerState[ myThreadIndex ] == WAITING )
	// 	// 		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	// 	// 	else {
	// 	// 		// run the update for all the relevant nodes
	// 	// 	}
	// 	// }
	// 	int index = indexcrement.fetch_add( 1 );
	// 	cout << "myindex is " << index << endl;
	// }};


	bool AllThreadComplete();

	// update all nodes with a single thread
	void SingleThreadSoftbodyUpdate();

	// ground data
	float getGroundPoint( float x, float y );
	FastNoise::SmartNode<> fnGenerator;
	float noiseOffset = 0.0;

};

#endif
