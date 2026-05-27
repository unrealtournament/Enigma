/*
 * Copyright (C) 2002,2003,2004 Daniel Heck
 * Copyright (C) 2008,2009,2010 Ronald Lamprecht
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
 */
#ifndef ACTORS_HH_INCLUDED
#define ACTORS_HH_INCLUDED

#include "StateObject.hh"
#include "display.hh"
#include "Value.hh"

namespace enigma {

enum ActorID {
    ac_INVALID = -1,
    ac_FIRST = 0,
    ac_marble_black = 0,
    ac_marble_white = 1,
    ac_marble_glass = 2,
    ac_pearl_black = 3,
    ac_pearl_white = 4,
    ac_pearl_glass = 5,
    ac_killer_black = 6,
    ac_killer_white = 7,
    ac_rotor = 8,
    ac_top = 9,
    ac_horse = 10,
    ac_bug = 11,
    ac_cannonball = 12,
    ac_spermbird = 13,
    ac_LAST = 13,
    ac_COUNT
};

struct ActorTraits {
    const char *name;
    ActorID id;
    unsigned id_mask;
    float radius;
    float defaultMass;
};

/* -------------------- ActorInfo -------------------- */

struct Contact {
    ecl::V2 pos;
    ecl::V2 normal;

    // Constructor
    Contact() {}

    Contact(ecl::V2 pos, ecl::V2 normal) : pos(std::move(pos)), normal(std::move(normal)) {}
};

#define MAX_CONTACTS 7

struct Field;
struct StoneContact;

/*!
 * This class contains the information the physics engine
 * maintains about dynamic objects ("actors").
 */
struct ActorInfo {
    // ---------- Variables ----------

    ecl::V2 pos;           // Absolute position
    GridPos gridpos;       // Grid position for pos
    GridPos last_gridpos;  // last pos handled by actor move
    const Field *field;    // Field of pos
    ecl::V2 vel;           // Velocity
    ecl::V2 frozen_vel;    // Velocity backup for forzen actors
    ecl::V2 pos_force;     // Extrapolated position for global force calc
    ecl::V2 forceacc;      // Force accumulator
    double charge;         // Electric charge
    double mass;           // Mass
    double radius;         // Radius of the ball
    bool grabbed;          // Actor not controlled by the physics engine
    bool created;          // Actor has already been created
    bool ignore_contacts;  // Do not perform collision handling

    // Variables used internally by the physics engine

    ecl::V2 force;  // Force used during tick
    ecl::V2 collforce;
    double friction;  // friction on the current position

    // 2 sets of contacts - one for the current tick, one for the last tick
    Contact contacts_a[MAX_CONTACTS];
    Contact contacts_b[MAX_CONTACTS];
    Contact *contacts;        // pointer to the durrent ticks contacts
    Contact *last_contacts;   // pointer to the last ticks contacts
    int contacts_count;       // number of valid contacts for current tick
    int last_contacts_count;  // number of valid contacts for last tick

    // Constructor
    ActorInfo();
};

class Actor : public StateObject, public display::ModelCallback {
    friend class World;
    friend class ActorsInRangeIterator;

public:
    static const double maxRadius;

    // ModelCallback interface
    void animcb() override;

    /* ---------- Object interface ---------- */
    void setAttr(const std::string &key, const Value &val) override;
    Value getAttr(const std::string &key) const override;
    Value message(const Message &m) override;

    /* ---------- Actor interface ---------- */
    virtual const ActorTraits &get_traits() const = 0;

    virtual void think(double dtime);

    virtual bool on_collision(Actor *a);
    virtual void on_creation(const ecl::V2 &pos);
    virtual void on_respawn(const ecl::V2 &pos);

    virtual bool is_dead() const = 0;
    virtual bool isMoribund() const { return is_dead(); }
    virtual bool is_movable() const { return true; }
    virtual bool is_flying() const { return false; }
    virtual bool is_on_floor() const { return true; }
    virtual bool is_drunken() const { return false; }
    virtual bool is_invisible() const { return false; }

    virtual bool can_drop_items() const { return true; }
    virtual bool can_move() const;
    virtual bool can_be_warped() const { return false; }
    virtual bool has_shield() const { return true; }

    virtual void init();

    /* ---------- Methods ---------- */
    void move();
    virtual void move_screen();
    void warp(const ecl::V2 &newPos);
    bool sound_event(const char *name, double volume = 1.0);

    void respawn();
    void set_respawnpos(const ecl::V2 &p);
    void remove_respawnpos();
    void find_respawnpos();
    const ecl::V2 &getRespawnPos() const;
    const ecl::V2 &getStartPos() const;

    virtual void hide();
    void show();

    void addForce(const ecl::V2 &f);
    virtual void beforeStoneBounce(const StoneContact &) {}
    virtual void afterStoneBounce(const StoneContact &) {}

    /* ---------- Accessors ---------- */
    ActorInfo &getMutableActorInfo();
    const ActorInfo &getActorInfo() const;
    const ecl::V2& getPos() const { return actorInfo.pos; }
    const ecl::V2 &getPosForce() const;

    ActorID getActorId() const { return get_traits().id; }

    double getRadius() const { return actorInfo.radius; }
    double getMass() const { return actorInfo.mass; }
    double getCharge() const { return actorInfo.charge; }
    const ecl::V2 &getVel() const { return actorInfo.vel; }

    bool hasSpikes() const { return spikes; }

    static double getMaxRadius();  // max. radius of all actors

    int getControllers() const { return controllers; }
    bool isSteerable() const { return adhesion != 0.0; }
    double get_mouseforce() const { return adhesion; }

    bool controlled_by(int player) const { return (getControllers() & (1 + player)) != 0; }

    const GridPos &get_gridpos() const { return actorInfo.gridpos; }
    double squareDistance(const Object *other) const override;
    bool isSouthOrEastOf(const Object *other) const override;

protected:
    ObjectType getObjectType() const override { return ACTOR; }

    Actor(const ActorTraits &traits);
    void setModel(const std::string &modelName);
    void setAnim(const std::string &modelName);

    display::SpriteHandle &getSprite() { return sprite; }

    /* ---------- Variables ---------- */
    ActorInfo actorInfo;
    bool centerRespawn;   // default, like on flag drop or sink, fall, shatter
    bool inplaceRespawn;  // respawn on exactly last valid position, use on suicide to prevent
                          // shortcuts
private:
    Actor* left = nullptr; // x-coordinate sorted doubly linked list
    Actor* right = nullptr;
    display::SpriteHandle sprite;
    ecl::V2 startingPos;
    ecl::V2 respawnPos;
    bool flagRespawn;
    bool firstGridStep = false;
    bool spikes;  // set by "it_pin"
    int controllers = 0;
    double adhesion = 0.0;
};

class ActorsInRangeIterator {
public:
    ActorsInRangeIterator(Actor *center, double range, unsigned type_mask);
    Actor *next();

private:
    Actor *centerActor;
    double xCenter;
    Actor *currentActor;
    bool moveLeft;
    double rangeDist;
    unsigned typeMask;
};

/* -------------------- Global Functions -------------------- */

void InitActors();

/* -------------------- Actor Macros -------------------- */

#define DECL_ACTORTRAITS       \
    static ActorTraits traits; \
    const ActorTraits &get_traits() const { return traits; }

#define DECL_ACTORTRAITS_ARRAY(n, subtype_expr) \
    static ActorTraits traits[n];               \
    const ActorTraits &get_traits() const { return traits[subtype_expr]; }

}  // namespace enigma

#endif
