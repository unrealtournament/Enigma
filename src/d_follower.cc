/*
 * Copyright (C) 2002,2003,2004,2005,2015 Daniel Heck
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
#include "d_follower.hh"

#include "d_engine.hh"
#include "ecl_math.hh"
#include "errors.hh"

#include <algorithm>

namespace enigma::display {

/* -------------------- Follower -------------------- */

Follower::Follower(DisplayEngine* engine) : borderX(0.5), borderY(0.5), m_engine(engine) {
}

void Follower::center(const ecl::V2 &point) {
    double hoff = get_hoff();
    double voff = get_voff();

    ecl::V2 off = point;
    off[0] = floor((off[0] - borderX) / hoff) * hoff;
    off[1] = floor((off[1] - borderY) / voff) * voff;

    set_offset(off);
}

double Follower::get_hoff() const {
    ScreenArea gamearea = m_engine->get_area();
    double result = gamearea.w / m_engine->get_tilew() - borderX * 2;
    ASSERT(result > 0, XLevelRuntime,
           "FollowThreshold must be less than half of level width/height and 10/6.5");
    return result;
}

double Follower::get_voff() const {
    ScreenArea gameArea = m_engine->get_area();
    double result = gameArea.h / m_engine->get_tileh() - borderY * 2;
    ASSERT(result > 0, XLevelRuntime,
           "FollowThreshold must be less than half of level width/height and 10/6.5");
    return result;
}

bool Follower::set_offset(ecl::V2 offset) {
    DisplayEngine *engine = getEngine();
    offset[0] = std::max(offset[0], 0.0);
    offset[1] = std::max(offset[1], 0.0);
    offset[0] = std::min(offset[0], double(engine->get_width() - get_hoff() - borderX * 2));
    offset[1] = std::min(offset[1], double(engine->get_height() - get_voff() - borderY * 2));
    if (offset != engine->get_offset()) {
        engine->set_offset(offset);
        return true;
    }
    return false;
}

/* -------------------- Follower_Screen -------------------- */

// Determine whether the screen must be scrolled or not and change the
// coordinate origin of the screen accordingly.
void Follower_Screen::tick(double, const ecl::V2 &point) {
    DisplayEngine *engine = getEngine();
    ecl::V2 oldoff = engine->get_offset();
    Follower::center(point);
    if (oldoff != engine->get_offset())
        engine->mark_redraw_screen();
}

Follower_Screen::Follower_Screen(DisplayEngine* engine, double borderX, double borderY)
    : Follower(engine) {
    setBorder(borderX, borderY);
}

/* -------------------- Follower_Scrolling -------------------- */

Follower_Scrolling::Follower_Scrolling(DisplayEngine *engine, bool screenWise, double borderX,
                                       double borderY)
: Follower(engine), currently_scrolling(false), scrollspeed(0), resttime(0), screenwise(screenWise) {
    setBorder(borderX, borderY);
}

void Follower_Scrolling::center(const ecl::V2 &point) {
    Follower::center(point);
    curpos = destpos = getEngine()->get_offset();
}

void Follower_Scrolling::tick(double dtime, const ecl::V2 &point) {
    DisplayEngine *engine = getEngine();

    if (!currently_scrolling) {
        ScreenArea gamearea = engine->get_area();
        int tilew = engine->get_tilew();
        int tileh = engine->get_tileh();

        int sx, sy;
        engine->world_to_screen(point, &sx, &sy);

        bool scrollx_p = (sx < gamearea.x + getBorderX() * tilew)
                || (sx >= gamearea.x + gamearea.w - getBorderX() * tilew);

        bool scrolly_p = (sy < gamearea.y + getBorderY() * tileh)
                || (sy >= gamearea.y + gamearea.h - getBorderY() * tileh);

        if (scrollx_p || scrolly_p) {
            ecl::V2 olddest = destpos;
            ecl::V2 scrollpos = engine->get_offset();

            currently_scrolling = true;

            // Move 'point' to center of the screen
            curpos = scrollpos;

            if (screenwise) {
                double hoff = get_hoff();
                double voff = get_voff();
                destpos[0] = floor((point[0] - getBorderX()) / hoff) * hoff;
                destpos[1] = floor((point[1] - getBorderY()) / voff) * voff;
            } else {
                destpos = point - ecl::V2(gamearea.w / tilew, gamearea.h / tileh) / 2;
                // round to grid - a hack just for 20x13 screen TODO rewrite for Enigma 1.2
                // x scroll of "alternating" 10 and 9 grids, try to join this grid after warps
                double xmod = std::fmod(destpos[0], 19);
                if (xmod < 5 || (xmod > 10 && xmod < 14.5))
                    destpos[0] = std::floor(destpos[0]);
                else
                    destpos[0] = std::ceil(destpos[0]);
                // y scroll of stable 6 grids
                destpos[1] = ecl::round_nearest<int>(destpos[1]);
            }

            // Don't scroll off the game area
            destpos[0] = ecl::Clamp<double>(
                    destpos[0], 0.0, (double)engine->get_width() - gamearea.w / tilew);
            destpos[1] = ecl::Clamp<double>(
                    destpos[1], 0.0, (double)engine->get_height() - gamearea.h / tileh);
            if (!scrollx_p)
                destpos[0] = olddest[0];
            if (!scrolly_p)
                destpos[1] = olddest[1];
        }
    }

    if (currently_scrolling) {
        scrollspeed = 45.0;
        resttime = length(destpos - curpos) / scrollspeed;

        resttime -= dtime;
        if (resttime <= 0) {
            engine->move_offset(destpos);
            currently_scrolling = false;
        } else {
            dir = normalize(destpos - curpos);
            curpos += dir * scrollspeed * dtime;
            engine->move_offset(curpos);
        }
    }
}

/* -------------------- Follower_Smooth -------------------- */

Follower_Smooth::Follower_Smooth(DisplayEngine *engine) : Follower(engine) {
}

ecl::V2 Follower_Smooth::calcOffset(const ecl::V2 &point) {
    DisplayEngine *engine = getEngine();
    ScreenArea gameArea = engine->get_area();
    int tileWidth = engine->get_tilew();
    int tileHeight = engine->get_tileh();

    ecl::V2 destPos = point
            - ecl::V2(double(gameArea.w) / tileWidth, double(gameArea.h) / tileHeight) / 2;
    // Round to integer pixel offset
    destPos[0] = ecl::round_nearest<int>(destPos[0] * tileWidth) / double(tileWidth);
    destPos[1] = ecl::round_nearest<int>(destPos[1] * tileHeight) / double(tileHeight);
    destPos[0] = ecl::Clamp(destPos[0], 0.0, (double)engine->get_width() - gameArea.w / tileWidth);
    destPos[1] = ecl::Clamp(
            destPos[1], 0.0, (double)engine->get_height() - gameArea.h / tileHeight);
    return destPos;
}

void Follower_Smooth::tick(double /*time*/, const ecl::V2 &point) {
    getEngine()->move_offset(calcOffset(point));
}

void Follower_Smooth::center(const ecl::V2 &point) {
    setBorder(0.5, 0.5);
    set_offset(calcOffset(point));
}

}  // namespace display
