cmake_minimum_required (VERSION 3.16)
project( jbDE VERSION 1.2 LANGUAGES C CXX )

set( CMAKE_VERBOSE_MAKEFILE ON )

set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED True )

# platform specific compiler flags
if(UNIX)
	set( CMAKE_CXX_FLAGS_DEBUG "-g" )
	set( CMAKE_CXX_FLAGS_RELEASE "-O4" )
elseif(WIN32)
	set( CMAKE_CXX_FLAGS_RELEASE "/O2" )
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi" )
	set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF" )
endif()

# I think opening the console is breaking steam deck usage... not sure how to disable
# set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS /SUBSYSTEM:WINDOWS )
# set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS /SUBSYSTEM:CONSOLE )

set( CMAKE_BUILD_TYPE Release )

add_definitions( "-DCMAKE_BUILD_TYPE=Release" )
add_definitions( "-DCMAKE_POSITION_INDEPENDENT_CODE=ON" )

# Define the output directories for different build types
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin" )

# SDL
add_subdirectory( ${PROJECT_SOURCE_DIR}/thirdparty/SDL3.2.4/ )
add_library( sdl3 INTERFACE )
target_include_directories( sdl3 INTERFACE "${PROJECT_SOURCE_DIR}/thirdparty/SDL3.2.4/include/" )
target_link_libraries( sdl3 INTERFACE SDL3::SDL3 )
# target_link_libraries( sdl3 PUBLIC SDL3::SDL3-static ) # would like to figure out static linking

# OpenGL (GLAD)
add_subdirectory( ${PROJECT_SOURCE_DIR}/thirdparty/glad )
add_library( gl INTERFACE )
target_include_directories( gl INTERFACE ${PROJECT_SOURCE_DIR}/thirdparty/glad/include/glad )
target_link_libraries( gl INTERFACE glad )

# GLM (vector/math library)
add_library( glm INTERFACE )
target_include_directories( glm INTERFACE "${PROJECT_SOURCE_DIR}/thirdparty/glm" )

# FastNoise2
add_subdirectory( ${PROJECT_SOURCE_DIR}/src/utils/noise/FastNoise2 )

# this is for ImGUI
add_library( imgui STATIC
	src/utils/ImGUI/imgui_impl_sdl3.cpp
	src/utils/ImGUI/imgui_impl_opengl3.cpp
	src/utils/ImGUI/imgui.cpp
	src/utils/ImGUI/imgui_demo.cpp
	src/utils/ImGUI/imgui_draw.cpp
	src/utils/ImGUI/imgui_widgets.cpp
	src/utils/ImGUI/imgui_tables.cpp
	src/utils/ImGUI/TextEditor/TextEditor.cc
	src/utils/ImGUI/TextEditor/LanguageDefinitions.cc
)
target_include_directories( imgui PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/SDL3.2.4/include )
target_link_libraries( imgui PRIVATE sdl3 gl )

# Tracy Profiler
# option( TRACY_ENABLE "" ON )
# option( TRACY_ON_DEMAND "" ON )
# add_subdirectory( src/utils/tracy )
# todo: adding profiler
# add_library( Tracy src/utils/tracy/public/TracyClient.cpp )

# FFTW3 FFT library
add_subdirectory( ${PROJECT_SOURCE_DIR}/src/utils/fftw-3.3.10 )

# Simple Autocomplete Utility
add_library( autocomplete STATIC src/utils/autocomplete/DictionaryTrie.cpp )

# TinyBVH
add_library( tinyBVH STATIC src/utils/tinyBVH_impl.cc )

# TinyXML XML serdes
add_library( tinyXML2 STATIC src/utils/Serialization/tinyXML2/tinyxml2.cc )

# yaml-cpp
add_subdirectory( ${PROJECT_SOURCE_DIR}/src/utils/Serialization/yaml-cpp )

# Image Libs
add_library( tinyEXR STATIC src/utils/imageLibs/tinyEXR/tinyexr.cc src/utils/imageLibs/tinyEXR/miniz/miniz.c )
add_library( STB_ImageUtilsWrapper STATIC src/utils/imageLibs/stb/impl.cc )
add_library( LodePNG STATIC src/utils/imageLibs/LodePNG/lodepng.cc )

