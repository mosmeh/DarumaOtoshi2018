#include "pch.h"

constexpr double holeWidth = 0.25;
constexpr double barrierInterval = 0.5;
constexpr double initialBarrierHeight = 0.1;
constexpr double playerPosY = 0.2;
constexpr double sideWallWidth = 0.1;

struct Barrier {
    enum class Type {
        Left,
        Right,
        Slit
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

        return yPos <= playerPosY && playerPosY <= yPos + height;
    }

    bool isVisible() const {
        return yPos > -height;
    }

    void draw() const {
        static constexpr double r = 5.0;
        switch (type) {
        case Type::Left:
            RoundRect({ sideWallWidth * Window::Width(), yPos * Window::Height() }, { (leftToHole - sideWallWidth) * Window::Width(), height * Window::Height() }, r).drawFrame(r, Palette::Black);
            break;
        case Type::Right:
            RoundRect({ leftToHole * Window::Width(), yPos * Window::Height() }, { (1.0 - leftToHole - sideWallWidth) * Window::Width(), height * Window::Height() }, r).drawFrame(r, Palette::Black);
            break;
        case Type::Slit:
            RoundRect({ sideWallWidth * Window::Width(), yPos * Window::Height() }, { (leftToHole - sideWallWidth) * Window::Width(), height * Window::Height() }, r).drawFrame(r, Palette::Black);
            RoundRect({ (leftToHole + holeWidth) * Window::Width(), yPos * Window::Height() }, { (1.0 - leftToHole - holeWidth - sideWallWidth) * Window::Width(), height * Window::Height() }, r).drawFrame(r, Palette::Black);
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
        Line({ sideWallWidth * Window::Width(), 0.0 }, { sideWallWidth * Window::Width(), Window::Height() }).draw(5.0, Palette::Black);
        Line({ (1.0 - sideWallWidth) * Window::Width(), 0.0 }, { (1.0 - sideWallWidth) * Window::Width(), Window::Height() }).draw(5.0, Palette::Black);

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
    double playerPosX, playerAngle;
    Stopwatch sw;
    Font font = Font(30, U"PixelMplus10-Regular.ttf");
    Font largeFont = Font(60, U"PixelMplus10-Regular.ttf");

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
        int i = 0;
        for (const auto& line : { U"タイトル", U"", U"そうさ", U"← → かたむける", U"", U"なにかキーをおして はじめる"}) {
            getData().font(line).drawAt(Window::Center() + Vec2(0.0, i++ * getData().font.height()), Palette::Black);
        }
    }
};

void drawScore(const Data& data) {
    data.font(U"SCORE ", Pad(data.getScore(), { 5, U'0' }), U" HIGHSCORE ", Pad(data.highScore, { 5,U'0' })).draw(Arg::topCenter = Vec2(Window::Width() / 2.0, 0.0), Palette::Black);
}

void drawPlayer(const Data& data) {
    TextureAsset(U"player")
        .resized({ 100, 100 })
        .rotated(-data.playerAngle + 0.3 * std::sin(data.sw.sF() * 3.0))
        .drawAt(data.playerPosX * Window::Width(), playerPosY * Window::Height());
}

class Playing : public App::Scene {
public:
    Playing(const InitData& init) : IScene(init) {
        getData().level = Level();
        getData().playerPosX = 0.5;
        getData().playerAngle = 0.0;
        getData().sw.restart();
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

            static constexpr std::array<double, maxDirection + 1> angles = {
                0.0, Math::Pi / 6.0, Math::Pi / 4.0, Math::Pi / 3.0
            };
            targetAngle = (direction > 0 ? 1 : -1) * angles.at(std::abs(direction));
        }

        getData().playerAngle += 0.1 * (targetAngle - getData().playerAngle);

        auto& level = getData().level;

        const double speed = 5e-5 * level.getMileage() + 5e-3;
        getData().playerPosX += std::sin(getData().playerAngle) * speed;

        if (level.hit(getData().playerPosX)) {
            changeScene(Scene::GameOver, 0, false);
            getData().sw.pause();
            return;
        }

        level.update(std::cos(getData().playerAngle) * speed);

        getData().highScore = std::max(getData().getScore(), getData().highScore);
    }

    void draw() const override {
        getData().level.draw();
        drawPlayer(getData());
        drawScore(getData());
    }

private:
    int direction = 0;
    double targetAngle = 0.0;
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
        drawPlayer(getData());
        drawScore(getData());

        getData().largeFont(U"GAME OVER").drawAt(Window::Center(), Palette::Black);
        getData().font(U"どれかキーをおして もういちどはじめる").drawAt(Window::Center() + Vec2(0.0, getData().largeFont.height()), Palette::Black);
    }
};

void Main() {
    constexpr int windowSize = 600;
    Window::SetTitle(U"DARUMA OTOSHI 2018");
    Window::Resize(windowSize, windowSize);

    Graphics::SetBackground(Palette::White);
    Graphics::SetTargetFrameRateHz(60);

    TextureAsset::Register(U"player", U"daruma.png");

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
