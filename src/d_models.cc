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

using namespace enigma;
using namespace display;

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

/* -------------------- Types -------------------- */

namespace {
class SurfaceCache_Alpha : public ecl::PtrCache<ecl::Surface> {
    ecl::Surface *acquire(const std::string &name) override;
};

class SurfaceCache : public ecl::PtrCache<ecl::Surface> {
    ecl::Surface *acquire(const std::string &name) override;
};

class ModelManager {
public:
    ModelManager();
    ~ModelManager();

    void define(const std::string &name, Model *m);

    /* Create a new model of type 'name'.  Returns 0 if no such
       model exists. */
    Model *create(const std::string &name);

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

ecl::Surface *SurfaceCache_Alpha::acquire(const std::string &name) {
    std::string filename;
    std::unique_ptr<ecl::Surface> es;

    FindImageReturnCode found = app.resourceFS->findImageFile(name + ".png", filename);
    if (found != IMAGE_NOT_FOUND)
        es.reset(ecl::LoadImage(filename.c_str()));
    if (found == IMAGE_NEEDS_SCALING_32_TO_16)
        return es->zoom(es->width() / 2, es->height() / 2);
    if (found == IMAGE_NEEDS_SCALING_48_TO_64)
        return es->zoom((es->width() * 4) / 3, (es->height() * 4) / 3);
    if (found == IMAGE_NEEDS_SCALING_32_TO_64)
        return es->zoom(es->width() * 2, es->height() * 2);
    return es.release();
}

ecl::Surface *SurfaceCache::acquire(const std::string &name) {
    std::string filename;
    std::unique_ptr<ecl::Surface> es;

    FindImageReturnCode found = app.resourceFS->findImageFile(name + ".png", filename);
    if (found != IMAGE_NOT_FOUND) {
        es.reset(ecl::LoadImage(filename.c_str()));
    }
    if (found == IMAGE_NEEDS_SCALING_32_TO_16)
        return es->zoom(es->width() / 2, es->height() / 2);
    if (found == IMAGE_NEEDS_SCALING_48_TO_64)
        return es->zoom((es->width() * 4) / 3, (es->height() * 4) / 3);
    if (found == IMAGE_NEEDS_SCALING_32_TO_64)
        return es->zoom(es->width() * 2, es->height() * 2);
    return es.release();
}

/* -------------------- ModelManager -------------------- */

ModelManager::ModelManager() : m_templates(1069) {
}

ModelManager::~ModelManager() {
}

void ModelManager::define(const std::string &name, Model *m) {
    m_templates[name] = std::unique_ptr<Model>(m);
}

Model *ModelManager::create(const std::string &name) {
    auto i = m_templates.find(name);
    return i != m_templates.end() ? i->second->clone() : nullptr;
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
std::vector<ecl::Surface *> image_pile;
std::string anim_templ_name;
Anim2d *anim_templ = nullptr;

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
    delete_sequence(image_pile.begin(), image_pile.end());
    image_pile.clear();
    anim_templ_name = "";
    anim_templ = nullptr;
}

ecl::Surface *display::CropSurface(const ecl::Surface *s, ecl::Rect r) {
    return ecl::Grab(s, r);
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

Model *display::MakeModel(const std::string &name) {
    if (Model *m = modelmgr->create(name)) {
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
    DefineModel(name, new ImageModel(s, 0, 0));
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
    ecl::Surface *sfc = Duplicate(surface_cache.get(images[0]));
    if (sfc) {
        ecl::GC gc(sfc);
        for (int i = 1; i < n; i++)
            blit(gc, 0, 0, surface_cache_alpha.get(images[i]));
        DefineModel(name, new ImageModel(sfc, 0, 0));
        image_pile.push_back(sfc);  // make sure it gets destructed
    }
}

void display::DefineComposite(const char *name, const char *bgname, const char *fgname) {
    DefineModel(name, new CompositeModel(MakeModel(bgname), MakeModel(fgname)));
}

void display::DefineAnim(const char *name, bool loop_p) {
    anim_templ = new Anim2d(loop_p);
    DefineModel(name, anim_templ);
    anim_templ_name = name;
}

void display::AddFrame(const char *name, const char *model, double time) {
    if (anim_templ_name != name)
        fprintf(stderr, "AddFrame: Cannot add frames to completed animations.");
    else
        anim_templ->add_frame(MakeModel(model), time / 1000.0);
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

Image::Image(ecl::Surface *sfc) : surface(sfc), rect(surface->size()) {
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

ImageModel::ImageModel(ecl::Surface *s, int xo, int yo)
: image(std::make_shared<Image>(s)), xoff(xo), yoff(yo) {
}

ImageModel::ImageModel(ecl::Surface *s, const ecl::Rect &r, int xo, int yo)
: image(std::make_shared<Image>(s, r)), xoff(xo), yoff(yo) {
}

ImageModel::~ImageModel() {
}

void ImageModel::draw(ecl::GC &gc, int x, int y) {
    draw_image(image.get(), gc, x + xoff, y + yoff);
}

Model *ImageModel::clone() {
    return new ImageModel(image, xoff, yoff);
}

ecl::Rect ImageModel::boundingBox() {
    return ecl::Rect(xoff, yoff, image->rect.w, image->rect.h);
}

/* -------------------- ShadowModel -------------------- */

ShadowModel::ShadowModel(Model *m, Model *sh) : model(m), shadow(sh) {
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

Model *ShadowModel::clone() {
    return new ShadowModel(model->clone(), shadow->clone());
}

ecl::Rect ShadowModel::boundingBox() {
    return bbox;
}

/* -------------------- RandomModel -------------------- */

Model *RandomModel::clone() {
    if (!modelnames.empty()) {
        int r = enigma::IntegerRand(0, modelnames.size() - 1, false);
        return MakeModel(modelnames[r]);
    } else {
        fprintf(stderr, "display_2d.cc: empty RandomModel\n");
        return nullptr;
    }
}

/* -------------------- AliasModel -------------------- */

Model *AliasModel::clone() {
    return MakeModel(name);
}

/* -------------------- Anim2d -------------------- */

Anim2d::Anim2d(bool looping) : rep(std::make_shared<AnimRep>(looping)) {
}

Anim2d::Anim2d(std::shared_ptr<AnimRep> rep, ecl::Rect &bbox) : rep(rep), bbox(bbox) {
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

void Anim2d::add_frame(Model *m, double duration) {
    rep->frames.push_back(std::make_unique<AnimFrame>(m, duration));

    // Cache the bounding box of all frames to ensure that it is constant.
    bbox = ecl::boundingbox(m->boundingBox(), bbox);
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

Model *Anim2d::clone() {
    return new Anim2d(rep, bbox);
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
