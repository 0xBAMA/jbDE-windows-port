#include "../../engine/engine.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "game.h"

class SpaceGame final : public engineBase {
public:
	SpaceGame () { config.forceResolution = ivec2( 1280, 720 ); Init(); OnInit(); PostInit(); }
	~SpaceGame () { Quit(); }

	universeController controller;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );
			tonemap.tonemapMode = 0;

			CompileShaders();

			// force nearest filtering on the display texture, regardless of config setting
			// textureManager.SetFilterMinMag( "Display Texture", GL_NEAREST, GL_NEAREST );

			// some initialization tasks that have to be done after OpenGL init
			controller.init( &textureManager, &inputHandler );

			// make sure that the atlasManager gets created and setup
			controller.atlas = new( AtlasManager );
			controller.atlas->textureManager = &textureManager;

			// pass in required handles for the line drawing
			controller.lines.Init( shaders[ "Line Draw" ], shaders[ "Line Draw Composite" ], shaders[ "Line Clear" ], textureManager, config.width, config.height );

			// load up the existing ship textures
			textureOptions_t opts;
			opts.dataType = GL_RGBA8;
			opts.textureType = GL_TEXTURE_2D;
			opts.minFilter = GL_LINEAR;
			opts.magFilter = GL_LINEAR;
			opts.borderColor = { 0.0f, 0.0f, 0.0f, 0.0f };

			// and the tiny font table (partial coverage: 3x3 alphas, 3x7 numerals... 3x5 punctuation, mostly, basically vertically centered - spaces between letters need to be added manually, so glyphs need to be 4 pixels wide in practice)
			Image_4U tinyFont( "../src/utils/fonts/fontRenderer/tinyFontPartial.png" );
			tinyFont.FlipVertical();
			opts.width = tinyFont.Width();
			opts.height = tinyFont.Height();
			opts.initialData = tinyFont.GetImageDataBasePtr();
			opts.minFilter = GL_NEAREST;
			opts.magFilter = GL_NEAREST;
			opts.dataType = GL_RGBA8UI;
			opts.pixelDataType = GL_UNSIGNED_BYTE;
			textureManager.Add( "TinyFont", opts );

			// add the intermediate value buffer for the line renderer
			opts.width = config.width;
			opts.height = config.height;
			opts.dataType = GL_R32UI;
			opts.initialData = nullptr;
			textureManager.Add( "Line Draw Buffer", opts );

			SDL_ShowCursor();

			// todo: console commands for setting position, etc on player controller

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		if ( inputHandler.getState4( KEY_Y ) == KEYSTATE_RISING ) {
			CompileShaders();
			logHighPriority( "Shaders Recompiled" );
		}

		if ( inputHandler.getState4( KEY_P ) == KEYSTATE_RISING ) {
			controller.lines.RandomizeColors();
		}
	}

	void CompileShaders () {
		shaders[ "Background Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/draw.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Background Draw" ], -1, string( "Background Draw" ).c_str() );

		shaders[ "BVH Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/bvh.cs.glsl" ).shaderHandle; // eventually atlas the textures etc to facilitate batching... not super important right now
		glObjectLabel( GL_PROGRAM, shaders[ "BVH Draw" ], -1, string( "BVH Draw" ).c_str() );

		controller.drawShader = shaders[ "Text Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/text.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Text Draw" ], -1, string( "Text Draw" ).c_str() );

		// similar structure to the existing text renderer... drawing to an intermediate buffer... then blending with the contents of the buffer, in passes
		shaders[ "Line Draw" ] = computeShader( "../src/projects/SpaceGame/shaders/lineDraw.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Line Draw" ], -1, string( "Line Draw" ).c_str() );

		shaders[ "Line Draw Composite" ] = computeShader( "../src/projects/SpaceGame/shaders/lineComposite.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Line Draw Composite" ], -1, string( "Line Draw Composite" ).c_str() );

		shaders[ "Line Clear" ] = computeShader( "../src/projects/SpaceGame/shaders/lineClear.cs.glsl" ).shaderHandle;
		glObjectLabel( GL_PROGRAM, shaders[ "Line Clear" ], -1, string( "Line Clear" ).c_str() );
	}

	void ImguiPass () {
		ZoneScoped;

		// doing a window to visualize the state of the sector resources...
		ImGui::Begin( "Sector Debug" );

		// show the atlas
		ImGui::Image( ( ImTextureID ) ( void * ) intptr_t( textureManager.Get( "AtlasTexture" ) ), ImVec2( 256, 256 ) );

		// show a table of entity information
		static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (ImGui::BeginTable("entityTable", 8, flags ) ) {
			ImGui::TableSetupColumn( "Type" );
			ImGui::TableSetupColumn( "Atlas x" );
			ImGui::TableSetupColumn( "Atlas y" );
			ImGui::TableSetupColumn( "Atlas w" );
			ImGui::TableSetupColumn( "Atlas h" );
			ImGui::TableSetupColumn( "Sector X" );
			ImGui::TableSetupColumn( "Sector Y" );
			ImGui::TableSetupColumn( "Scale" );
			ImGui::TableHeadersRow();

			// report the type, atlas x, y, w, h, location x, y for each entity
			for ( auto& entity : controller.entityList ) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex( 0 );
				switch ( entity.type ) {	// Type label
				case PLAYER:	ImGui::Text( "PLAYER" );	break;
				case ASTEROID:	ImGui::Text( "ASTEROID" );	break;
				case STATION:	ImGui::Text( "STATION" );	break;
				case FRIEND:	ImGui::Text( "FRIEND" );	break;
				case FOE:		ImGui::Text( "FOE" );		break;}

				// Atlas info
				ImGui::TableSetColumnIndex( 1 );
				ImGui::PushItemWidth( 100 );
				ImGui::Text( "%d", controller.atlas->entityRegions[ entity.atlasIndex ][ 0 ] );
				ImGui::TableSetColumnIndex( 2 );
				ImGui::Text( "%d", controller.atlas->entityRegions[ entity.atlasIndex ][ 1 ] );
				ImGui::TableSetColumnIndex( 3 );
				ImGui::Text( "%d", controller.atlas->entityRegions[ entity.atlasIndex ][ 2 ] );
				ImGui::TableSetColumnIndex( 4 );
				ImGui::Text( "%d", controller.atlas->entityRegions[ entity.atlasIndex ][ 3 ] );

				ImGui::TableSetColumnIndex( 5 );
				ImGui::Text( "%s", fixedWidthNumberString( int( RangeRemap( entity.position.x - floor( entity.position.x ), 0.0f, 1.0f, -controller.sectorSize / 2.0f, controller.sectorSize / 2.0f ) ), 5, ' ' ) );
				ImGui::TableSetColumnIndex( 6 );
				ImGui::Text( "%s", fixedWidthNumberString( int( RangeRemap( entity.position.y - floor( entity.position.y ), 0.0f, 1.0f, -controller.sectorSize / 2.0f, controller.sectorSize / 2.0f ) ), 5, ' ' ) );
				ImGui::PopItemWidth();
				ImGui::TableSetColumnIndex( 7 );
				ImGui::Text( "%f, %f", entity.scale.x, entity.scale.y );
			}
			ImGui::EndTable();
		}
		ImGui::End();

		TonemapControlsWindow();

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void ComputePasses () {
		ZoneScoped;

		{
			scopedTimer Start( "Background Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Background Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
			glUniform1f( glGetUniformLocation( shader, "globalZoom" ), controller.globalZoom );
			glUniform1f( glGetUniformLocation( shader, "sectorSize" ), controller.sectorSize );

			static rngi noiseOffset( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

			// "position", center of the view, is V ahead of the ship's location at P (with some scale factors)
			vec2 v = controller.entityList[ 0 ].GetVelocityVector();
			vec2 p = controller.entityList[ 0 ].GetPositionVector() * vec2( 1.0f, -1.0f ) * 0.02f;
			glUniform2f( glGetUniformLocation( shader, "positionVector" ), p.x - v.x, p.y + v.y );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{
			scopedTimer Start( "BVH Draw" );
			GLuint shader = shaders[ "BVH Draw" ];
			glUseProgram( shader );

		// having this BVH is actually very nice, because it means that I can have this model of all objects in the sector available on CPU and GPU
			// low geometric complexity of the ship bounding boxes means that this will scale to large object counts easily, even having to be updated every frame
			// occlusion solution via symmetric z scaling of the bounding boxes... smaller ships are taller bboxes, so viewed from above, they are encountered first, closer
				// this is used to place larger ships and stations beneath smaller ships, very similar to a simple 2D primitive draw order
				// symmetric scaling of the bounding boxes means that everything exists in the z=0 plane... you can do 2D logic there
			// this pass only uses it for rendering... but gameplay can use it for any sort of AI usage, simulated sensors, weapon guidance and collision

			glUniform1f( glGetUniformLocation( shader, "globalZoom" ), controller.globalZoom );

			static rngi noiseOffset( 0, 512 );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

			vec2 v = controller.entityList[ 0 ].GetVelocityVector() * ( controller.entityList[ 0 ].shipSpeed / controller.entityList[ 0 ].stats.maxSpeedForward ) * controller.entityList[ 0 ].stats.maxThrustDisplacement;
			vec2 p = controller.entityList[ 0 ].GetPositionVector();
			glUniform2f( glGetUniformLocation( shader, "centerPoint" ), p.x - v.x, p.y - v.y );

			textureManager.BindTexForShader( "AtlasTexture", "atlasTexture" , shader, 3 );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{
			scopedTimer Start( "Line Drawing" );

			// we want to use this to draw some elements of the UI...
				// this happens in 8 layers... each is drawn in a distinct color
			controller.lineUIDraw();
		}

		{
			scopedTimer Start( "Tiny Text Drawing" );
			controller.tinyTextDrawing( textureManager );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			bindSets[ "Postprocessing" ].apply();
			glUseProgram( shaders[ "Tonemap" ] );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{
			scopedTimer Start( "Text Rendering" );

			DrawMenuBasics( textRenderer, textureManager );

			// DrawBlockMenu( "Test", textRenderer, textureManager );

			DrawInfoLog( textRenderer, textureManager );

			// show terminal, if active - check happens inside
			textRenderer.Clear();
			// textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.drawTerminal( terminal );
			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
		}

		{ // show trident with current orientation
			// scopedTimer Start( "Trident" );
			// trident.Update( textureManager.Get( "Display Texture" ) );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void RebuildAtlasIfNeeded () {
		ZoneScoped;
		if ( controller.entityListDirty ) {
			controller.atlas->UpdateAtlas( controller.entityList );
			controller.entityListDirty = false;
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		controller.update();
		RebuildAtlasIfNeeded();
		controller.updateBVH();
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		ComputePasses();			// multistage update of displayTexture
		BlitToScreen();				// fullscreen triangle copying to the screen
		{
			scopedTimer Start( "ImGUI Pass" );
			ImguiFrameStart();		// start the imgui frame
			ImguiPass();			// do all the gui stuff
			ImguiFrameEnd();		// finish imgui frame and put it in the framebuffer
		}
		window.Swap();				// show what has just been drawn to the back buffer
	}

	bool MainLoop () { // this is what's called from the loop in main
		ZoneScoped;

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal (active check happens inside)
		terminal.update( inputHandler );

		// event handling
		HandleTridentEvents();
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	ZoneScoped;
	SpaceGame engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
