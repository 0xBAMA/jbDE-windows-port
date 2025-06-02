#include "game.h"

entity::entity( int type, vec2 location, float rotation, universeController * universeP, vec2 scale, int indexOfTexture )
    : type( type ), position( location ), rotation( rotation ), universe( universeP ), scale( scale ) {
    // setting the base texture for the entity, from a list of loaded images
    entityImage = universe->entitySprites[ indexOfTexture ];
}
