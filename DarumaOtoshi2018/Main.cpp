#include "pch.h"

constexpr double holeWidth = 0.25;
constexpr double barrierInterval = 0.5;
constexpr double initialBarrierHeight = 0.1;
constexpr double planePosY = 0.2;
constexpr double sideWallWidth = 0.1;

struct Barrier {
    enum class Type {
        Left = 0,
        Right = 1,
        Slit = 2
    };

    const Type type;
    const double leftToHole, height;
    double yPos;

    bool hit(double x) const {
        switch (type) {
        case Type::Left:
            if (x > leftToHole) {
                return false;
            }
            break;
        case Type::Right:
            if (x < leftToHole) {
                return false;
            }
            break;
        case Type::Slit:
            if (leftToHole < x && x < leftToHole + holeWidth) {
                return false;
            }
            break;
        }

        return yPos < planePosY && planePosY < yPos + height;
    }

    bool isVisible() const {
        return yPos > -height;
    }

    void draw() const {
        switch (type) {
        case Type::Left:
            RectF(0, yPos * Window::Height(), leftToHole * Window::Width(), height * Window::Height()).draw(Palette::Black);
            break;
        case Type::Right:
            RectF(leftToHole * Window::Width(), yPos * Window::Height(), (1.0 - leftToHole) * Window::Width(), height * Window::Height()).draw(Palette::Black);
            break;
        case Type::Slit:
            RectF(0, yPos * Window::Height(), leftToHole * Window::Width(), height * Window::Height()).draw(Palette::Black);
            RectF((leftToHole + holeWidth) * Window::Width(), yPos * Window::Height(), (1.0 - leftToHole - holeWidth) * Window::Width(), height * Window::Height()).draw(Palette::Black);
            break;
        }
    }
};

class Level {
public:
    Level() : mileage(0.0) {
        double height = 0.0;
        while (height < 1.0) {
            addBarrier();
            height += barrierInterval;
        }
    }

    void update(double speedY) {
        mileage += speedY;

        for (auto& barrier : barriers) {
            barrier.yPos -= speedY;
        }
        if (!barriers.front().isVisible()) {
            barriers.pop_front();
            addBarrier();
        }
    }

    void draw() const {
        RectF(sideWallWidth * Window::Width(), 0, (1.0 - 2.0 * sideWallWidth) * Window::Width(), Window::Height()).draw(Palette::White);

        for (const auto& barrier : barriers) {
            barrier.draw();
        }
    }

    bool hit(double posX) const {
        if (posX < sideWallWidth || posX > 1.0 - sideWallWidth) {
            return true;
        }
        for (const auto& barrier : barriers) {
            if (barrier.hit(posX)) {
                return true;
            }
        }
        return false;
    }

    double getMileage() const {
        return mileage;
    }

private:
    std::deque<Barrier> barriers = { {Barrier::Type::Slit, 0.5 - holeWidth / 2.0, initialBarrierHeight, 1.0} };
    double mileage;

    void addBarrier() {
        auto type = Barrier::Type::Left;
        double leftToHole = 0.0;

        const double pos = Random(0.4, 0.6);
        switch (barriers.back().type) {
        case Barrier::Type::Left:
            type = RandomBool() ? Barrier::Type::Right : Barrier::Type::Slit;
            leftToHole = pos;
            break;
        case Barrier::Type::Right:
            type = RandomBool() ? Barrier::Type::Left : Barrier::Type::Slit;
            leftToHole = 1.0 - pos;
            break;
        case Barrier::Type::Slit:
            type = Barrier::Type(Random(0, 2));
            leftToHole = pos - holeWidth / 2.0;
            break;
        }

        const double factor = std::min(mileage / 100.0, 1.0);
        const double height = initialBarrierHeight + factor * (0.3 - initialBarrierHeight);

        barriers.emplace_back<Barrier>({ type, leftToHole, height, barriers.back().yPos + barrierInterval });
    }
};

enum class Scene {
    Title,
    Playing,
    GameOver
};

struct Data {
    int score = 0;
    int highScore = 0;
    Optional<detail::Gamepad_impl> gamepad;
    Level level;
    Font font = Font(30, U"PixelMplus10-Regular.ttf");

    int getScore() const {
        return static_cast<int>(level.getMileage() * 5.0);
    }

