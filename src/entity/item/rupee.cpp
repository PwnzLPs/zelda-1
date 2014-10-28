#include "rupee.hpp"
#include "../mob.hpp"
#include "../mob/link.hpp"

SpriteSheet* Rupee::RUPEES_SPRITESHEET;
std::vector<Sprite*> Rupee::RUPEES;

void Rupee::Load() {
    RUPEES_SPRITESHEET = new SpriteSheet("item/rupees.png", 27, 16, 9, 16);
    RUPEES = RUPEES_SPRITESHEET->GetSprites(0, 3);
}

Rupee* Rupee::Random(float x, float y) {
    int r = rand() % 100;

    Rupee::Type type;

    if(r < 85)
        type = Rupee::Type::GREEN;
    else if(r < 97)
        type = Rupee::Type::BLUE;
    else
        type = Rupee::Type::RED;

    return new Rupee(type, x - RUPEES[type]->width() / 2.0f, y - RUPEES[type]->height() / 2.0f);
}

Rupee::Rupee(Rupee::Type type, float x, float y) :
        super(RUPEES[type], x, y),
        rupee_type_(type)
{}

bool Rupee::HandleCollisionWith(Mob* mob) {
    int amount = rupee_type_ == Rupee::Type::GREEN ? 1 : rupee_type_ == Rupee::Type::BLUE ? 2 : 5;

    // Mob has to be a player
    ((Link*) mob)->UpdateRupees(amount);

    Kill();
    return false;
}