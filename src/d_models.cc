/*
 * Copyright (C) 2002,2003,2005 Daniel Heck
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
#include "d_models.hh"

#include "lua.hh"
#include "d_engine.hh"
#include "ecl_cache.hh"
#include "ecl_video.hh"
#include "video.hh"
#include "main.hh"
#include "nls.hh"
#include "gui/ErrorMenu.hh"

#include <cstdio>
#include <cstring>
#include <iostream>

#ifndef CXXLUA
extern "C" {
#include "lualib.h"
#include "tolua++.h"
}
#else
#include "lualib.h"
#include "tolua++.h"
#endif

#include "lua-global.hh"
#include "lua-display.hh"
#include "lua-enigma.hh"
#include "lua-ecl.hh"

using namespace enigma;
using namespace display;


/* -------------------- Types -------------------- */

namespace {
class SurfaceCache_Alpha : public ecl::Cache<ecl::Surface> {
    std::unique_ptr<ecl::Surface> acquire(const std::string &name) override;
};

class SurfaceCache : public ecl::Cache<ecl::Surface> {
    std::unique_ptr<ecl::Surface> acquire(const std::string &name) override;
};

class ModelManager {
public:
    ModelManager();
    ~ModelManager();

    void define(const std::string &name, Model *m);

    /* Create a new model of type 'name'.  Returns 0 if no such
       model exists. */
    std::unique_ptr<Model> create(const std::string &name);

    /* Remove the model definition for 'name'. */
    void remove(const std::string &name);

    bool hasModel(const std::string &name) const;

    size_t num_templates() const;

private:
    // Variables
    using ModelMap = std::unordered_map<std::string, std::unique_ptr<Model>>;
    ModelMap m_templates;
};
}

/* -------------------- SurfaceCache -------------------- */

std::unique_ptr<ecl::Surface> SurfaceCache_Alpha::acquire(const std::string &name) {
    std::string filename;
    std::unique_ptr<ecl::Surface> es;

    FindImageReturnCode found = app.resourceFS->findImageFile(name + ".png", filename);
    if (found != IMAGE_NOT_FOUND)
        es = ecl::LoadImage(filename.c_str());
    if (found == IMAGE_NEEDS_SCALING_32_TO_16)
        return es->zoom(es->width() / 2, es->height() / 2);
    if (found == IMAGE_NEEDS_SCALING_48_TO_64)
        return es->zoom((es->width() * 4) / 3, (es->height() * 4) / 3);
    if (found == IMAGE_NEEDS_SCALING_32_TO_64)
        return es->zoom(es->width() * 2, es->height() * 2);
    return es;
}

std::unique_ptr<ecl::Surface> SurfaceCache::acquire(const std::string &name) {
    std::string filename;
    std::unique_ptr<ecl::Surface> es;

    FindImageReturnCode found = app.resourceFS->findImageFile(name + ".png", filename);
    if (found != IMAGE_NOT_FOUND) {
        es = ecl::LoadImage(filename.c_str());
    }
    if (found == IMAGE_NEEDS_SCALING_32_TO_16)
        return es->zoom(es->width() / 2, es->height() / 2);
    if (found == IMAGE_NEEDS_SCALING_48_TO_64)
        return es->zoom((es->width() * 4) / 3, (es->height() * 4) / 3);
    if (found == IMAGE_NEEDS_SCALING_32_TO_64)
        return es->zoom(es->width() * 2, es->height() * 2);
    return es;
}

/* -------------------- ModelManager -------------------- */

ModelManager::ModelManager() : m_templates(1069) {
}

ModelManager::~ModelManager() = default;

void ModelManager::define(const std::string &name, Model *m) {
    m_templates[name] = std::unique_ptr<Model>(m);
}

std::unique_ptr<Model> ModelManager::create(const std::string &name) {
    auto i = m_templates.find(name);
    return i != m_templates.end() ? std::unique_ptr<Model>(i->second->clone()) : nullptr;
}

void ModelManager::remove(const std::string &name) {
    m_templates.erase(name);
}

bool ModelManager::hasModel(const std::string &name) const {
    return m_templates.find(name) != m_templates.end();
}

