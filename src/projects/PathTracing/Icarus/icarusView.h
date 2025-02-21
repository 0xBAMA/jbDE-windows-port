// =============================================================================================================
struct viewerState_t {

	// passed to the viewer
	float scale = 1.0f;
	vec2 offset = vec2( 0.0f );

	vec2 viewerSize;

	// apply scrolling
	void scroll ( float amount ) {
		scale = std::clamp( scale - amount * ( ( SDL_GetModState() & SDL_KMOD_SHIFT ) ? 0.07f : 0.01f ), 0.005f, 5.0f );
	}

	void dragUpdate ( ivec2 dragDelta, bool dragging ) {
		static bool isDragging = false;
		static vec2 offsetPush = vec2( 0.0f );
		if ( dragging && !isDragging ) {
			isDragging = true;
			offsetPush = offset;
		} else if ( dragging ) { // while this dragging is happening...
			offset = offsetPush - ( scale * vec2( dragDelta ) * vec2( 2.0f / std::min( viewerSize.x, viewerSize.y ) ) );
		} else {
			isDragging = false;
		}
	}

};