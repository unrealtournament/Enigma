/*
 * Copyright (C) 2002,2003,2004,2005 Daniel Heck
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
#ifndef DISPLAY_HH_INCLUDED
#define DISPLAY_HH_INCLUDED

#include "enigma.hh"
#include "ecl_geom.hh"
#include "ecl_math.hh"
#include "ecl_video.hh"

namespace enigma::display {

//----------------------------------------
// Definition of models
//----------------------------------------

class ModelLayer;

// Animations can invoke a callback of this type on completion. Note that you
// may not delete or replace models other than the one that triggered the
// callback from inside the callback; use a timer or a flag to do this.
class ModelCallback {
public:
    virtual ~ModelCallback() = default;
    virtual void animcb() = 0;
};

class Model {
public:
    virtual ~Model() = default;
    virtual void setCallback(ModelCallback *) {}
    virtual void reverse() {}
    virtual void restart() {}

    [[nodiscard]] virtual bool hasFinished() const { return false; }
    virtual void tick(double /*dtime*/) {}
    virtual bool hasChanged(ecl::Rect & /*changed_region*/) { return false; }

    virtual void draw(ecl::GC & /*gc*/, int /*x*/, int /*y*/) {}
    virtual void drawShadow(ecl::GC & /*gc*/, int /*x*/, int /*y*/) {}

    [[nodiscard]] virtual Model *get_shadow() const { return nullptr; }

    virtual void expose(ModelLayer * /*ml*/, int /*videox*/, int /*videoy*/) {}
    virtual void removeFromLayer(ModelLayer * /*ml*/) {}

    virtual std::unique_ptr<Model> clone() = 0;
    
    virtual ecl::Rect boundingBox();
};

/* -------------------- Functions -------------------- */

void InitModels();
void ShutdownModels();

std::unique_ptr<Model> MakeModel(const std::string &name);

int DefineImage(const char *name, const char *fname, int xoff, int yoff, int padding);
int DefineSubImage(const char *name, const char *fname, int xOff, int yOff, ecl::Rect subRect);
void DefineRandModel(const char *name, int n, char **names);
void DefineShadedModel(const char *name, const char *model, const char *shade);
void DefineOverlayImage(const char *name, int n, char **images);
void DefineComposite(const char *name, const char *bgname, const char *fgname);
void DefineAnim(const char *name, bool looping);
void AddFrame(const char *name, const char *model, double time);
void DefineAlias(const char *name, const char *othername);

//----------------------------------------
// Models on the grid
//----------------------------------------

Model *SetModel(const GridLoc &l, const std::string &modelname);
Model *SetModel(const GridLoc &l, std::unique_ptr<Model> m);
void KillModel(const GridLoc &l);
Model *GetModel(const GridLoc &l);
std::unique_ptr<Model> YieldModel(const GridLoc &l);

/* -------------------- Scrolling -------------------- */

enum FollowMode {
    FOLLOW_NONEOLD = 0,          // Don't follow any sprite
    FOLLOW_SCROLLING = 1,        // Scroll the screen
    FOLLOW_SCREEN = 2,           // Flip the screen region
    FOLLOW_SCREENSCROLLING = 3,  // Scroll to the next screen
    FOLLOW_SMOOTH = 4,           // Follow pixel by pixel
};

enum FollowType {
    FOLLOW_NONE = 0,    // Don't follow any sprite
    FOLLOW_SCROLL = 1,  // Scroll pixelwise
    FOLLOW_FLIP = 2,    // Flip the display to destination
};

void SetFollowMode(FollowMode m);
void UpdateFollowMode();
void SetScrollBoundary(double boundary);

void SetReferencePoint(const ecl::V2 &point);
void GetReferencePointCoordinates(int *x, int *y);
void FocusReferencePoint();

/* -------------------- Sprites -------------------- */

enum SpriteLayer {
    SPRITE_ACTOR,
    SPRITE_EFFECT,
    SPRITE_DEBRIS
};

typedef unsigned int SpriteId;

class DL_Sprites;

class SpriteHandle {
    DL_Sprites *layer;
    unsigned id;

public:
    SpriteHandle(DL_Sprites *l, unsigned spriteid);
    SpriteHandle();

    void kill();
    void move(const ecl::V2 &newpos) const;
    void replace_model(std::unique_ptr<Model> m) const;
    [[nodiscard]] Model *get_model() const;
    void set_callback(ModelCallback *cb) const;
    void hide() const;
    void show() const;
};

// Add a new effect sprite. Sprites of this type are automatically deleted
// once the animation has finished.
SpriteHandle AddEffect(const ecl::V2 &pos, const char *modelname, bool isDispensible = false);

// Create a new sprite. If modelname==0, the sprite is considered invisible.
// Sprites of this type are _never_ automatically deleted.
SpriteHandle AddSprite(const ecl::V2 &pos, const char *modelname = nullptr);

/* -------------------- Rubber bands -------------------- */

class DL_Lines;

class LineHandle {
public:
    explicit LineHandle(DL_Lines *layer = nullptr, unsigned id = 0);
    unsigned getId() const { return id; }

    void setStartPoint(const ecl::V2 &start);
    void setEndPoint(const ecl::V2 &end);
    void kill();
private:
    DL_Lines *lineLayer;
    unsigned id;
};

// Adds a rubber band between points p1 and p2.
LineHandle AddRubber(const ecl::V2& start, const ecl::V2& end, unsigned short red,
        unsigned short green, unsigned short blue, bool isThick);

/* -------------------- Status bar -------------------- */

class StatusBar {
public:
    virtual ~StatusBar() = default;
    virtual void setInventory(Player activePlayer, const std::vector<std::string>& modelNames) = 0;

    virtual void showText(const std::string &str, bool scrolling, double duration) = 0;
    virtual void hideText() = 0;

    virtual void showMoveCounter(bool active) = 0;
    virtual void setCMode(bool flag) = 0;
    virtual void setBasicModes(std::string flags) = 0;

    virtual void setTime(double time) = 0;
    virtual void setSpeed(double speed) = 0;
    virtual void setTravelledDistance(double distance) = 0;
    virtual void setCounter(int nummoves) = 0;
};

StatusBar *GetStatusBar();

/* -------------------- Constants -------------------- */

const int MIN_TextSpeed = 1;
const int MAX_TextSpeed = 20;
const int DEFAULT_TextSpeed = 8;
const int FACTOR_TextSpeed = 20;  // Multiplicative factor for text speed

/* -------------------- Interface to display engine -------------------- */

enum DisplayFlags {
    SHOW_FLOOR = 0x01,
    SHOW_STONES = 0x02,
    SHOW_ITEMS = 0x04,
    SHOW_SHADES = 0x08,
    SHOW_SPRITES = 0x10,
    SHOW_TIME = 0x20,
    SHOW_INVENTORY = 0x40,
    SHOW_ALL = 0x7f
};

void ToggleFlag(DisplayFlags flag);

void Init(bool showFps = false);
void Shutdown();

void NewWorld(int w, int h);
void ResizeGameArea(int w, int h);
const ecl::Rect &GetGameArea();

void DrawAll(ecl::GC &gc);
void RedrawAll(ecl::Screen *screen);
void Redraw(ecl::Screen *screen);
void Tick(double dtime);

void SetTextSpeed(int newSpeed);
int GetTextSpeed();

} // namespace enigma::display

#endif
