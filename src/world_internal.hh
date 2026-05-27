/*
 * Copyright (C) 2002,2003,2004 Daniel Heck
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

#pragma once

#include "ecl_array2.hh"
#include "ecl_dict.hh"
#include "SoundEngine.hh"
#include "world.hh"

#include <list>
#include <vector>

namespace enigma {

struct Field;
struct Signal;

typedef ecl::Array2<Field> FieldArray;

typedef std::vector<ForceField *> ForceList;
typedef std::list<Other *> OtherList;
typedef std::list<Rubberband *> RubberbandList;
typedef std::vector<Actor *> ActorList;
typedef std::vector<Signal> SignalList;

/* -------------------- MouseForce -------------------- */

/// This class implements the "force field" that accelerates objects when the
/// mouse is moved.  This force field only affects objects that have the
/// "mouseforce" and the "controllers" attributes set.
class MouseForce {
public:
    void set_force(ecl::V2 f) { force = f; }
    void add_force(ecl::V2 f) { force += f; }

    ecl::V2 get_force(Actor *a) {
        if (a->is_flying() || a->is_dead())
            return ecl::V2();
        return force * a->get_mouseforce();
    }

    void tick(double /*dtime*/) { force = ecl::V2(); }

private:
    ecl::V2 force;
};

/* -------------------- Scramble -------------------- */

/// Stores all positions plus direction where puzzle scrambling
/// should be performed.
struct Scramble {
    // Variables
    GridPos pos;
    Direction dir;
    int intensity;

    Scramble(GridPos p, Direction d, int i) : pos(p), dir(d), intensity(i) {}

    bool expired() const { return intensity < 1; }
};

/* -------------------- DelayedImpulse -------------------- */

class DelayedImpulse {
    Impulse impulse;
    double delay;
    const Stone *receiver;  // to test if stone has changed
    bool isReferenced;      // an iterator references this impulse
    bool isObsolete;        // the impulse should be deleted

public:
    DelayedImpulse(const Impulse& impulse_, double delay_, const Stone* receiver_)
        : impulse(impulse_), delay(delay_), receiver(receiver_), isReferenced(false),
          isObsolete(false) {}

    DelayedImpulse(const DelayedImpulse& other) = default;
    DelayedImpulse &operator=(const DelayedImpulse &other) = delete;

    bool tick(double dtime) {  // returns true if Impulse has to be sent NOW
        delay -= dtime;
        return delay <= 0;
    }

    const GridPos &destination() const { return impulse.dest; }

    bool is_receiver(const Stone *target) const { return target == receiver; }

    bool is_sender(const Stone *target) const { return target == impulse.sender; }

    bool is_referenced() const { return isReferenced; }

    void mark_referenced(bool state) { isReferenced = state; }

    bool is_obsolete() const { return isObsolete; }

    void mark_obsolete() { isObsolete = true; }

    void send_impulse(Stone *target) const {
        // @@@ FIXME: the test for equality of Stones sometimes fails
        // (e.g., with turnstiles rotated by rotators)
        //
        // I guess this happens, because when the turnstile-pivot
        // removes and adds arms during turn, it may happen that
        // the Stone receives the same memory address
        //
        // Possible fix: add unique ID to all objects

        if (is_receiver(target)) {
            // if object did not change since impulse was initiated
            target->on_impulse(impulse);
        }
    }
};

typedef std::list<DelayedImpulse> ImpulseList;

/* -------------------- Layer -------------------- */

template <class T>
class Layer {
    T *defaultVal;

public:
    explicit Layer(T *deflt = nullptr) : defaultVal(deflt) {}
    virtual ~Layer() = default;

    T *get(GridPos p);
    virtual T *yield(GridPos p);
    virtual void set(GridPos p, T *x);
    void kill(GridPos p) { dispose(yield(p)); }

protected:
    virtual T *raw_get(Field &) = 0;
    virtual void raw_set(Field &, T *) = 0;

private:
    virtual void dispose(T *x) {
        if (x)
            DisposeObject(x);
    }
};

/*
** Floor layer
*/
class FloorLayer : public Layer<Floor> {
public:
    Floor *yield(GridPos p) override {
        Floor *f = Layer<Floor>::yield(p);
        if (f != nullptr) {
            if (Value v = f->getAttr("name")) {
                NamePosition(p, v.toString());
            }
        }
        return f;
    }

    void set(GridPos p, Floor *x) override {
        Floor *f = get(p);
        if (f != nullptr) {
            if (Value v = f->getAttr("name")) {
                NamePosition(p, v.toString());
            }
        }
        Layer<Floor>::set(p, x);
    }

protected:
    Floor *raw_get(Field &f) override { return f.floor; }
    void raw_set(Field &f, Floor *x) override { f.floor = x; }
};

/*
** Item layer
*/
class ItemLayer : public Layer<Item> {
protected:
    Item *raw_get(Field &f) override { return f.item; }
    void raw_set(Field &f, Item *x) override { f.item = x; }
};