size_t ModelManager::num_templates() const {
    return m_templates.size();
}

/* -------------------- Variables -------------------- */

namespace {

SurfaceCache surface_cache;
SurfaceCache_Alpha surface_cache_alpha;
ModelManager *modelmgr = nullptr;
std::string s_currentAnimationName;
Anim2d *s_currentAnimation = nullptr;

}  // namespace

/* -------------------- Functions -------------------- */

void display::InitModels() {
    const VideoTileset *vts = video_engine->GetTileset();

    modelmgr = new ModelManager;

    lua_State *L = lua_open();
    luaL_openlibs(L);
    lua_register(L, "FindDataFile", lua::FindDataFile);
    tolua_open(L);
    tolua_global_open(L);
    tolua_enigma_open(L);
    tolua_display_open(L);
    tolua_px_open(L);

    if (lua::DoSysFile(L, "compat.lua") != lua::NO_LUAERROR) {
        std::string message =
            ecl::strf("Error loading 'compat.lua'\nError: '%s'\n", lua::LastError(L).c_str());
        fprintf(stderr, "%s", message.c_str());
        gui::ErrorMenu m(message + _("\n\nThis error may cause the application to behave strange!"),
                         N_("Continue"));
        m.manage();
    }

    std::string fname;

    fname = app.systemFS->findFile(vts->initscript);
    if (lua::DoSysFile(L, vts->initscript) != lua::NO_LUAERROR) {
        std::string message = ecl::strf("Error loading '%s'\nError: '%s'\n", fname.c_str(),
                                        lua::LastError(L).c_str());
        fprintf(stderr, "%s", message.c_str());
        gui::ErrorMenu m(message + _("\n\nThis error may cause the application to behave strange!"),
                         N_("Continue"));
        m.manage();
    }
    enigma::Log << "# models: " << modelmgr->num_templates() << std::endl;

    surface_cache_alpha.clear();
    lua_close(L);
}

void display::ShutdownModels() {
    delete modelmgr;
    surface_cache.clear();
    s_currentAnimationName = "";
    s_currentAnimation = nullptr;
}

ecl::Surface* display::CropSurface(const ecl::Surface *s, ecl::Rect r) {
    // We have to unbox the unique_ptr here because the pointer is
    // passed to the Lua layer.
    return ecl::Grab(s, r).release();
}

/// Register a new model template `m' under the name `name'.
/// Takes ownership of 'm'.
void display::DefineModel(const char *name, Model *m) {
    if (modelmgr->hasModel(name)) {
        enigma::Log << "Redefining model '" << name << "'\n";
        modelmgr->remove(name);
    }
    modelmgr->define(name, m);
}

std::unique_ptr<Model> display::MakeModel(const std::string &name) {
    if (std::unique_ptr<Model> m = modelmgr->create(name)) {
        return m;
    } else {
        enigma::Log << "Unknown model " << name << std::endl;
        return modelmgr->create("dummy");
    }
}

int display::DefineImage(const char *name, const char *fname, int xoff, int yoff, int padding) {
    ecl::Surface *surface = surface_cache.get(fname);
    if (!surface)
        return 1;

    ecl::Rect r = surface->size();
    r.x += padding;
    r.y += padding;
    r.w -= 2 * padding;
    r.h -= 2 * padding;
    DefineModel(name, new ImageModel(surface, r, xoff + padding, yoff + padding));
    return 0;
}

void display::DefineImageModel(const char *name, ecl::Surface *s) {
    DefineModel(name, new ImageModel(std::unique_ptr<ecl::Surface>(s), 0, 0));
}

int display::DefineSubImage(const char *name, const char *fname, int xoff, int yoff,
                            ecl::Rect subrect) {
    ecl::Surface *sfc = surface_cache.get(fname);
    if (!sfc)
        return 1;

    DefineModel(name, new ImageModel(sfc, subrect, xoff, yoff));
    return 0;
}

void display::DefineRandModel(const char *name, int n, char **names) {
    auto m = new RandomModel();
    for (int i = 0; i < n; i++)
        m->add_model(names[i]);
    DefineModel(name, m);
}

