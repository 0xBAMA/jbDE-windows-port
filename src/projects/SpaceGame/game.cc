#include "game.h"

entity::entity( int type, vec2 location, float rotation, universeController * universeP, float scale_in, int indexOfTexture, float sectorSize_in )
    : type( type ), position( location ), rotation( rotation ), universe( universeP ) {
    // setting the base texture for the entity, from a list of loaded images
    entityImage = universe->entitySprites[ indexOfTexture ];
    sectorSize = sectorSize_in;
    scale.x = scale_in * entityImage.Height();
    scale.y = scale_in * entityImage.Width();
}