# TinyOBJLoader OBJ model loading
add_library( TinyOBJLoader STATIC src/utils/ModelLoading/TinyOBJLoader/tiny_obj_loader.cc )

# basic perlin noise implementation
add_library( Perlin STATIC src/utils/noise/perlin.cc )

# this is the bigint library to support the VAT code
add_library( BigInt STATIC
	src/utils/noise/VAT/BigInt/BigUnsigned.cc
	src/utils/noise/VAT/BigInt/BigInteger.cc
	src/utils/noise/VAT/BigInt/BigIntegerAlgorithms.cc
	src/utils/noise/VAT/BigInt/BigUnsignedInABase.cc
	src/utils/noise/VAT/BigInt/BigIntegerUtils.cc
)
# target_compile_options( BigInt PUBLIC -Wno-deprecated )

# makes including the 'includes.h', 'engine.h' simpler
add_library( engineIncludes INTERFACE )
target_include_directories( engineIncludes INTERFACE "${CMAKE_SOURCE_DIR}/src/engine/" )
target_include_directories( engineIncludes INTERFACE "${CMAKE_SOURCE_DIR}/src/data/" )
target_include_directories( engineIncludes INTERFACE "${CMAKE_SOURCE_DIR}/src/utils/" )

# bringing everything together
add_library( ENGINE_LIBS INTERFACE )
target_link_libraries(
	ENGINE_LIBS
	INTERFACE
	engineIncludes
	sdl3
	glm
	gl
	fftw3
	imgui
	BigInt
	tinyEXR
	tinyXML2
	yaml-cpp
	autocomplete
	tinyBVH
	STB_ImageUtilsWrapper
	FastNoise
	fftw3
	Perlin
	# Tracy::TracyClient
	TinyOBJLoader
	LodePNG
)

# somebody's stepping on this
set( CMAKE_BUILD_TYPE RELEASE )

# base engine functionality
file ( GLOB ENGINE_BASE
	src/engine/engine.cc
	src/engine/engineUtils.cc
	src/engine/engineInit.cc
	src/engine/engineImguiUtils.cc
)

# compile once and link to each target - this is less redundant
add_library( engineBase STATIC
	${ENGINE_BASE}
)
target_link_libraries( engineBase
	PUBLIC
	ENGINE_LIBS
)

# target building
# add_executable ( Demo "src/projects/EngineDemo/main_old.cpp" )
# target_link_libraries( Demo PUBLIC ENGINE_LIBS )

add_executable( Demo
	src/projects/EngineDemo/main.cc
)

target_link_libraries( Demo
	PUBLIC
	engineBase
)


# =================================================================================================
# Space Game
# =================================================================================================
add_executable( SpaceGame
	src/projects/SpaceGame/main.cc
)

target_link_libraries( SpaceGame
	PUBLIC
	engineBase
)

# =================================================================================================
# Siren
# =================================================================================================
add_executable( Siren
	src/projects/PathTracing/Siren/main.cc
)

target_link_libraries( Siren
	PUBLIC
	engineBase
)

# =================================================================================================
# Daedalus
# =================================================================================================
add_executable( Daedalus
	src/projects/PathTracing/Daedalus/main.cc
	src/projects/PathTracing/Daedalus/daedalusData.cc
	src/projects/PathTracing/Daedalus/daedalusImgui.cc
)

target_link_libraries( Daedalus
	PUBLIC
	engineBase
)

# =================================================================================================
# realtime caustics experiment
# =================================================================================================
add_executable( Caustic
	src/projects/PathTracing/Caustic/main.cc
)

target_link_libraries( Caustic
	PUBLIC
	engineBase
)

# =================================================================================================
# Icarus
# =================================================================================================
add_executable( Icarus
	src/projects/PathTracing/Icarus/main.cc
)

target_link_libraries( Icarus
	PUBLIC
	engineBase
)

# =================================================================================================
# BVH construction/traversal experiment based on https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/
# =================================================================================================
add_executable( BVHtest
	src/projects/PathTracing/BVHtest/main.cc
)

target_link_libraries( BVHtest
	PUBLIC
	engineBase
)

# =================================================================================================
# Cellular Automata (1D)
# =================================================================================================
add_executable( CA1D
	src/projects/CellularAutomata/1D/main.cc
)

