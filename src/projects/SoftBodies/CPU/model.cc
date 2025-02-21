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

	// spawn all the threads
	// for ( int i = 0; i < numThreads; i++ ) {
	// 	workerState[ i ] = WAITING;
	// 	workerThreads[ i ] = std::thread( &model::MultiThreadUpdateFunc, this, i );
	// }
}

model::~model() {
//   // quit and join all the threads
// 	for ( int i = 0; i < numThreads; i++ ) {
// 		workerState[ i ] = QUIT;
// 		workerThreads[ i ].join();
// 	}
}

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
	// mirrored SUSPENSION1 edges
	addEdge( 0, 13, SUSPENSION1 );
	addEdge( 0, 15, SUSPENSION1 );
	addEdge( 0, 17, SUSPENSION1 );
	addEdge( 0, 19, SUSPENSION1 );
	addEdge( 0, 20, SUSPENSION1 );
	addEdge( 0, 21, SUSPENSION1 );
	addEdge( 0, 22, SUSPENSION1 );
	addEdge( 0, 23, SUSPENSION1 );
	addEdge( 0, 24, SUSPENSION1 );

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
	// mirrored SUSPENSION1 edges
	addEdge( 1, 35, SUSPENSION1 );
	addEdge( 1, 25, SUSPENSION1 );
	addEdge( 1, 37, SUSPENSION1 );
	addEdge( 1, 39, SUSPENSION1 );
	addEdge( 1, 41, SUSPENSION1 );
	addEdge( 1, 42, SUSPENSION1 );
	addEdge( 1, 43, SUSPENSION1 );
	addEdge( 1, 44, SUSPENSION1 );
	addEdge( 1, 45, SUSPENSION1 );

	// back left
	addEdge( 2, 26, SUSPENSION );
	addEdge( 2, 28, SUSPENSION );
	addEdge( 2, 29, SUSPENSION );
	addEdge( 2, 30, SUSPENSION );
	addEdge( 2, 31, SUSPENSION );
	addEdge( 2, 32, SUSPENSION );
	addEdge( 2, 36, SUSPENSION );
	addEdge( 2, 38, SUSPENSION );
	// mirrored SUSPENSION1 edges
	addEdge( 2, 4, SUSPENSION1 );
	addEdge( 2, 6, SUSPENSION1 );
	addEdge( 2, 7, SUSPENSION1 );
	addEdge( 2, 8, SUSPENSION1 );
	addEdge( 2, 9, SUSPENSION1 );
	addEdge( 2, 10, SUSPENSION1 );
	addEdge( 2, 14, SUSPENSION1 );
	addEdge( 2, 16, SUSPENSION1 );

	// back right
	addEdge( 3, 4, SUSPENSION );
	addEdge( 3, 6, SUSPENSION );
	addEdge( 3, 7, SUSPENSION );
	addEdge( 3, 8, SUSPENSION );
	addEdge( 3, 9, SUSPENSION );
	addEdge( 3, 10, SUSPENSION );
	addEdge( 3, 14, SUSPENSION );
	addEdge( 3, 16, SUSPENSION );
	// mirrored SUSPENSION1 edges
	addEdge( 3, 26, SUSPENSION1 );
	addEdge( 3, 28, SUSPENSION1 );
	addEdge( 3, 29, SUSPENSION1 );
	addEdge( 3, 30, SUSPENSION1 );
	addEdge( 3, 31, SUSPENSION1 );
	addEdge( 3, 32, SUSPENSION1 );
	addEdge( 3, 36, SUSPENSION1 );
	addEdge( 3, 38, SUSPENSION1 );
}

void model::GPUSetup() {
	//VAO
	glGenVertexArrays( 1, &simGeometryVAO );
	glBindVertexArray( simGeometryVAO );

	//BUFFER
	glGenBuffers( 1, &simGeometryVBO );
	glBindBuffer( GL_ARRAY_BUFFER, simGeometryVBO );
}

float model::getGroundPoint( float x, float y ) {
	return fnGenerator->GenSingle2D( x * displayParameters.scale, y * displayParameters.scale + noiseOffset, 42069 ) * simParameters.noiseAmplitudeScale - 0.4;
}