void display::DefineShadedModel(const char *name, const char *model, const char *shadow) {
    DefineModel(name, new ShadowModel(MakeModel(model), MakeModel(shadow)));
}

/* Create an image by overlaying several other images.  The first entry in
   `images' is the name of the background image, the following images are
   drawn on top of it. */
void display::DefineOverlayImage(const char *name, int n, char **images) {
    std::unique_ptr<ecl::Surface> sfc = Duplicate(surface_cache.get(images[0]));
    if (sfc) {
        ecl::GC gc(sfc.get());
        for (int i = 1; i < n; i++)
            blit(gc, 0, 0, surface_cache_alpha.get(images[i]));
        DefineModel(name, new ImageModel(std::move(sfc), 0, 0));
    }
}

void display::DefineComposite(const char *name, const char *bgname, const char *fgname) {
    DefineModel(name, new CompositeModel(MakeModel(bgname), MakeModel(fgname)));
}

void display::DefineAnim(const char *name, bool looping) {
    s_currentAnimation = new Anim2d(looping);
    DefineModel(name, s_currentAnimation);
    s_currentAnimationName = name;
}

void display::AddFrame(const char *name, const char *model, double time) {
    if (s_currentAnimationName != name)
        fprintf(stderr, "AddFrame: Cannot add frames to completed animations.");
    else
        s_currentAnimation->add_frame(MakeModel(model), time / 1000.0);
}

void display::DefineAlias(const char *name, const char *othername) {
    if (std::strcmp(name, othername) != 0)
        DefineModel(name, new AliasModel(othername));
}

/* -------------------- Model -------------------- */
ecl::Rect Model::boundingBox() {
    return ecl::Rect();
}

/* -------------------- Image -------------------- */

Image::Image(std::unique_ptr<ecl::Surface> sfc) : surface(std::move(sfc)), rect(surface->size()) {
}

Image::Image(ecl::Surface *sfc, ecl::Rect r) : surface(Duplicate(sfc)), rect(r) {
}

void display::draw_image(Image *image, ecl::GC &gc, int x, int y) {
    blit(gc, x, y, image->surface.get(), image->rect);
}

/* -------------------- ImageModel -------------------- */

ImageModel::ImageModel(const std::shared_ptr<Image> &image, int xo, int yo)
: image(image), xoff(xo), yoff(yo) {
    assert(image);
}

ImageModel::ImageModel(std::unique_ptr<ecl::Surface> s, int xo, int yo)
: image(std::make_shared<Image>(std::move(s))), xoff(xo), yoff(yo) {
}

ImageModel::ImageModel(ecl::Surface *s, const ecl::Rect &r, int xo, int yo)
: image(std::make_shared<Image>(s, r)), xoff(xo), yoff(yo) {
}

ImageModel::~ImageModel() {
}

void ImageModel::draw(ecl::GC &gc, int x, int y) {
    draw_image(image.get(), gc, x + xoff, y + yoff);
}

std::unique_ptr<Model> ImageModel::clone() {
    return std::make_unique<ImageModel>(image, xoff, yoff);
}

ecl::Rect ImageModel::boundingBox() {
    return ecl::Rect(xoff, yoff, image->rect.w, image->rect.h);
}

/* -------------------- ShadowModel -------------------- */

ShadowModel::ShadowModel(std::unique_ptr<Model> m, std::unique_ptr<Model> sh)
: model(std::move(m)), shadow(std::move(sh)) {
    bbox = ecl::boundingbox(model->boundingBox(), shadow->boundingBox());
}

ShadowModel::~ShadowModel() {
}

void ShadowModel::expose(ModelLayer *ml, int vx, int vy) {
    model->expose(ml, vx, vy);
    shadow->expose(ml, vx, vy);
}
void ShadowModel::remove(ModelLayer *ml) {
    shadow->remove(ml);
    model->remove(ml);
}

void ShadowModel::set_callback(ModelCallback *cb) {
    model->set_callback(cb);
}

void ShadowModel::reverse() {
    model->reverse();
    shadow->reverse();
}

void ShadowModel::restart() {
    model->restart();
    shadow->restart();
}