target_link_libraries( CA1D
	PUBLIC
	engineBase
)


# =================================================================================================
# Cellular Automata with History
# =================================================================================================
add_executable( CAHistory
	src/projects/CellularAutomata/CAHistory/main.cc
)

target_link_libraries( CAHistory
	PUBLIC
	engineBase
)

# =================================================================================================
# Same thing but visualizing in 3d
# =================================================================================================
add_executable( CAHistory3D
	src/projects/CellularAutomata/CAHistory3D/main.cc
)

target_link_libraries( CAHistory3D
	PUBLIC
	engineBase
)

# =================================================================================================
# Cellular Automata with History ( split colors )
# =================================================================================================
add_executable( CAColorSplit
src/projects/CellularAutomata/colorSplit/main.cc
)

target_link_libraries( CAColorSplit
PUBLIC
engineBase
)

# =================================================================================================
add_executable( CATable
	src/projects/CellularAutomata/table/main.cc
)

target_link_libraries( CATable
	PUBLIC
	engineBase
)

# =================================================================================================
# Basic Game of Life sim
# =================================================================================================
add_executable( GoL
	src/projects/CellularAutomata/GoL/main.cc
)

target_link_libraries( GoL
	PUBLIC
	engineBase
)

# =================================================================================================
# Cellular Automata in Parallel Bit Planes
# =================================================================================================
add_executable( CABitPlanes
	src/projects/CellularAutomata/bitPlanes/main.cc
)

target_link_libraries( CABitPlanes
	PUBLIC
	engineBase
)

# =================================================================================================
# Aquaria
# =================================================================================================
add_executable( Aquaria
	src/projects/Aquaria/main.cc
)

target_link_libraries( Aquaria
	PUBLIC
	engineBase
)

# =================================================================================================
# CellarDoor
# =================================================================================================
add_executable( CellarDoor
	src/projects/PathTracing/CellarDoor/main.cc
)

target_link_libraries( CellarDoor
	PUBLIC
	engineBase
)

# =================================================================================================
# Physarum ( 2D )
# =================================================================================================
add_executable( Physarum2D
	src/projects/Physarum/2D/main.cc
)

target_link_libraries( Physarum2D
	PUBLIC
	engineBase
)

# =================================================================================================
# Physarum ( 2.5D )
# =================================================================================================
add_executable( Physarum2.5D
	src/projects/Physarum/2.5D/main.cc
)

target_link_libraries( Physarum2.5D
	PUBLIC
	engineBase
)

# =================================================================================================
# PhysarumEmbossed
# =================================================================================================
add_executable( PhysarumEmbossed
	src/projects/Physarum/Embossed/main.cc
)

target_link_libraries( PhysarumEmbossed
	PUBLIC
	engineBase
)

# =================================================================================================
# Physarum ( 3D )
# =================================================================================================
add_executable( Physarum3D
	src/projects/Physarum/3D/main.cc
)

target_link_libraries( Physarum3D
	PUBLIC
	engineBase
)




# =================================================================================================
# VoxelSpace
# =================================================================================================
add_executable( VoxelSpace
	src/projects/VoxelSpace/CommancheMaps/main.cc
)

target_link_libraries( VoxelSpace
	PUBLIC
	engineBase
)

# =================================================================================================
# VoxelSpace_Erode
# =================================================================================================
add_executable( VoxelSpace_Erode
	src/projects/VoxelSpace/Erosion/main.cc
)

target_link_libraries( VoxelSpace_Erode
	PUBLIC
	engineBase
)

# =================================================================================================
# VoxelSpace_Physarum
# =================================================================================================
add_executable( VoxelSpace_Physarum
	src/projects/VoxelSpace/Physarum/main.cc
)

target_link_libraries( VoxelSpace_Physarum
	PUBLIC
	engineBase
)

# =================================================================================================
# SoftBodies
# =================================================================================================
add_executable( SoftBodies
	src/projects/SoftBodies/CPU/main.cc
	src/projects/SoftBodies/CPU/model.cc
)

target_link_libraries( SoftBodies
	PUBLIC
	engineBase
)

# =================================================================================================
# =================================================================================================
# Point Sprite Experiments
# =================================================================================================

