/*
 * Copyright (C) 2002,2003 Daniel Heck
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
#ifndef D_MODELS_HH
#define D_MODELS_HH

#include "display_internal.hh"
#include "ecl_video.hh"
#include "ecl_geom.hh"
#include <vector>
#include <string>

namespace enigma::display {

/* -------------------- Image -------------------- */

struct Image {
    // Variables.
    std::unique_ptr<ecl::Surface> surface;
    ecl::Rect rect;  // location of image inside surface

    // Constructors.
    explicit Image(std::unique_ptr<ecl::Surface> sfc);
    Image(ecl::Surface *sfc, ecl::Rect r);
    Image(const Image& i) = delete;
    Image& operator=(const Image& i) = delete;
};

void draw_image(Image *image, ecl::GC &gc, int x, int y);

/* -------------------- ImageModel -------------------- */

class ImageModel : public Model {
    std::shared_ptr<Image> image;
    int xoff, yoff;  // relative origin of the image
public:
    // Constructors
    ImageModel(const std::shared_ptr<Image> &image, int xo, int yo);
    ImageModel(std::unique_ptr<ecl::Surface> s, int xo, int yo);
    ImageModel(ecl::Surface *s, const ecl::Rect &r, int xo, int yo);
    ~ImageModel() override;

    // Model interface
    void draw(ecl::GC &gc, int x, int y) override;
    std::unique_ptr<Model> clone() override;
    ecl::Rect boundingBox() override;
    Image *get_image() const { return image.get(); }
};

/* -------------------- ShadowModel -------------------- */

class ShadowModel : public Model {
public:
    ShadowModel(std::unique_ptr<Model> m, std::unique_ptr<Model> sh);
    ~ShadowModel() override;

    // Model interface
    void expose(ModelLayer *ml, int vx, int vy) override;
    void removeFromLayer(ModelLayer *ml) override;

    void setCallback(ModelCallback *cb) override;
    void reverse() override;
    void restart() override;
    void draw(ecl::GC &gc, int x, int y) override;
    void drawShadow(ecl::GC &gc, int x, int y) override;
    Model *get_shadow() const override;
    std::unique_ptr<Model> clone() override;

    ecl::Rect boundingBox() override;

private:
    std::unique_ptr<Model> model;
    std::unique_ptr<Model> shadow;
    ecl::Rect bbox;  // combined bounding box of model and shadow
};

/* -------------------- CompositeModel -------------------- */

class CompositeModel : public Model {
    std::unique_ptr<Model> background;
    std::unique_ptr<Model> foreground;

public:
    CompositeModel(std::unique_ptr<Model> bg, std::unique_ptr<Model> fg)
    : background(std::move(bg)), foreground(std::move(fg)) {}

    // Animation interface
    void setCallback(ModelCallback *cb) override {
        foreground->setCallback(cb);
    }
    void reverse() override {
        foreground->reverse();
    }
    void restart() override { foreground->restart(); }

    // Model interface
    Model *get_shadow() const override { return background->get_shadow(); }
    void expose(ModelLayer *ml, int vx, int vy) override {
        foreground->expose(ml, vx, vy);
    }
    void removeFromLayer(ModelLayer *ml) override {
        foreground->removeFromLayer(ml);
    }
    void draw(ecl::GC &gc, int x, int y) override {
        background->draw(gc, x, y);
        foreground->draw(gc, x, y);
    }
    void drawShadow(ecl::GC &gc, int x, int y) override {
        background->drawShadow(gc, x, y);
    }
    std::unique_ptr<Model> clone() override {
        return std::make_unique<CompositeModel>(background->clone(), foreground->clone());
    }

    ecl::Rect boundingBox() override { return foreground->boundingBox(); }
};

/* -------------------- RandomModel -------------------- */

/* Creates new models randomly from a set of template models. */
class RandomModel : public Model {
    std::vector<std::string> modelNames;

public:
    void add_model(const std::string &name) { modelNames.push_back(name); }
    std::unique_ptr<Model> clone() override;
};

/* -------------------- AliasModel -------------------- */

class AliasModel : public Model {
    std::string name;

public:
    explicit AliasModel(const std::string &modelname) : name(modelname) {}
    std::unique_ptr<Model> clone() override;
};

/* -------------------- Animations -------------------- */

struct AnimFrame {
    std::unique_ptr<Model> model;
    double duration;

    AnimFrame(std::unique_ptr<Model> model, double duration)
        : model(std::move(model)), duration(duration) {}
};

struct AnimRep {
    std::vector<std::unique_ptr<AnimFrame>> frames;
    bool looping;

    explicit AnimRep(bool looping) : looping(looping) {}
};

class Anim2d : public Model, public ecl::Nocopy {
public:
    explicit Anim2d(bool looping);
    Anim2d(const std::shared_ptr<AnimRep> &rep, const ecl::Rect &bbox);

    void setCallback(ModelCallback *cb) override;

    void addFrame(std::unique_ptr<Model> model, double duration);

    /* ---------- Model interface ---------- */
    void draw(ecl::GC &gc, int x, int y) override;
    void drawShadow(ecl::GC &gc, int x, int y) override;
    std::unique_ptr<Model> clone() override;
    void reverse() override;
    void restart() override;

    void expose(ModelLayer *layer, int vx, int vy) override;
    void removeFromLayer(ModelLayer *layer) override;

    void tick(double dtime) override;
    bool hasChanged(ecl::Rect &changedRegion) override;
    bool hasFinished() const override { return finished; }

    void move(int newX, int newY);

    ecl::Rect boundingBox() override;

private:
    // ---------- Variables ----------
    std::shared_ptr<AnimRep> rep;
    unsigned curframe = 0; // Current frame number
    double frameTime = 0;  // Elapsed time since frame was activated
    bool finished = false; // Animation has finished
    bool changed = false;  // Model state has changed since last redraw
    bool reversed = false; // Play the animation in reverse direction

    int screenX = 0, screenY = 0;   // Video coordinates of sprite
    ecl::Rect bbox;  // largest bounding box of all frames
    ModelCallback *callback = nullptr;
};

ecl::Surface *GetSurface(const std::string &filename);
ecl::Surface *CropSurface(const ecl::Surface *s, ecl::Rect r);
void DefineModel(const char *name, Model *m);
void DefineImageModel(const char *name, ecl::Surface *s);

}  // namespace display

#endif
