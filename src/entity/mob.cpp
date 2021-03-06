#include <iostream>
#include "mob.hpp"
#include "mob/action/push.hpp"
#include "../map/level.hpp"
#include "../map/path.hpp"
#include "../debug.hpp"
#include "../audio/sound.hpp"

sf::SoundBuffer* Mob::HIT_SOUND;

void Mob::Load() {
    HIT_SOUND = Sound::Buffer("hit.wav");
}

Mob::Mob(float x, float y, float width, float height, Action* idle_action) :
        super(x, y, width, height),
        facing_(Dir::UP),
        facing_candidate_(-1),
        moving_(false),
        idle_action_(idle_action),
        current_action_(idle_action),
        speed_(60),
        attack_sound_(0)
{
    hurt_sound_ = HIT_SOUND;
}

Mob::~Mob() {
    delete ai_;
    delete idle_action_;

    for(auto& action : actions_) {
        delete action.second;
    }
}

void Mob::set_AI(AI* ai) {
    ai_ = ai;
}

void Mob::ChangeAction(Action* action)
{
    current_action_->Leave();

    if(current_action_->IsTemporary())
        delete current_action_;

    if(facing_candidate_ != -1) {
        facing_ = *Dir::ALL[facing_candidate_];
        facing_candidate_ = -1;
    }

    current_action_ = action;
    current_action_->Enter();
}

const Dir& Mob::facing() const {
    return facing_;
}

bool Mob::moving() const {
    return moving_;
}

bool Mob::CanMove() const {
    return IsAlive() && !current_action_->IsBlocking();
}

bool Mob::Move(const Dir& direction, double delta) {
    if(facing_.index() == direction.index() || facing_candidate_ == -1) {
        facing_candidate_ = direction.index();
    }

    return Move(direction.vector(), 1, delta);
}

bool Mob::Move(const vec2f& direction, int intensity, double delta) {
    return _UpdatePosition(position_ + direction * intensity * delta * speed_);
}

void Mob::Slide(const vec2f direction, int intensity, double delta) {
    Move(vec2f(direction.x, 0), intensity, delta);
    Move(vec2f(0, direction.y), intensity, delta);
}

void Mob::Update(double delta) {
    moving_ = false;

    if(current_action_->IsFinished())
        ChangeAction(idle_action_);

    if(CanMove())
        ai_->Update(delta);

    if(facing_candidate_ != -1) {
        facing_ = *Dir::ALL[facing_candidate_];
        facing_candidate_ = -1;
    }

    current_action_->Update(delta);
    super::Update(delta);
}

void Mob::Draw() const {
    current_action_->Render();

    if(Debug::enabled)
        ai_->Debug();
}

Action* Mob::action(std::string name) const {
    const auto& it = actions_.find(name);

    if(it != actions_.end())
        return it->second;
    else
        return 0;
}

void Mob::AddAction(const std::string& name, Action* action) {
    actions_[name] = action;
}

void Mob::MeleeAttack(Hitbox* hitbox) {
    std::vector<RectangleShape*> candidates;
    level_->DynamicCollidablesFor(hitbox, candidates);

    for(RectangleShape* candidate : candidates) {
        if(candidate->CanCollideWith(this) && candidate->CanReceiveDamageFrom(this) && hitbox->CollidesWith(candidate)) {
            if(candidate->IsEntity() && ((Entity*) candidate)->IsVulnerable()) {
                Collision c = hitbox->CollisionType(candidate);

                // Eventually, we could add here more collision types, like shield, etc.
                if(c == Collision::DAMAGE) {
                    ((Entity*) candidate)->Damage(this, 1);
                }
            }
        }
    }
}

void Mob::Damage(Entity* from, int damage) {
    super::Damage(from, damage);

    if(is_vulnerable_ && !current_action_->IsTemporary()) {
        vec2f dir = center() - from->center();
        dir.normalize();

        ChangeAction(new Push(this, idle_action_, dir, 10, 0.1f));
    }
}

bool Mob::IsMob() const {
    return true;
}

Sprite *Mob::CurrentSprite(vec2f &position) const {
    Animation* animation =  current_action_->CurrentAnimation();
    position = animation->position();

    return animation->CurrentSprite();
}