/*
** Stone layer
*/
class StoneLayer : public Layer<Stone> {
public:
    StoneLayer() : Layer<Stone>(&borderstone) {}

protected:
    Stone *raw_get(Field &f) override { return f.stone; }
    void raw_set(Field &f, Stone *st) override { f.stone = st; }

private:
    void dispose(Stone *st) override {
        if (st) {
            SendMessage(st, "disconnect");
            DisposeObject(st);
        }
    }

    /* This stone is used as the virtual border of the playing area.
       It is immovable and indestructible and makes sure the player's
       marble cannot leave the level. */
    class BorderStone : public Stone {
    public:
        BorderStone() : Stone("borderstone") {}
        Stone *clone() override { return this; }
        void dispose() override {}
        const StoneTraits& get_traits() const override {
            static StoneTraits border_traits = {
                    "INVALID", st_borderstone, stf_none, material_stone, 1.0};
            return border_traits;
        }
    };

    BorderStone borderstone;
};

/* ------------- Sound Damping List -------------- */

typedef std::list<sound::SoundDamping> SoundDampingList;

/* -------------------- World -------------------- */

/// Contains the level information (in theory, everything that is
/// local to the current level; in practice, a lot of things are unfortunately
/// still stored in global variables).
class World {
public:
    World(int ww, int hh);
    ~World();

    bool contains(const GridPos& p) const {
        return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height;
    }

    bool contains(const ecl::V2& p) const {
        return p[0] >= 0 && p[1] >= 0 && p[0] < width && p[1] < height;
    }

    bool isBorder(const GridPos &p) const;

    Field *getField(GridPos p) {
        if (contains(p))
            return &fields(p.x, p.y);
        return nullptr;
    }

    void name_object(Object *obj, const std::string &name);
    void unname(Object *);
    Object *getNamed(const std::string &);
    std::list<Object *> get_group(const std::string &tmpl, Object *reference = nullptr);
    void namePosition(const Value& po, const std::string &name);
    Value getNamedPosition(const std::string &name);
    PositionList getPositionList(const std::string &tmpl, Object *reference = nullptr);

    void disposeObject(Object *obj) {
        if (obj) {
            unname(obj);
            obj->dispose();
        }
    }
    
    void tick(double dtime);
    void remove(ForceField *ff);

    void addScramble(GridPos p, Direction dir);
    void scramblePuzzles();

    void add_actor(Actor *a);
    void add_actor(Actor *a, const ecl::V2 &pos);
    Actor *yield_actor(Actor *a);
    void exchange_actors(Actor *a1, Actor *a2);
    void did_move_actor(Actor *a);

private:
    /* ---------- Private methods ---------- */

    ecl::V2 drunkenMouseforce(Actor *actor, const ecl::V2 &mforce);
    ecl::V2 get_local_force(Actor *a);
    ecl::V2 get_global_force(Actor *a);

    void advanceActor(Actor *actor, double &dt);
    void moveActors(double dtime);
    void find_contact_with_stone(Actor* actor, GridPos p, StoneContact& c,
            DirectionBits winFacesActorStone = NODIRBIT, bool isRounded = true,
            Stone* st = nullptr);
    void find_contact_with_edge(Actor* a, GridPos pe, GridPos p1, GridPos p2, StoneContact& c0,
            StoneContact& c1, StoneContact& c2, DirectionBits winFacesActorStone = NODIRBIT);
    void find_contact_with_window(Actor* a, GridPos p, StoneContact& c0, StoneContact& c1,
            DirectionBits winFacesActorStone);
    void find_stone_contacts(Actor *a, StoneContact &c0, StoneContact &c1, StoneContact &c2);
    void handle_stone_contact(StoneContact &sc);
    void handle_actor_contacts();
    void handle_actor_contact(Actor *actor1, Actor *actor2);
    void handle_stone_contacts(unsigned actoridx);
    void stone_change(GridPos p);
    void tick_sound_dampings();
    void doPerformPendingActions();

public:
    static const double contact_e;  // epsilon distant limit for contacts

    /* ---------- Variables ---------- */

    FieldArray fields;  // Contains floors, items, etc.
    int width, height;           // Width and height of the level
    ForceList forceFields;
    ActorList actorList;    // List of movable, dynamic objects
    Actor *leftmostActor;  // sorted doubly linked list of actors
    Actor *rightmostActor;
    OtherList others;
    RubberbandList rubberBands;
    MouseForce mouseForce;
    ecl::V2 globalForce;
    int scrambleIntensity;
    int numMeditatists;
    int indispensableHollows;
    int engagedIndispensableHollows;
    int engagedDispensableHollows;

    //! True if the game is not running yet
    bool preparing_level;

    std::vector<GridPos> changed_stones;
    std::list<GridPos> collisionCriticalPositions;  // stones set to positions after collision check
    bool registerCriticalPositions;
    std::list<Action> actionList;  // pending delayed actions for secure performance

    SoundDampingList sound_dampings;  // see SoundEffectManager for details

    FloorLayer floorLayer;
    ItemLayer itemLayer;
    StoneLayer stoneLayer;

private:
    ecl::Dict<Object *> namedObjects;   // Name -> object mapping
    ecl::Dict<Value> namedPositions;  // Name -> position mapping

    std::list<Scramble> scrambles;
};

}  // namespace enigma
