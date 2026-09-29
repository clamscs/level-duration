#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cstdint>

using namespace geode::prelude;

std::string formatDuration(int64_t totalSeconds) {
    if (totalSeconds <= 0) return "";

    constexpr int64_t MINUTE = 60;
    constexpr int64_t HOUR = 3600;
    constexpr int64_t DAY = 86400;
    constexpr int64_t MONTH = 2592000;
    constexpr int64_t YEAR = 31536000;

    int64_t rem = totalSeconds;
    int64_t years = rem / YEAR;
    rem %= YEAR;

    int64_t months = rem / MONTH;
    rem %= MONTH;

    int64_t days = rem / DAY;
    rem %= DAY;

    int64_t hours = rem / HOUR;
    rem %= HOUR;

    int64_t minutes = rem / MINUTE;
    int64_t seconds = rem % MINUTE;

    std::string result;
    if (years > 0) result += std::to_string(years) + "y ";
    if (months > 0) result += std::to_string(months) + "mo ";
    if (days > 0) result += std::to_string(days) + "d ";
    if (hours > 0) result += std::to_string(hours) + "h ";
    if (minutes > 0) result += std::to_string(minutes) + "m ";
    if (seconds > 0 || result.empty()) result += std::to_string(seconds) + "s";
    else if (!result.empty() && result.back() == ' ') result.pop_back();

    return result;
}

struct SpeedChange {
    float x;
    float speed;
};

int64_t calculateClassicDuration(const std::string& levelString) {
    if (levelString.empty()) return 0;

    std::string data = ZipUtils::decompressString(levelString, false, 0);
    if (data.empty()) data = levelString;

    std::stringstream ss(data);
    std::string token;
    float startSpeed = 311.58f;
    std::vector<SpeedChange> portals;
    float maxX = 0.0f;

    while (std::getline(ss, token, ';')) {
        if (token.empty()) continue;

        if (token.find("kA") != std::string::npos || token.find("kS") != std::string::npos) {
            std::stringstream headerStream(token);
            std::string key, val;
            while (std::getline(headerStream, key, ',') && std::getline(headerStream, val, ',')) {
                if (key == "kA4") {
                    int speedIndex = std::atoi(val.c_str());
                    if (speedIndex == 1) startSpeed = 251.16f;
                    else if (speedIndex == 0) startSpeed = 311.58f;
                    else if (speedIndex == 2) startSpeed = 387.42f;
                    else if (speedIndex == 3) startSpeed = 468.0f;
                    else if (speedIndex == 4) startSpeed = 576.0f;
                }
            }
            continue;
        }

        std::stringstream objStream(token);
        std::string key, val;
        int id = 0;
        float x = 0.0f;

        while (std::getline(objStream, key, ',') && std::getline(objStream, val, ',')) {
            if (key == "1") id = std::atoi(val.c_str());
            else if (key == "2") x = std::strtof(val.c_str(), nullptr);
        }

        if (x > maxX) maxX = x;

        if (id == 200) portals.push_back({ x, 251.16f });
        else if (id == 201) portals.push_back({ x, 311.58f });
        else if (id == 202) portals.push_back({ x, 387.42f });
        else if (id == 203) portals.push_back({ x, 468.0f });
        else if (id == 1334) portals.push_back({ x, 576.0f });
    }

    if (maxX <= 0.0f) return 0;

    std::sort(portals.begin(), portals.end(), [](const SpeedChange& a, const SpeedChange& b) {
        return a.x < b.x;
    });

    float currentX = 0.0f;
    float currentSpeed = startSpeed;
    float totalTime = 0.0f;

    for (const auto& portal : portals) {
        if (portal.x > maxX) break;
        if (portal.x > currentX) {
            totalTime += (portal.x - currentX) / currentSpeed;
            currentX = portal.x;
        }
        currentSpeed = portal.speed;
    }

    if (maxX > currentX) {
        totalTime += (maxX - currentX) / currentSpeed;
    }

    return static_cast<int64_t>(totalTime);
}

class $modify(LevelDurationLayer, LevelInfoLayer) {
    struct Fields {
        CCLabelBMFont* m_durationLabel = nullptr;
    };

    CCNode* getLengthLabel() {
        CCNode* label = m_lengthLabel;
        if (!label) {
            label = this->getChildByID("length-label");
        }
        return label;
    }

    void updatePosition() {
        if (!m_fields->m_durationLabel) return;

        CCNode* lengthLabel = this->getLengthLabel();
        if (!lengthLabel || !lengthLabel->getParent()) return;

        CCRect bounds = lengthLabel->boundingBox();
        CCPoint worldBottomMid = lengthLabel->getParent()->convertToWorldSpace(
            ccp(bounds.getMidX(), bounds.getMinY())
        );

        CCPoint localPos = this->convertToNodeSpace(worldBottomMid);
        float offset = (m_fields->m_durationLabel->getScaledContentSize().height * 0.5f) + 1.5f;

        m_fields->m_durationLabel->setPosition({ localPos.x, localPos.y - offset });
    }

    void updateDurationDisplay() {
        if (!m_fields->m_durationLabel || !m_level || m_level->isPlatformer()) return;

        int64_t duration = 0;
        if (!m_level->m_levelString.empty()) {
            duration = calculateClassicDuration(m_level->m_levelString);
        }

        if (duration > 0) {
            m_fields->m_durationLabel->setString(formatDuration(duration).c_str());
            m_fields->m_durationLabel->setVisible(true);

            float baseScale = 0.26f;
            float maxWidth = 75.0f;
            float rawWidth = m_fields->m_durationLabel->getContentSize().width;
            if (rawWidth > 0.0f && rawWidth * baseScale > maxWidth) {
                m_fields->m_durationLabel->setScale(maxWidth / rawWidth);
            } else {
                m_fields->m_durationLabel->setScale(baseScale);
            }
        } else {
            m_fields->m_durationLabel->setString("");
            m_fields->m_durationLabel->setVisible(false);
        }

        this->updatePosition();
    }

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        if (!level || level->isPlatformer()) return true;

        CCNode* lengthLabel = this->getLengthLabel();
        if (lengthLabel) {
            auto durationLabel = CCLabelBMFont::create("", "bigFont.fnt");
            durationLabel->setID("duration-label"_spr);
            durationLabel->setScale(0.26f);
            durationLabel->setOpacity(210);
            durationLabel->setColor({ 255, 255, 255 });
            durationLabel->setAnchorPoint({ 0.5f, 0.5f });

            this->addChild(durationLabel, lengthLabel->getZOrder() + 1);
            m_fields->m_durationLabel = durationLabel;

            this->updateDurationDisplay();
        }

        if (level->m_levelString.empty() && level->m_levelID > 0) {
            auto glm = GameLevelManager::sharedState();
            if (glm) {
                glm->m_levelDownloadDelegate = this;
                glm->downloadLevel(level->m_levelID, false, 0);
            }
        }

        return true;
    }

    void levelDownloadFinished(GJGameLevel* level) {
        LevelInfoLayer::levelDownloadFinished(level);
        if (level == m_level && !level->isPlatformer()) {
            this->updateDurationDisplay();
        }
    }
};
