/*
 * Copyright (C) 2002,2003,2004 Daniel Heck
 * Copyright (C) 2007,2008,2009,2010 Ronald Lamprecht
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

#include "stones/WindowStone.hh"

#include "actors.hh"
#include "items.hh"
#include "main.hh"
#include "player.hh"
#include "server.hh"
#include "world.hh"

#include <vector>

namespace enigma {
    
    WindowStone::WindowStone(const std::string& faces) {
        setAttr("faces", faces);
    }
    
    void WindowStone::setAttr(const std::string &key, const Value &val) {
        if (key == "secure") {
            if (val.toBool() != (objFlags & OBJBIT_SECURE)) {
                if (val.toBool()) {
                    objFlags |= OBJBIT_SECURE;
                } else {
                    objFlags &= ~OBJBIT_SECURE;
                }
                if (isDisplayable())
                    init_model();
                return;
            }
        } else if (key == "scratches") {
            int d = NODIRBIT;
            std::string vs = val.toString();
            if (vs.find('n') != std::string::npos) d |= NORTHBIT;
            if (vs.find('e') != std::string::npos) d |= EASTBIT;
            if (vs.find('s') != std::string::npos) d |= SOUTHBIT;
            if (vs.find('w') != std::string::npos) d |= WESTBIT;
            objFlags &= ~OBJBIT_SCRATCHDIRS;
            objFlags |= ((d & getFaces())<< 24) ;
            if (isDisplayable())
                init_model();
            return;
        }
        Stone::setAttr(key, val);
    }
    
    Value WindowStone::getAttr(const std::string &key) const {
        if (key == "secure") {
            return (bool)(objFlags & OBJBIT_SECURE);
        }
        if (key == "scratches") {
            std::string result;
            DirectionBits db = (DirectionBits)((objFlags & OBJBIT_SCRATCHDIRS) >> 24);
            if (db & NORTHBIT)
                result += "n";
            if (db & EASTBIT)
                result += "e";
            if (db & SOUTHBIT)
                result += "s";
            if (db & WESTBIT)
                result += "w";
            return result;
        }
        return Stone::getAttr(key);
    }
    
    Value WindowStone::message(const Message &m) {
        if (m.message == "inner_pull" ) {
            return Value(tryMoveWindowFace(to_direction(m.value)));
        } else if (m.message == "_explosion") {
            GridPos expl = m.value.toGridPos();
            DirectionBits faces = getFaces();
            DirectionBits scratchDirs = (DirectionBits)((objFlags & OBJBIT_SCRATCHDIRS) >> 24);
            if ((faces & WESTBIT) && ((move(get_pos(), WEST) == expl) || (get_pos() == expl)) 
                    && (!(objFlags & OBJBIT_SECURE) || (scratchDirs & WESTBIT)))
                breakFaces(WESTBIT);
            if ((faces & SOUTHBIT) && ((move(get_pos(), SOUTH) == expl) || (get_pos() == expl))
                    && (!(objFlags & OBJBIT_SECURE) || (scratchDirs & SOUTHBIT)))
                breakFaces(SOUTHBIT);
            if ((faces & EASTBIT) && ((move(get_pos(), EAST) == expl) || (get_pos() == expl))
                    && (!(objFlags & OBJBIT_SECURE) || (scratchDirs & EASTBIT)))
                breakFaces(EASTBIT);
            if ((faces & NORTHBIT) && ((move(get_pos(), NORTH) == expl) || (get_pos() == expl))
                    && (!(objFlags & OBJBIT_SECURE) || (scratchDirs & NORTHBIT)))
                breakFaces(NORTHBIT);
            
            return Value();
        }
        return Stone::message(m);
    }
    
    int WindowStone::externalState() const {
        return 0;
    }
    
    void WindowStone::setState(int /*extState*/) {
        // ignore any state access
    }

    DirectionBits WindowStone::getFaces(bool actorInvisible) const {
        if (!actorInvisible || objFlags & OBJBIT_SECURE)
            return Stone::getFaces(actorInvisible);
        else
            return NODIRBIT;
    }
    
    void WindowStone::init_model() {
        if (state == IDLE) {   // anims will not be cleared 
            if (getFaces() == NODIRBIT) {   // a tease of level author tried to set a faceless window!
                KillStone(get_pos());
                return;
            }
            uint32_t scratchDirs = ((objFlags & OBJBIT_SCRATCHDIRS) >> 24);
            set_model(ecl::strf("st_window_%s%d_%d", objFlags & OBJBIT_SECURE ? "green" : "blue",
                    getFaces() & ~scratchDirs, scratchDirs));                    
        }
    }

    void WindowStone::animcb() {
        postFaceChange();
        if (state == FINALBREAK) {
            KillStone(get_pos());
        } else {
            state = IDLE;
            init_model();
        }
    }
    
    bool WindowStone::allowsSpreading(Direction dir, bool /*isFlood*/) const {
        return (dir != NODIR) ? !has_dir(getFaces(), dir) : true;
    }
    
    void WindowStone::actor_hit(const StoneContact &sc) {
        if (state != IDLE)
            return;

        Actor *actor = sc.actor;
        double impulse = -(actor->get_vel() * sc.normal) * get_mass(actor);
        double threshold = 22;
        if (player::WieldedItemIs(sc.actor, "it_hammer"))
            threshold -= 7;
        if (objFlags & (sc.faces << 24)) // face is scratched
            threshold -= 10;

        if (impulse > 35 && !(objFlags & OBJBIT_SECURE)) {
            SendMessage(actor, "_shatter");
        } else if (!(objFlags & OBJBIT_SECURE) && impulse > threshold) {
            breakFaces(sc.faces);
        } else if (player::WieldedItemIs(sc.actor, "it_wrench")) {
            if (sc.faces == WESTBIT && sc.normal[0] < 0) {
                tryMoveWindowFace(EAST, actor);
            } else if (sc.faces == EASTBIT && sc.normal[0] > 0) {
                tryMoveWindowFace(WEST, actor);
            } else if (sc.faces == SOUTHBIT && sc.normal[1] > 0) {
                tryMoveWindowFace(NORTH, actor);
            } else if (sc.faces == NORTHBIT && sc.normal[1] < 0) {
                tryMoveWindowFace(SOUTH, actor);
            }
        } else if (player::WieldedItemIs(sc.actor, "it_ring")) {
            objFlags |= ((sc.faces & getFaces()) << 24); // scratch face
            sound_event("crack");
            init_model();
        }
    }
    
    bool WindowStone::is_sticky(const Actor */*a*/) const  {
        return false;
    }
    
    bool WindowStone::on_move(const GridPos &/*origin*/) {
        // do not shatter actors
        return true;
    }

    // Thickness of a window face
    constexpr double faceWidth = 3.0/32.0;

    StoneResponse WindowStone::collision_response(const StoneContact &sc) {
        // blue windows let invisible actors pass
        if (!(objFlags & OBJBIT_SECURE) && sc.actor->is_invisible()) 
            return STONE_PASS;
            
        DirectionBits faces = getFaces();
        if (faces & WESTBIT && sc.contactPoint[0] <= get_pos().x + faceWidth
                || faces & EASTBIT && sc.contactPoint[0] >= get_pos().x + 1 - faceWidth
                || faces & NORTHBIT && sc.contactPoint[1] <= get_pos().y + faceWidth
                || faces & SOUTHBIT && sc.contactPoint[1] >= get_pos().y + 1 - faceWidth) {
            return STONE_REBOUND;
        }
        return STONE_PASS;
    }
    
    void WindowStone::breakFaces(DirectionBits faces) {
        DirectionBits remainigFaces = (DirectionBits)(getFaces() & ~faces);  // remove breaking face
        objFlags &= ~(faces << 24);  // remove scratch mark from old position
        Object::setAttr("$connections", ALL_DIRECTIONS ^ remainigFaces);     // avoid init of model
        sound_event("shatter");
        state = (remainigFaces == NODIRBIT) ? FINALBREAK : BREAK;
        
        uint32_t scratchDirs = ((objFlags & OBJBIT_SCRATCHDIRS) >> 24);
        set_anim(ecl::strf("st_window_%s%d_%d_anim",  objFlags & OBJBIT_SECURE ? "green" : "blue",
            getFaces() & ~scratchDirs, scratchDirs));
        if (server::GameCompatibility == GAMET_OXYD1)
            KillItem(get_pos());
    }
    
    bool WindowStone::tryMoveWindowFace(Direction dir, const Actor *initiator) {
        // Stop if there is no face to move or there is already a face on the opposite side.
        if (!has_dir(getFaces(), reverse(dir)) || has_dir(getFaces(), dir))
            return false;

        // Stop if there is a stone on the neighboring field that is not a window
        // or if the neighboring window would block the moved face.
        GridPos neighborPos = move(get_pos(), dir);
        Stone* neighborWindow = GetStone(neighborPos);
        if (neighborWindow
                && (neighborWindow->get_traits().id != st_window
                        || has_dir(neighborWindow->getFaces(), reverse(dir)))) {
            return false;
        }

        DirectionBits startFaceBit = to_bits(reverse(dir));
        DirectionBits endFaceBit = to_bits(dir);
        DirectionBits unchangedFaces = static_cast<DirectionBits>(
                (getFaces() & ~startFaceBit) | endFaceBit);
        Object::setAttr("$connections", ALL_DIRECTIONS ^ unchangedFaces); // avoid init of model

        // Transfer scratches
        if (((objFlags & OBJBIT_SCRATCHDIRS) >> 24) & startFaceBit)
            objFlags |= (endFaceBit << 24); // mark moved face as scratched
        objFlags &= ~(startFaceBit << 24);  // remove scratch mark from old position
        init_model();

        // Move item to neighboring field or squash it there is already an item.
        if (Item* it = GetItem(get_pos()); it != nullptr && !it->isStatic()) {
            if (Item* neighborItem = GetItem(neighborPos); neighborItem == nullptr)
                SetItem(neighborPos, YieldItem(get_pos()));
            else
                SetItem(get_pos(), MakeItem("it_squashed"));
        }

        // Move actors
        // we do not have to worry about the level border as no face can be pushed into the border
        std::vector<Actor*> foundActors;
        const double range_one_field = 1.415
                + Actor::get_max_radius(); // approx. 1 field [ > sqrt(1+1) ]
        GetActorsInRange(get_pos().center(), range_one_field, foundActors);
        for (auto actor : foundActors) {
            if (actor == initiator)
                continue;
            ecl::V2 actorPos = actor->get_pos();
            double radius = actor->getRadius();
            GridPos windowPos(get_pos());

            // Actors in the current field are always moved. Actors in the neighboring
            // field are only moved if they touch the moved face of the window.
            const bool actorAffected = (GridPos(actorPos) == windowPos)
                    || (GridPos(actorPos) == neighborPos // or actor is in neighboring field
                            && (dir == EAST && actorPos[0] - radius < windowPos.x + 1
                                    || dir == WEST && actorPos[0] + radius > windowPos.x
                                    || dir == SOUTH && actorPos[1] - radius < windowPos.y + 1
                                    || dir == NORTH && actorPos[1] + radius > windowPos.y));
            if (!actorAffected)
                continue;

            auto hasWindowFace = [](GridPos pos, Direction dir) {
                Stone* stone = GetStone(pos);
                return stone != nullptr && stone->get_traits().id == st_window
                        && has_dir(stone->getFaces(), dir);
            };
            auto isBlocked = [actor](GridPos pos, Direction dir) -> bool {
                Stone* obstacle = GetStone(move(pos, dir));
                return obstacle != nullptr
                        && ((obstacle->get_traits().id == st_window
                                    && has_dir(obstacle->getFaces(), reverse(dir)))
                                || obstacle->is_sticky(actor));
            };

            // Compute updated position for actor
            ecl::V2 dest = actor->get_pos();
            if (dir == EAST || dir == WEST) {
                dest[0] = dir == EAST ? windowPos.x + 1 + radius : windowPos.x - radius;
                if (hasWindowFace(neighborPos, NORTH)) {
                    dest[1] = ecl::Max(dest[1], windowPos.y + radius + faceWidth);
                } else if (isBlocked(neighborPos, NORTH)) {
                    dest[1] = ecl::Max(dest[1], windowPos.y + radius);
                }
                if (hasWindowFace(neighborPos, SOUTH)) {
                    dest[1] = ecl::Min(dest[1], windowPos.y + 1 - radius - faceWidth);
                } else if (isBlocked(neighborPos, SOUTH)) {
                    dest[1] = ecl::Min(dest[1], windowPos.y + 1 - radius);
                }
            } else {
                dest[1] = dir == SOUTH ? windowPos.y + 1 + radius : windowPos.y - radius;
                if (hasWindowFace(neighborPos, WEST)) {
                    dest[0] = ecl::Max(dest[0], windowPos.x + radius + faceWidth);
                } else if (isBlocked(neighborPos, WEST)) {
                    dest[0] = ecl::Max(dest[0], windowPos.x + radius);
                }
                if (hasWindowFace(neighborPos, EAST)) {
                    dest[0] = ecl::Min(dest[0], windowPos.x + 1 - radius - faceWidth);
                } else if (isBlocked(neighborPos, EAST)) {
                    dest[0] = ecl::Min(dest[0], windowPos.x + 1 - radius);
                }
            }
            WarpActor(actor, dest[0], dest[1], true);
        }
        TouchStone(get_pos()); // avoid another actor getting below a moved window face
        postFaceChange();
        return true;
    }

    void WindowStone::postFaceChange() {
        SendMessage(GetFloor(get_pos()), "_checkflood");
        for (Direction d = NORTH; d != NODIR; d = previous(d))
            SendMessage(GetFloor(move(get_pos(), d)), "_checkflood");
    }

    
    DEF_TRAITSM(WindowStone, "st_window", st_window, MOVABLE_BREAKABLE);

    BOOT_REGISTER_START
        BootRegister(new WindowStone("s"), "st_window");    // compatibility window with south face only
        BootRegister(new WindowStone("w"), "st_window_w");
        BootRegister(new WindowStone("s"), "st_window_s");
        BootRegister(new WindowStone("sw"), "st_window_sw");
        BootRegister(new WindowStone("e"), "st_window_e");
        BootRegister(new WindowStone("ew"), "st_window_ew");
        BootRegister(new WindowStone("es"), "st_window_es");
        BootRegister(new WindowStone("esw"), "st_window_esw");
        BootRegister(new WindowStone("n"), "st_window_n");
        BootRegister(new WindowStone("nw"), "st_window_nw");
        BootRegister(new WindowStone("ns"), "st_window_ns");
        BootRegister(new WindowStone("nsw"), "st_window_nsw");
        BootRegister(new WindowStone("ne"), "st_window_ne");
        BootRegister(new WindowStone("new"), "st_window_new");
        BootRegister(new WindowStone("nes"), "st_window_nes");
        BootRegister(new WindowStone("nesw"), "st_window_nesw");
    BOOT_REGISTER_END

} // namespace enigma