# =================================================================================================
# ProjectedFramebuffers
# =================================================================================================
add_executable( ProjectedFramebuffers
	src/projects/Impostors/ProjectedFramebuffers/main.cc
)

target_link_libraries( ProjectedFramebuffers
	PUBLIC
	engineBase
)

# =================================================================================================
# Vertexture
# =================================================================================================
add_executable( Vertexture
	src/projects/Impostors/Vertexture/main.cc
)

target_link_libraries( Vertexture
	PUBLIC
	engineBase
)

# =================================================================================================
# VertextureClassic
# =================================================================================================
add_executable( VertextureClassic
	src/projects/Impostors/VertextureClassic/main.cc
)

target_link_libraries( VertextureClassic
	PUBLIC
	engineBase
)

# =================================================================================================
# Chorizo
# =================================================================================================
add_executable( Chorizo
	src/projects/Impostors/Chorizo/main.cc
)

target_link_libraries( Chorizo
	PUBLIC
	engineBase
)

# =================================================================================================
# Random L-system generator
# =================================================================================================
add_executable( ChorizoForest
	src/projects/Impostors/ChorizoForest/main.cc
)

target_link_libraries( ChorizoForest
	PUBLIC
	engineBase
)

# =================================================================================================
# SoftBodies (CPU) using the Chorizo renderer
# =================================================================================================
add_executable( ChorizoSoftbody
	src/projects/Impostors/ChorizoSoftbody/main.cc
	src/projects/Impostors/ChorizoSoftbody/model.cc
)

target_link_libraries( ChorizoSoftbody
	PUBLIC
	engineBase
)

# =================================================================================================
# Trying to implement slow-scan television (SSTV)
# =================================================================================================
add_executable( Spectrogram
	src/projects/SignalProcessing/Spectrogram/main.cc
)

target_link_libraries( Spectrogram
	PUBLIC
	engineBase
)

# =================================================================================================
# Messing with 2D IFS
# https://flam3.com/flame_draves.pdf
# https://www.cs.uaf.edu/~olawlor/papers/2011/ifs/lawlor_ifs_2011.pdf
# =================================================================================================
add_executable( ifs2D
	src/projects/IFS/main.cc
)

target_link_libraries( ifs2D
	PUBLIC
	engineBase
)

# =================================================================================================
# Looking at relationships between neighboring bytes
# =================================================================================================
add_executable( CantorDust
	src/projects/SignalProcessing/CantorDust/main.cc
)

target_link_libraries( CantorDust
	PUBLIC
	engineBase
)

# =================================================================================================
# Neural net stuff for identifying handwritten characters from the MNIST dataset
# =================================================================================================
add_executable( MNIST
	src/projects/SignalProcessing/MNIST/main.cc
)

target_link_libraries( MNIST
	PUBLIC
	engineBase
)

# =================================================================================================
# generalizing Adam7 to arbitrary size neighborhoods
# =================================================================================================
add_executable( Adam
	src/projects/SignalProcessing/Adam/main.cc
)

target_link_libraries( Adam
	PUBLIC
	engineBase
)

# =================================================================================================
# Trying to copy DynaDraw: http://www.graficaobscura.com/dyna/
# =================================================================================================
add_executable( jbPaint
	src/projects/SignalProcessing/jbPaint/main.cc
)

target_link_libraries( jbPaint
	PUBLIC
	engineBase
)

# =================================================================================================
# Prototype line renderer
# =================================================================================================
add_executable( LineSpam
	src/projects/LineSpam/main.cc
)

target_link_libraries( LineSpam
	PUBLIC
	engineBase
)

# =================================================================================================
# Grass rendering
# =================================================================================================
add_executable( Raqia
	src/projects/Raqia/main.cc
)

target_link_libraries( Raqia
	PUBLIC
	engineBase
)

# =================================================================================================
# Voraldo13 port - RGBA direct volume graphics editor and renderer with various lighting utilities
# =================================================================================================
add_executable( Voraldo13
	src/projects/Voraldo13/main.cc
	src/projects/Voraldo13/voraldoImguiUtils.cc
	src/projects/Voraldo13/voraldoUtils.cc
)

target_link_libraries( Voraldo13
	PUBLIC
	engineBase
)