void model::passNewGPUData() {
	// populate the arrays out of the edge and node data
	std::vector< glm::vec4 > points;
	std::vector< glm::vec4 > colors;
	std::vector< glm::vec4 > tColors;

	// chassis nodes
	drawParameters.nodesBase = points.size();
	for ( auto n : nodes )
		if ( displayParameters.showChassisNodes ) {
			points.push_back( glm::vec4( n.position * displayParameters.scale, 10.0 ) ),
			colors.push_back( softbodyColors::steel ),
			tColors.push_back( glm::vec4( 0. ) );
		}

	// the ground nodes - this could be done more efficiently, but whatever
	for ( float x = -1.0; x < 1.0; x += 0.01 ) {
		for ( float y = -1.5; y < 1.5; y += 0.01 ) {
			float groundHeight = getGroundPoint( x / displayParameters.scale, y / displayParameters.scale );
			points.push_back( glm::vec4( glm::vec3( x, groundHeight, y ), ( -groundHeight + 1.3 ) * 15.0f ) );
			glm::vec4 sampleColor = 4.0f * groundHeight * displayParameters.groundHigh + ( 1.0f -  4.0f * groundHeight ) * displayParameters.groundLow;
			// glm::vec4 sampleColor = glm::vec4( ( x + 1.0f ) / 2.0f, ( y + 1.5f ) / 3.0f, 0.0f, 1.0f );
			colors.push_back( glm::vec4( sampleColor.xyz(), 1.0f ) );
			tColors.push_back( glm::vec4( 0.0f ) );
		}
	}

	// end of points
	drawParameters.nodesNum = points.size() - drawParameters.nodesBase;

	// edges
	drawParameters.edgesBase = points.size();
	for ( auto e : edges ) {
		points.push_back( glm::vec4( nodes[ e.node1 ].position * displayParameters.scale, 10.0 ) );
		points.push_back( glm::vec4( nodes[ e.node2 ].position * displayParameters.scale, 10.0 ) );
		switch ( e.type ) {
			case CHASSIS:
			colors.push_back( displayParameters.chassisColor );
			colors.push_back( displayParameters.chassisColor );
			break;
			case SUSPENSION:
			colors.push_back( displayParameters.suspColor );
			colors.push_back( displayParameters.suspColor );
			break;
			case SUSPENSION1:
			colors.push_back( displayParameters.susp1Color );
			colors.push_back( displayParameters.susp1Color );
			break;
			default: break;
		}
		tColors.push_back( softbodyColors::black ); // this will become a mapping that involves length and baselength
		tColors.push_back( softbodyColors::black );   // for the edge as well as the compColor and tensColor
	}
	drawParameters.edgesNum = points.size() - drawParameters.edgesBase;

	// faces
	drawParameters.facesBase = points.size();
	for ( auto f : faces ) {
		// bring it in a touch, less collision with the chassis edges
		points.push_back( glm::vec4( nodes[ f.node1 ].position * displayParameters.scale * displayParameters.chassisRescaleAmnt, 10.0 ) );
		points.push_back( glm::vec4( nodes[ f.node2 ].position * displayParameters.scale * displayParameters.chassisRescaleAmnt, 10.0 ) );
		points.push_back( glm::vec4( nodes[ f.node3 ].position * displayParameters.scale * displayParameters.chassisRescaleAmnt, 10.0 ) );

		colors.push_back( displayParameters.faceColor );
		colors.push_back( displayParameters.faceColor );
		colors.push_back( displayParameters.faceColor );

		// calculate normal
		glm::vec4 normal = glm::vec4( glm::normalize( glm::cross( nodes[ f.node1 ].position - nodes[ f.node2 ].position, nodes[ f.node1 ].position - nodes[ f.node3 ].position ) ), 1.0 );

		tColors.push_back( normal );
		tColors.push_back( normal );
		tColors.push_back( normal );
	}
	drawParameters.facesNum = points.size() - drawParameters.facesBase;


	// buffer the data to the GPU
	uintptr_t numBytesPoints  =  points.size() * sizeof( glm::vec4 );
	uintptr_t numBytesColors  =  colors.size() * sizeof( glm::vec4 );
	uintptr_t numBytesTColors = tColors.size() * sizeof( glm::vec4 );

	// send it
	glBufferData(GL_ARRAY_BUFFER, numBytesPoints + numBytesColors + numBytesTColors, NULL, GL_DYNAMIC_DRAW);
	uint32_t bufferbase = 0;
	glBufferSubData(GL_ARRAY_BUFFER, bufferbase, numBytesPoints, &points[0]);
	bufferbase += numBytesPoints;
	glBufferSubData(GL_ARRAY_BUFFER, bufferbase, numBytesColors, &colors[0]);
	bufferbase += numBytesColors;
	glBufferSubData(GL_ARRAY_BUFFER, bufferbase, numBytesTColors, &tColors[0]);


	// set up the pointers to the vertex data
	GLvoid* base = 0;
	glEnableVertexAttribArray( glGetAttribLocation( simGeometryShader, "vPosition" ));
	glVertexAttribPointer( glGetAttribLocation( simGeometryShader, "vPosition" ), 4, GL_FLOAT, GL_FALSE, 0, base );

	base = ( GLvoid* ) numBytesPoints;
	glEnableVertexAttribArray( glGetAttribLocation( simGeometryShader, "vColor" ));
	glVertexAttribPointer( glGetAttribLocation( simGeometryShader, "vColor" ), 4, GL_FLOAT, GL_FALSE, 0, base );

	base = ( GLvoid* ) ( numBytesPoints + numBytesColors );
	glEnableVertexAttribArray( glGetAttribLocation( simGeometryShader, "vtColor" ));
	glVertexAttribPointer( glGetAttribLocation( simGeometryShader, "vtColor" ), 4, GL_FLOAT, GL_FALSE, 0, base );
}

