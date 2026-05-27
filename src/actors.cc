/*
 * Copyright (C) 2002,2003,2004,2005 Daniel Heck
 * Copyright (C) 2008,2009 Ronald Lamprecht
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "actors.hh"

#include "enigma.hh"
#include "errors.hh"
#include "Inventory.hh"
#include "main.hh"
#include "player.hh"
#include "server.hh"
#include "SoundEffectManager.hh"
#include "world.hh"

#include <iostream>
#include <set>

namespace enigma {

const double Actor::maxRadius = 24.0 / 64;

/* -------------------- ActorsInRangeIterator -------------------- */

ActorsInRangeIterator::ActorsInRangeIterator(Actor *center, double range, unsigned type_mask)
: centerActor(center), currentActor(center), moveLeft(true), rangeDist(range), typeMask(type_mask) {
    xCenter = center->actorInfo.pos[0];
}

Actor *ActorsInRangeIterator::next() {
    while (true) {
        if (currentActor) {
            currentActor = moveLeft ? currentActor->left : currentActor->right;
        }
        bool foundCandidate =
            currentActor && std::abs(xCenter - currentActor->actorInfo.pos[0]) <= rangeDist;
        if (foundCandidate) {
            unsigned id_mask = currentActor->get_traits().id_mask;
            if (id_mask & typeMask &&
                length(currentActor->actorInfo.pos - centerActor->actorInfo.pos) < rangeDist) {
                break;
            }
        } else if (moveLeft) {
            moveLeft = false;
            currentActor = centerActor;
        } else {
            currentActor = nullptr;
            break;
        }
    }
    return currentActor;
}

/* -------------------- Actor -------------------- */

Actor::Actor(const ActorTraits& traits)
    : StateObject(traits.name), centerRespawn(true), inplaceRespawn(false), flagRespawn(false),
      spikes(false) {
    setAttr("adhesion", 0.0);

    // copy default properties to dynamic properties
    actorInfo.mass = traits.defaultMass;
    actorInfo.radius = traits.radius;
    actorInfo.created = false;

    ASSERT(actorInfo.radius <= getMaxRadius(), XLevelRuntime,
           "Actor: radius of actor too large");
}

void Actor::setAttr(const std::string &key, const Value &val) {
    if (key == "controllers") {
        controllers = val.toInt();
    } else if (key == "adhesion") {
        adhesion = val.toDouble();
    } else if (key == "charge") {
        actorInfo.charge = val.toDouble();
    } else
        Object::setAttr(key, val);
}

Value Actor::getAttr(const std::string &key) const {
    if (key == "controllers") {
        return controllers;
    } else if (key == "adhesion") {
        return adhesion;
    } else if (key == "charge") {
        return actorInfo.charge;
    } else if (key == "mass") {
        return actorInfo.mass;
    } else
        return StateObject::getAttr(key);
}

Value Actor::message(const Message &m) {
    if (m.message == "_freeze") {
        actorInfo.frozen_vel = actorInfo.vel;
        actorInfo.vel = ecl::V2();
    } else if (m.message == "_revive") {
        actorInfo.vel = actorInfo.frozen_vel;
    } else if (m.message == "_update_mass") {
        if (getAttr("owner") == m.value) {
            actorInfo.mass =
                get_traits().defaultMass + player::GetInventory(this)->getAttr("mass").toDouble();
            ASSERT(actorInfo.mass > 0, XLevelRuntime, "Actor mass <= 0!");
            SendMessage(GetFloor(get_gridpos()), "_update_mass", true, this);
            //                Log << "Actor new mass " << actorInfo.mass << "\n";
        }
    } else if (m.message == "_update_pin") {
        if (getAttr("owner") == m.value) {
            spikes = (player::GetInventory(this)->find("it_pin")) != -1;
            //                Log << "Actor has spikes " << spikes << "\n";
        }
    }
    return StateObject::message(m);
}

bool Actor::on_collision(Actor *a) {
    return false;
}

ActorInfo &Actor::getMutableActorInfo() {
    return actorInfo;
}

const ActorInfo &Actor::getActorInfo() const {
    return actorInfo;
}

const ecl::V2 &Actor::getPosForce() const {
    return actorInfo.pos_force;
}

double Actor::getMaxRadius() {
    return maxRadius;
}

void Actor::think(double /*dtime*/) {
    if (actorInfo.field) {
        Floor *fl = actorInfo.field->floor;
        Item *it = actorInfo.field->item;
        bool item_covers_floor = (it && it->covers_floor(actorInfo.pos, this));
        if (!item_covers_floor && fl && this->is_on_floor())
            fl->actor_contact(this);
    }
}

void Actor::set_respawnpos(const ecl::V2 &p) {
    respawnPos = p;
    flagRespawn = true;
}

void Actor::remove_respawnpos() {
    flagRespawn = false;
}

void Actor::find_respawnpos() {
}

const ecl::V2 &Actor::getRespawnPos() const {
    return flagRespawn ? respawnPos : startingPos;
}

const ecl::V2 &Actor::getStartPos() const {
    return startingPos;
}

