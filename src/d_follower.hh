/*
 * Copyright (C) 2003, 2015 Daniel Heck
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
#ifndef ENIGMA_D_FOLLOWER_HH_INCLUDED
#define ENIGMA_D_FOLLOWER_HH_INCLUDED

#include "ecl_math.hh"

namespace enigma::display {

class DisplayEngine;

// Base class for different kinds of sprite followers.
class Follower {
public:
    explicit Follower(DisplayEngine *engine);
    virtual ~Follower() = default;

    // This function is called by the display engine during each screen
    // update. 'dtime' is the elapsed time since the last frame in seconds, and 'point'
    // is the position of the sprite that is being followed.
    virtual void tick(double dtime, const ecl::V2 &point) = 0;

    virtual void center(const ecl::V2 &point);

    double getBorderX() const { return borderX; }
    double getBorderY() const { return borderY; }

    void setBorder(double borderX_, double borderY_) {
        borderX = borderX_;
        borderY = borderY_;
    }

protected:
    DisplayEngine *getEngine() const { return engine; }
    bool setOffset(ecl::V2 offset);
    double get_hoff() const;
    double get_voff() const;

private:
    double borderX;
    double borderY;
    DisplayEngine *engine;
};

// Follows a sprite by flipping to the next screen as soon as the sprite
// reaches the border of the current screen.
class Follower_Screen : public Follower {
public:
    explicit Follower_Screen(DisplayEngine *engine, double borderX = 0.5, double borderY = 0.5);
    void tick(double dtime, const ecl::V2 &point) override;
};

// Follows a sprite by softly scrolling the visible area to the next screen as
// soon as the sprite reaches the border of the current screen.
class Follower_Scrolling : public Follower {
public:
    Follower_Scrolling(
            DisplayEngine* engine, bool screenWise, double borderX = 0.5, double borderY = 0.5);
    void tick(double dtime, const ecl::V2 &point) override;
    void center(const ecl::V2 &point) override;

private:
    bool currentlyScrolling;
    ecl::V2 curpos, destpos;
    ecl::V2 dir;
    double scrollSpeed;
    double resttime;
    bool screenwise;
};

// Follows a sprite by keeping it centered on the screen at all times.
class Follower_Smooth : public Follower {
public:
    explicit Follower_Smooth(DisplayEngine *engine);
    void tick(double time, const ecl::V2 &point) override;
    void center(const ecl::V2 &point) override;

    ecl::V2 calcOffset(const ecl::V2 &point);
};

} // namespace enigma::display

#endif  // ENIGMA_D_FOLLOWER_HH_INCLUDED