    bool anyKeyIsDown() const {
        for (uint8 i = KeyCancel.code(); i <= KeyBackslash_JP.code(); ++i) {
            if (Key{ InputDevice::Keyboard, i }.down()) {
                return true;
            }
        }
        if (gamepad.has_value()) {
            if ((gamepad->povLeft | gamepad->povRight | gamepad->povUp | gamepad->povDown).down()) {
                return true;
            }
            for (auto button : gamepad->buttons) {
                if (button.down()) {
                    return true;
                }
            }
        }
        return false;
    }
};

using App = SceneManager<Scene, Data>;

class Title : public App::Scene {
public:
    Title(const InitData& init) : IScene(init) {}

    void update() override {
        if (getData().anyKeyIsDown()) {
            changeScene(Scene::Playing, 0, false);
        }
    }

    void draw() const override {
        getData().font(U"TITLE").drawAt(Window::Center(), Palette::White);
        getData().font(U"PRESS ANY KEY TO START").drawAt(Window::Center() + Vec2(0.0, getData().font.height()), Palette::White);
    }
};

class Playing : public App::Scene {
public:
    Playing(const InitData& init) : IScene(init) {
        getData().level = Level();
    }

    void update() override {
        constexpr unsigned int maxDirection = 3;

        const auto gp = getData().gamepad;
        const bool keyLeft = KeyLeft.down() || (gp && gp->povLeft.down());
        const bool keyRight = KeyRight.down() || (gp && gp->povRight.down());

        if (keyLeft ^ keyRight) {
            if (keyLeft) {
                direction = std::max(direction - 1, -static_cast<int>(maxDirection));
            }
            if (keyRight) {
                direction = std::min(direction + 1, static_cast<int>(maxDirection));
            }
        }

        static constexpr std::array<double, maxDirection + 1> angles = {
            0.0, Math::Pi / 6.0, Math::Pi / 4.0, Math::Pi / 3.0
        };
        const double angle = (direction > 0 ? 1 : -1) * angles.at(std::abs(direction));

        auto& level = getData().level;

        const double planeSpeed = 5e-5 * level.getMileage() + 5e-3;
        planePosX += std::sin(angle) * planeSpeed;

        if (level.hit(planePosX)) {
            changeScene(Scene::GameOver, 0, false);
        }

        level.update(std::cos(angle) * planeSpeed);

        getData().highScore = std::max(getData().getScore(), getData().highScore);
    }

    void draw() const override {
        getData().level.draw();

        RectF r(50, 50);
        r.setCenter(planePosX * Window::Width(), planePosY * Window::Height());
        r.draw(Palette::Red);

        const int score = getData().getScore();
        getData().font(U"SCORE ", score, U" HIGHSCORE ", getData().highScore).draw(Arg::topCenter = Vec2(Window::Width() / 2.0, 0.0), Palette::Black);
    }

private:
    double planePosX = 0.5;
    int direction = 0;
};

class GameOver : public App::Scene {
public:
    GameOver(const InitData& init) : IScene(init) {}

    void update() override {
        if (getData().anyKeyIsDown()) {
            changeScene(Scene::Playing, 0, false);
        }
    }

    void draw() const override {
        getData().level.draw();

        const int score = getData().getScore();
        getData().font(U"SCORE ", score, U" HIGHSCORE ", getData().highScore).draw(Arg::topCenter = Vec2(Window::Width() / 2.0, 0.0), Palette::Black);

        getData().font(U"GAME OVER").drawAt(Window::Center(), Palette::Black);
        getData().font(U"PRESS ANY KEY TO RETRY").drawAt(Window::Center() + Vec2(0.0, getData().font.height()), Palette::Black);
    }
};

void Main() {
    constexpr int windowSize = 600;
    Window::SetTitle(U"test");
    Window::Resize(windowSize, windowSize);

    Graphics::SetBackground(Palette::Black);
    Graphics::SetTargetFrameRateHz(60);

    const auto data = std::make_shared<Data>();

    const auto scoreFile = U"score";
    if (FileSystem::Exists(scoreFile)) {
        BinaryReader reader(scoreFile);
        if (!reader.read(data->highScore)) {
            data->highScore = 0;
        }
    }

    const auto pads = System::EnumerateGamepads();
    if (!pads.empty()) {
        data->gamepad = Gamepad(pads.front().index);
    }

    App mgr(data);
    mgr.add<Title>(Scene::Title)
        .add<Playing>(Scene::Playing)
        .add<GameOver>(Scene::GameOver);
    mgr.changeScene(Scene::Title, 0, false);

    while (System::Update()) {
        if (!mgr.update()) {
            break;
        }
    }

    BinaryWriter writer(scoreFile);
    writer.write(&data->highScore, sizeof(data->highScore));
}