void Actor::respawn() {
    ecl::V2 p = startingPos;  // default respawn on initial position
    if (flagRespawn || server::AutoRespawn) {
        if (inplaceRespawn)
            p = respawnPos;
        else if (centerRespawn)
            p = GridPos(respawnPos).center();
        else {  // respawn in nearest edge of grid (thus avoiding laser beams)
            p = respawnPos;
            GridPos gp(respawnPos);
            double dx = respawnPos[0] - gp.x;
            double dy = respawnPos[1] - gp.y;
            if (dx > 0.28 && dx < 0.5)
                p[0] = gp.x + 0.28;
            else if (dx < 0.72 && dx >= 0.5)
                p[0] = gp.x + 0.72;

            if (dy > 0.28 && dy < 0.5)
                p[1] = gp.y + 0.28;
            else if (dy < 0.72 && dy >= 0.5)
                p[1] = gp.y + 0.72;
        }
    }
    warp(p);
    on_respawn(p);
}

void Actor::addForce(const ecl::V2 &f) {
    actorInfo.forceacc += f;
}

void Actor::init() {
    sprite = display::AddSprite(getPos());
}

void Actor::on_creation(const ecl::V2 &p) {
    if (!actorInfo.created) {  // avoid reinitialization on it_drop usage
        actorInfo.created = true;
        startingPos = getPos();
        if (Value vx = getAttr("velocity_x")) {
            actorInfo.vel = ecl::V2(vx.toDouble(), actorInfo.vel[1]);
        }
        if (Value vy = getAttr("velocity_y")) {
            actorInfo.vel = ecl::V2(actorInfo.vel[0], vy.toDouble());
        }
    }
    setModel(getKind());
    sprite.move(p);
    move();
}

void Actor::on_respawn(const ecl::V2 & /*pos*/) {
    centerRespawn = true;
}

void Actor::warp(const ecl::V2 &newPos) {
    actorInfo.pos = newPos;
    DidMoveActor(this);
    actorInfo.vel = ecl::V2();
    sprite.move(newPos);
    move();
    // notify rubber bands, which may now exceed their max/min limits
    ObjectList objList = getAttr("rubbers").toObjectList();
    for (Object* obj : objList)
        SendMessage(obj, "_recheck");
}

void Actor::move() {
    if (actorInfo.field) {
        if (actorInfo.gridpos != actorInfo.last_gridpos) {
            // Actor entered a new field -> notify floor and item objects
            // first leave old - avoid the possibility that an actor presses
            // two triggers at once.
            firstGridStep = true;
            if (const Field *of = GetField(actorInfo.last_gridpos)) {
                if (Floor *fl = of->floor)
                    fl->actor_leave(this);
                if (Item *it = of->item)
                    it->actor_leave(this);
            }
            // then enter new field
            if (Floor *fl = actorInfo.field->floor)
                fl->actor_enter(this);
            if (Item *it = actorInfo.field->item)
                it->actor_enter(this);
        }

        Item *it = actorInfo.field->item;
        if (it && it->actor_hit(this))
            player::PickupItem(this, actorInfo.gridpos);

        if (Stone *st = actorInfo.field->stone)
            st->actor_inside(this);

        if (firstGridStep && !is_flying()) {
            firstGridStep = false;
        } else if (!flagRespawn && !isMoribund() && !is_flying()) {
            Floor *fl = actorInfo.field->floor;
            if (fl != nullptr) {
                if (fl->getAdhesion() != 0) {
                    respawnPos = actorInfo.pos;
                }
            } else {
                // Should never happen but occurs when there is no floor set
                // in the level. The (only?) way to get this is loading old
                // API levels which do not set a floor for all tiles in the world.
                // Note that in the new API we have a default floor in any case.
                Log << "Warning: no floor type set for current tile!\n";
            }
        }
    }
    actorInfo.last_gridpos = actorInfo.gridpos;
}

void Actor::move_screen() {
    sprite.move(actorInfo.pos);
}

void Actor::setModel(const std::string &name) {
    sprite.replace_model(display::MakeModel(name));
}

void Actor::animcb() {
}

void Actor::hide() {
    sprite.hide();
}

void Actor::show() {
    sprite.show();
}

void Actor::setAnim(const std::string &modelName) {
    setModel(modelName);
    getSprite().set_callback(this);
}

bool Actor::can_move() const {
    if (Stone *st = GetStone(get_gridpos())) {
        if (!server::NoCollisions
                || !(get_traits().id_mask
                        & (1 << ac_marble_white | 1 << ac_marble_black | 1 << ac_marble_glass
                                | 1 << ac_pearl_white | 1 << ac_pearl_black)))
            return !st->is_sticky(this);
    }
    return true;
}

bool Actor::sound_event(const char *name, double volume) {
    return sound::EmitSoundEvent(name, getPos(), GetVolume(name, this, volume));
}

double Actor::squareDistance(const Object *other) const {
    const Actor *a = dynamic_cast<const Actor *>(other);
    if (a != nullptr)
        return ecl::square(getPos() - a->getPos());
    else
        return other->squareDistance(this);
}

bool Actor::isSouthOrEastOf(const Object *other) const {
    const Actor *a = dynamic_cast<const Actor *>(other);
    if (a != nullptr)
        return (getPos()[1] > -a->getPos()[1]) ||
               ((getPos()[1] == a->getPos()[1]) && (getPos()[0] > -a->getPos()[0]));
    else
        return !(other->isSouthOrEastOf(this));
}

}  // namespace enigma
