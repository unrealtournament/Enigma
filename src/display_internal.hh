#ifndef DISPLAY_INTERNAL_HH
#define DISPLAY_INTERNAL_HH

#include "display.hh"
#include "ecl_font.hh"
#include "ecl_video.hh"
#include "enigma.hh"

#include <list>
#include <memory>

namespace enigma::display {

class DisplayLayer;
class Model;

typedef ecl::Rect ScreenArea;
typedef ecl::Rect WorldArea;
typedef std::list<Model *> ModelList;

class Window {
public:
    Window() = default;
    explicit Window(ScreenArea area) : m_area(area) {}

    const ScreenArea &getArea() const { return m_area; }

private:
    ScreenArea m_area;
};

class TextDisplay {
public:
    explicit TextDisplay(ecl::Font &font);

    void setText(const std::string &newText, bool scrolling, double duration = -1);

    void tick(double dtime);
    bool hasChanged() const { return changed; }
    bool hasFinished() const { return finished; }

    void draw(ecl::GC &gc, const ecl::Rect &r);

private:
    ecl::Rect area;
    std::string text;
    bool changed = false, finished = true;
    bool pingpong = false;
    bool showscroll = false;
    double xoff = 0;
    double scrollSpeed;  // pixels per second
    std::unique_ptr<ecl::Surface> textSurface = nullptr;
    ecl::Font &font;
    double time = 0, maxtime = 0;
};

class StatusBarImpl : public StatusBar, public Window {
public:
    explicit StatusBarImpl(const ScreenArea &area);
    ~StatusBarImpl() override;

    bool hasChanged() const { return changed; }
    void redraw(ecl::GC &gc, const ScreenArea &r);
    void tick(double dtime);
    void newWorld();

    // StatusBar interface.
    void setTime(double time) override;
    void setInventory(Player activePlayer, const std::vector<std::string> &modelNames) override;
    void showText(const std::string &str, bool scrolling, double duration) override;
    void hideText() override;

    void showMoveCounter(bool active) override;
    void setCMode(bool flag) override;
    void setBasicModes(std::string flags) override;

    void setSpeed(double speed) override;
    void setTravelledDistance(double distance) override;
    void setCounter(int newCounter) override;

private:
    ScreenArea itemArea;
    std::vector<std::unique_ptr<Model>> itemModels;
    Player player = YIN;
    bool changed = false;
    TextDisplay textDisplay;
    ecl::Font *timeFont = nullptr;
    ecl::Font *movesFont = nullptr;
    ecl::Font *modesFont = nullptr;

    double levelTime = 0; ///< Elapsed time in seconds
    bool showTime = true;
    int moveCounter = 0;
    bool showMoves = false;
    bool interruptible = true;  // Current text message may be interrupted
    bool textActive = false;
    bool cMode = false;  // collision mode flag
    int playerImage = 0;
    double playerImageDuration = 0;
    std::string basicModes;  // set by world on start of level
    int widthDigit[10] = {};
    int widthColon = 0;
    int widthApos = 0;
    int widthQuote = 0;
    int maxWidthDigit = 0;
};

}  // namespace display

#endif
