class spaceshipController {
public:

	float angle = 0.0f;
	float velocity = 0.0f;

	vec2 position = vec2( 0.0f );

	spaceshipStats stats;

	void Update ( inputHandler_t &input ) {
		// solve for deltaT
		const float deltaT = clamp( input.millisecondsSinceLastUpdate(), 1.0f, 100.0f );

	// consider user inputs
		// rotation
		if ( input.getState( KEY_A ) ) { angle += stats.turnRate * deltaT * input.getState_soft( KEY_A ); }
		if ( input.getState( KEY_D ) ) { angle -= stats.turnRate * deltaT * input.getState_soft( KEY_D ); }

		// clamping angle
		// if ( angle < 0.0f ) { angle += tau; }
		// if ( angle > tau ) { angle -= tau; }

		// acceleration/deceleration
		if ( input.getState( KEY_W ) ) { velocity += stats.accelerationRate * deltaT * input.getState_soft( KEY_W ); }
		if ( input.getState( KEY_S ) ) { velocity -= stats.decelerationRate * deltaT * input.getState_soft( KEY_S ); }

		// clamping large magnitude velocity
		velocity = glm::clamp( velocity, -stats.maxSpeedBackward, stats.maxSpeedForward );

		// apply velocity times deltaT to get new position
		position += deltaT * GetVelocityVector();
	}

	vec2 GetPositionVector() {
		return position;
	}

	vec2 GetVelocityVector() {
		return glm::mat2(
			cos( angle ), -sin( angle ),
			sin( angle ), cos( angle )
		) * vec2( velocity, 0.0f );
	}

};