void model::colorModeSelect( int mode ) {
	glUniform1i( glGetUniformLocation( simGeometryShader, "colorMode" ), mode );
}

void model::updateUniforms() {
	int numDisplays = 0;
	SDL_DisplayID* displays;
	displays = SDL_GetDisplays( &numDisplays );
	// making an assumption of uniform display resolution (ha ha ha)
	const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode( displays[ 0 ] );

	const float AR = float( dm->w ) / float( dm->h );
	const glm::mat4 proj = glm::perspective( glm::radians( 65.0f ), AR, -1.0f, 3.0f );

	glUniformMatrix4fv( glGetUniformLocation( simGeometryShader, "perspective" ), 1, GL_TRUE, glm::value_ptr( proj ) );
	glUniform1fv( glGetUniformLocation( simGeometryShader, "aspect_ratio" ), 1, &AR );

	// point size and point highlight
	glUniform1fv( glGetUniformLocation( simGeometryShader, "defaultPointSize" ), 1, &drawParameters.pointScale );
	glUniform1i( glGetUniformLocation( simGeometryShader, "nodeSelect" ), nodeSelect );

	glm::mat3 orientationMatrix;
	orientationMatrix[ 0 ] = trident->basisX;
	orientationMatrix[ 1 ] = trident->basisY;
	orientationMatrix[ 2 ] = trident->basisZ;
	glUniformMatrix3fv( glGetUniformLocation( simGeometryShader, "orientation" ), 1, GL_FALSE, glm::value_ptr( orientationMatrix ) );

	const float t = 0.001f * SDL_GetTicks();
	glUniform3f( glGetUniformLocation( simGeometryShader, "lightPos" ), 0.75 * cos( t ), 0.5, 0.75 * sin( t ) );

	// if ( ++nodeSelect == 4 ) nodeSelect = 0;
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
					case SUSPENSION1:
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

void model::MultiThreadUpdateFunc ( int myThreadIndex ) {
	// cout << "id is " << myThreadIndex << endl;
	while ( workerState[ myThreadIndex ] != QUIT ) {
		if ( workerState[ myThreadIndex ] == WAITING )
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		else if ( workerState[ myThreadIndex ] == WORKING ) {
			// run the update for all the relevant nodes
			for ( unsigned int n = myThreadIndex; n < nodes.size(); n += numThreads ) {
				cout << "thread index " << myThreadIndex << " is updating node " << n << endl;
				if ( !nodes[ n ].anchored ) {
					glm::vec3 forceAccumulator = glm::vec3( 0, 0, 0 );
					float k = 0;
					float d = 0;
					//get your forces from all the connections - accumulate in forceAccumulator vector
					for ( auto& e : nodes[ n ].edges ) {
						switch ( e.type ) {
							case CHASSIS:
							k = simParameters.chassisKConstant;
							d = simParameters.chassisDamping;
							break;
							case SUSPENSION:
							case SUSPENSION1:
							k = simParameters.suspensionKConstant;
							d = simParameters.suspensionDamping;
							break;
						}
						//get positions of the two nodes involved
						glm::vec3 myPosition = nodes[ n ].oldPosition;
						glm::vec3 otherPosition = nodes[ e.node2 ].anchored ?
						nodes[ e.node2 ].position :    // use new position for anchored nodes ( position is up to date )
						nodes[ e.node2 ].oldPosition;  // use old position for unanchored nodes ( old value is what you use )

						//less than 1 is shorter, greater than 1 is longer than base length
						float springRatio = glm::distance( myPosition, otherPosition ) / e.baseLength;
						forceAccumulator += -k * glm::normalize( myPosition - otherPosition ) * ( springRatio - 1 ); // spring force
						forceAccumulator -= d * nodes[ n ].oldVelocity;                           //damping force
					}
					forceAccumulator += ( *nodes[ n ].mass ) * glm::vec3( 0.0f, -simParameters.gravity, 0.0f ); // add gravity
					glm::vec3 acceleration = forceAccumulator / ( *nodes[ n ].mass );                           // get the resulting acceleration
					nodes[ n ].velocity = nodes[ n ].oldVelocity + acceleration * simParameters.timeScale;    // compute the new velocity
					nodes[ n ].position = nodes[ n ].oldPosition + nodes[ n ].velocity * simParameters.timeScale;      // get the new position
				}
			}
			cout << "setting " << myThreadIndex << " to wait" << endl;
			workerState[ myThreadIndex ] = WAITING;
		}
	}
}

bool model::AllThreadComplete () {
	bool check = true;
	for ( int i = 0; i < numThreads; i++ ) {
		if ( workerState[ i ] == WORKING ) {
			check = false;
			cout << "thread " << i << " is still working" << endl;
		}
	}
	return check;
}

void model::EnableAllWorkers () {
	for( int i = 0; i < numThreads; i++ ) {
		workerState[ i ] = WORKING;
	}
}

void model::CachePreviousValues () {
	for ( auto& n : nodes )
		if ( !n.anchored ) {
			n.oldPosition = n.position;
			n.oldVelocity = n.velocity;
		}
}

void model::Update () {
	// offset the noise over time
	noiseOffset += 0.001f * simParameters.noiseSpeed;

	// sample terrain surface height at the wheel points
	nodes[ 0 ].position.y = getGroundPoint( nodes[ 0 ].position.x, nodes[ 0 ].position.z ) / displayParameters.scale + displayParameters.wheelDiameter;
	nodes[ 1 ].position.y = getGroundPoint( nodes[ 1 ].position.x, nodes[ 1 ].position.z ) / displayParameters.scale + displayParameters.wheelDiameter;
	nodes[ 2 ].position.y = getGroundPoint( nodes[ 2 ].position.x, nodes[ 2 ].position.z ) / displayParameters.scale + displayParameters.wheelDiameter;
	nodes[ 3 ].position.y = getGroundPoint( nodes[ 3 ].position.x, nodes[ 3 ].position.z ) / displayParameters.scale + displayParameters.wheelDiameter;

	// back up velocities and positions in the 'old' values

	// single threaded update structure
	// auto tstart = std::chrono::high_resolution_clock::now();
	// for ( int i = 0; i < 10; i++ ){
		CachePreviousValues();
		SingleThreadSoftbodyUpdate();
	// }
	// cout << "singlethread update (x10) " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-tstart).count() << "ns\n";

	// // multithreaded update structure
	// auto tstartm = std::chrono::high_resolution_clock::now();
	// // for ( int i = 0; i < 10; i++ ){
	// 	CachePreviousValues();
	// 	EnableAllWorkers();							// set worker thread enable flag
	// 	while( !AllThreadComplete() );	// wait for all threads to reach completion
	// // }
	// cout << "multithread update " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-tstartm).count() << "ns\n";

	// pass the new GPU data
	passNewGPUData();
}

void model::Display() {
	// OpenGL config
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	glEnable( GL_PROGRAM_POINT_SIZE ); // lets you set pointsize in the shader - for suspension point select

	// use the first shader / VAO / VBO to draw the lines, points
	glUseProgram( simGeometryShader );
	glBindVertexArray( simGeometryVAO );
	glBindBuffer( GL_ARRAY_BUFFER, simGeometryVBO );

	updateUniforms();

	// chassis nodes
	if ( displayParameters.showChassisNodes ) {
		colorModeSelect( 0 );
		glDrawArrays( GL_POINTS, drawParameters.nodesBase, drawParameters.nodesNum );
	}

	// body panels
	if ( displayParameters.showChassisFaces ) {
		colorModeSelect( 3 );
		glDrawArrays( GL_TRIANGLES, drawParameters.facesBase, drawParameters.facesNum );
	}

	if ( displayParameters.showChassisEdges ) {
		if ( !displayParameters.tensionColorOnly ) {
			// regular colors - this is the chassis segments
			colorModeSelect( 1 );
			glLineWidth( drawParameters.lineScale );
			glDrawArrays( GL_LINES, drawParameters.edgesBase, drawParameters.edgesNum );
		}

		// tension colors / softbodyColors::black outlines
		colorModeSelect( 2 );
		glLineWidth( drawParameters.outlineRatio * drawParameters.lineScale );
		glDrawArrays( GL_LINES, drawParameters.edgesBase, drawParameters.edgesNum );
	}

	// use the other shader / VAO / VBO to do flat shaded polygons for the body panels
}

void model::addNode( float* mass, glm::vec3 position, bool anchored ) {
	node n;
	n.mass = mass;
	n.anchored = anchored;
	n.position = n.oldPosition = position;
	n.velocity = n.oldVelocity = glm::vec3( 0.0 );
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