void ShadowModel::draw(ecl::GC &gc, int x, int y) {
    model->draw(gc, x, y);
}

void ShadowModel::draw_shadow(ecl::GC &gc, int x, int y) {
    shadow->draw(gc, x, y);
}

Model *ShadowModel::get_shadow() const {
    return shadow.get();
}

std::unique_ptr<Model> ShadowModel::clone() {
    return std::make_unique<ShadowModel>(model->clone(), shadow->clone());
}

ecl::Rect ShadowModel::boundingBox() {
    return bbox;
}

/* -------------------- RandomModel -------------------- */

std::unique_ptr<Model> RandomModel::clone() {
    if (!modelnames.empty()) {
        int r = IntegerRand(0, modelnames.size() - 1, false);
        return MakeModel(modelnames[r]);
    } else {
        fprintf(stderr, "display_2d.cc: empty RandomModel\n");
        return nullptr;
    }
}

/* -------------------- AliasModel -------------------- */

std::unique_ptr<Model> AliasModel::clone() {
    return MakeModel(name);
}

/* -------------------- Anim2d -------------------- */

Anim2d::Anim2d(bool looping) : rep(std::make_shared<AnimRep>(looping)) {
}

Anim2d::Anim2d(const std::shared_ptr<AnimRep> &rep, const ecl::Rect &bbox) : rep(rep), bbox(bbox) {
    frametime = 0;
}

void Anim2d::set_callback(ModelCallback *cb) {
    callback = cb;
}

void Anim2d::reverse() {
    reversep = !reversep;
}

void Anim2d::restart() {
    finishedp = false;
    frametime = 0;
    curframe = 0;
    changedp = true;
}

void Anim2d::add_frame(std::unique_ptr<Model> m, double duration) {
    ecl::Rect frameBbox = m->boundingBox();
    rep->frames.push_back(std::make_unique<AnimFrame>(std::move(m), duration));

    // Cache the bounding box of all frames to ensure that it is constant.
    bbox = ecl::boundingbox(frameBbox, bbox);
}

void Anim2d::draw(ecl::GC &gc, int x, int y) {
    if (!finishedp) {
        AnimFrame *f = rep->frames[curframe].get();
        f->model->draw(gc, x, y);
        changedp = false;
    }
}

void Anim2d::draw_shadow(ecl::GC &gc, int x, int y) {
    if (!finishedp) {
        AnimFrame *f = rep->frames[curframe].get();
        f->model->draw_shadow(gc, x, y);
    }
}

std::unique_ptr<Model> Anim2d::clone() {
    return std::make_unique<Anim2d>(rep, bbox);
}

void Anim2d::expose(ModelLayer *ml, int vx, int vy) {
    ml->activate(this);
    videox = vx;
    videoy = vy;
}

void Anim2d::remove(ModelLayer *ml) {
    ml->deactivate(this);
}

bool Anim2d::has_changed(ecl::Rect &r) {
    bool retval = changedp;
    if (changedp) {
        r = boundingBox();
        r.x += videox;
        r.y += videoy;
    }
    return retval;
}

void Anim2d::move(int newx, int newy) {
    videox = newx;
    videoy = newy;
}

ecl::Rect Anim2d::boundingBox() {
    return bbox;
}

void Anim2d::tick(double dtime) {
    assert(curframe < rep->frames.size());
    frametime += dtime;
    double framedur = rep->frames[curframe]->duration;

    if (frametime >= framedur) {
        frametime -= framedur;
        changedp = true;

        if (reversep) {
            if (curframe >= 1)
                curframe--;
            else if (rep->looping)
                curframe = rep->frames.size() - 1;
            else
                finishedp = true;
        } else {
            if (curframe + 1 < rep->frames.size())
                curframe++;
            else if (rep->looping)
                curframe = 0;
            else
                finishedp = true;
        }
        if (finishedp && callback != nullptr)
            callback->animcb();
    }
}

/* -------------------- Functions -------------------- */

namespace display {

ecl::Surface *GetSurface(const std::string &filename) {
    return surface_cache.get(filename);
}

}  // namespace display