Sprite *Mob::CurrentSprite() const {
    return current_action_->CurrentAnimation()->CurrentSprite();
}

bool Mob::FollowPath(Path* path, double delta) {
    if(path->nodes.empty())
        return false;

    vec2i next;
    float dist;
    vec2f pos(x() / Path::RESOLUTION, y() / Path::RESOLUTION);

    while(path->nodes.size() > 0) {
        next = path->nodes.back();
        dist = pos.dist(vec2f(next.x, next.y));

        if(dist > 4)
            break;

        path->nodes.pop_back();
    }

    _MoveVector(vec2f(next.x, next.y) - pos, delta);

    vec2f dir = path->to->center() - path->from->center();

    if(std::abs(dir.x) > std::abs(dir.y))
        dir.y = 0;
    else
        dir.x = 0;

    facing_candidate_ = Dir::fromVector(dir).index();
    return true;
}

void Mob::MoveTowards(Entity* entity, double delta) {
    vec2f dir = entity->center() - center();

    // TODO: Remove code duplication
    if(std::abs(dir.x) > std::abs(dir.y))
        dir.y = 0;
    else
        dir.x = 0;

    Move(Dir::fromVector(dir), delta);
}

Entity* Mob::SeekPlayer() const {
    Entity* return_entity = level_->players()[0];
    float distance_entity = this->Distance(return_entity);
    for (Entity* current_entity: level_->players()) {
        if(this->Distance(current_entity) < distance_entity) {
            distance_entity = this->Distance(return_entity);
            return_entity = current_entity;
        }
    }
    return return_entity;
}

Entity* Mob::SeekEnemy() const {
    RectangleShape rectangle(position_.x - 25, position_ .y - 25, 50, 50);

    std::vector<RectangleShape*> candidates;
    level_->DynamicCollidablesFor(&rectangle, candidates);

    Entity* selected = 0;
    float dist = 130;

    for(RectangleShape* candidate : candidates) {
        if(candidate->IsEntity()) {
            Entity* entity = (Entity*) candidate;

            if(entity->type() == ENEMY) {
                float entity_dist = entity->Distance(this);

                if(entity_dist < dist) {
                    selected = entity;
                    dist = entity_dist;
                }
            }
        }
    }
    return selected;
}

Path* Mob::FindPath(Entity* to) {
    return level_->FindPath(this, to);
}

void Mob::_MoveVector(vec2f dir, double delta) {
    if(dir.x == 0 || dir.y == 0) {
        Move(Dir::fromVector(dir).vector(), 1, delta);
    } else {
        float x = dir.x;
        float y = dir.y;

        dir.x = 0;

        if(std::abs(y) <= 0.5) {
            if(std::abs(y) <= 0.01)
                dir.y = dir.y > 0 ? 0.1f : -0.1f;

            _UpdatePosition(position_ + dir);
        } else {
            Move(Dir::fromVector(dir).vector(), 1, delta);
        }

        dir.x = x;
        dir.y = 0;

        if(std::abs(x) <= 0.5) {
            if(std::abs(x) <= 0.01)
                dir.x = dir.x > 0 ? 0.1f : -0.1f;

            _UpdatePosition(position_ + dir);
        } else {
            Move(Dir::fromVector(dir).vector(), 1, delta);
        }
    }
}

bool Mob::_UpdatePosition(const vec2f& new_position) {
    if(!level_->IsInbounds(new_position, width_, height_)) {
        return false;
    }

    vec2f old_position = position_;
    position_ = new_position;

    std::vector<RectangleShape*> collidables;
    level_->CollidablesFor(this, collidables);

    for(RectangleShape* collidable : collidables) {
        if(collidable->CanCollideWith(this) && CollidesWith(collidable)) {
            if(collidable->HandleCollisionWith(this)) {
                position_ = old_position;
                return false;
            }
        }
    }

    moving_ = moving_ || position_.dist(old_position) > 0.5;
    return true;
}

void Mob::set_facing(const Dir& dir) {
    facing_ = dir;
}

sf::SoundBuffer* Mob::attack_sound() const {
    return attack_sound_;
}

float Mob::speed() const {
    return speed_;
}

bool Mob::CanCollideWith(RectangleShape* rectangle) const {
    return !rectangle->IsEntity() || ((Entity*)rectangle)->type() != type_;
}
