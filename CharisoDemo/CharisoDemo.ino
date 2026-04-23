#include <M5Cardputer.h>
#include <Preferences.h>

#include <array>
#include <cctype>
#include <climits>

namespace {

constexpr int kBaseWidth = 240;
constexpr int kBaseHeight = 240;
constexpr int kTerrainCols = 48;
constexpr int kColumnWidth = 5;
constexpr uint32_t kRenderIntervalMs = 16;
constexpr uint32_t kTitleStepMs = 48;

enum class Scene : uint8_t {
  Title,
  Play,
  Setup,
  Edit,
  GameOver,
};

enum class EndReason : uint8_t {
  None,
  Overtaken,
  Cliff,
  Fall,
  GiveUp,
};

struct Settings {
  String player = "Suzuki";
  String rival = "Sato";
  String commentator = "Tanaka";
  bool showMeter = false;
  uint8_t volume = 8;
  bool vibration = true;
};

struct TitleState {
  std::array<int16_t, kTerrainCols> terrain{};
  uint32_t lastTick = 0;
};

struct EditState {
  String label;
  String* target = nullptr;
  String buffer;
};

struct PlayState {
  std::array<int16_t, kTerrainCols> terrain{};
  uint32_t lastTick = 0;
  uint32_t frameDelay = 20;
  bool turbo = false;
  bool finished = false;
  bool newHighScore = false;
  EndReason endReason = EndReason::None;
  int count = 0;
  int totalBeforeRun = 0;
  int playerX = 102;
  int playerY = 200;
  int playerAirState = 0;
  int playerVel = 0;
  int rivalX = 0;
  int rivalY = 0;
  int rivalAirState = 0;
  int rivalVel = 0;
  int rivalImpulse = 0;
  int rivalMessageState = 0;
  int rivalSpark = 1;
  int terrainTrend = 0;
  int presetId = 0;
  int presetIndex = 0;
  int gapId = 0;
  int gapIndex = 0;
  int obstacleX = -10;
  int obstacleY = 0;
  int gateX = -10;
  int gateY = 0;
  int gatePlayerVel = 0;
  int gateRivalVel = 0;
  int gustX = -200;
  int gustY = 0;
  bool bannerActive = false;
  int bannerX = -10;
  int bannerY = 0;
  bool rollerActive = false;
  int rollerX = -10;
  int rollerY = 0;
  int rollerFrame = 0;
  String message;
  int messageTimer = 0;
};

Preferences prefs;
M5Canvas frameBuffer(&M5Cardputer.Display);
Settings settings;
TitleState titleState;
EditState editState;
PlayState playState;
Scene currentScene = Scene::Title;
int setupSelection = 0;
int highScore = 0;
int totalMeters = 0;
uint32_t lastRenderTick = 0;

const int16_t kTerrainPresets[8][48] = {
    {-10, -10, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240,
     -240, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240,
     -240, -240, -240, -240, -240, -240, -240, -240, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, -240},
    {-10, -10, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240,
     -240, -240, -240, 110, 111, 112, 113, 114, 114, 114, 115, 115, 115, 115,
     115, 116, 116, 116, 116, 116, 116, 116, 116, 116, 115, 115, 115, 115,
     114, 114, 114, 113, 112, 111, 110, 0},
    {0, 0, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240,
     -240, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240,
     -240, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240, -240,
     -240, -240, -240, -240, -240, -240, -240, -240, -240, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -4, -5, -5, -6, -6, -6, -7,
     -7, -7, -7, -7, -7, -7, -7, -7, -7, -6, -6, -6, -5, -5, -4, -3, -2, -1,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -4, -4, -5, -5, -6, -6, -7,
     -7, -7, -7, -7, -7, -6, -6, -5, -5, -4, -4, -3, -2, -1, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -4, -5, -5, -6, -6, -6, -7,
     -7, -7, -7, -7, -7, -7, -7, -7, -7, -6, -6, -6, -5, -5, -4, -3, -2, -1,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, -240, -240, -240, -240, -240, -240, -240, 0, 0, 0, 0, 0, 0, -240,
     -240, -240, -240, -240, -240, -240, 5, 5, 5, 5, 5, 5, -240, -240, -240,
     -240, -240, -240, 10, 10, 10, 10, 10, 10, -240, -240, -240, -240, -240,
     -240, 0, 0},
    {0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 22, 22, 22, 22,
     22, 22, 22, 22, 22, 22, 22, 22, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
     32, 32, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42}};

const int16_t kGapPresets[3][5] = {
    {12, 12, 12, 12, 12},
    {-12, -12, -12, -12, -12},
    {-240, 0, -240, 0, -240},
};

const char* kSetupItems[] = {
    "Player", "Rival", "Commentator", "Meter", "Volume", "Vibration"};

int clampInt(const int value, const int low, const int high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return M5Cardputer.Display.color565(r, g, b);
}

int worldToScreenX(int x) {
  return (x * M5Cardputer.Display.width()) / kBaseWidth;
}

int worldToScreenY(int y) {
  return (y * M5Cardputer.Display.height()) / kBaseHeight;
}

int worldToScreenW(int w) {
  const int scaled = (w * M5Cardputer.Display.width()) / kBaseWidth;
  return scaled > 0 ? scaled : 1;
}

int worldToScreenH(int h) {
  const int scaled = (h * M5Cardputer.Display.height()) / kBaseHeight;
  return scaled > 0 ? scaled : 1;
}

void beep(uint16_t frequency, uint16_t duration) {
  if (settings.volume == 0) {
    return;
  }
  M5Cardputer.Speaker.tone(frequency, duration);
}

int randomTerrainStep(int y) {
  const int delta = random(-1, 2);
  return clampInt(y + delta, 50, 230);
}

int terrainUnder(const int x, const std::array<int16_t, kTerrainCols>& terrain) {
  const int idx = clampInt(x / kColumnWidth, 0, kTerrainCols - 1);
  return terrain[idx];
}

int supportedGroundY(const int x, const std::array<int16_t, kTerrainCols>& terrain) {
  int support = -1;
  const int samples[3] = {terrainUnder(x - 5, terrain), terrainUnder(x, terrain),
                          terrainUnder(x + 5, terrain)};
  for (const int sample : samples) {
    if (sample != 240 && sample > support) {
      support = sample;
    }
  }
  return support >= 0 ? support : 240;
}

bool isCliffPixel(const int x, const int y,
                  const std::array<int16_t, kTerrainCols>& terrain) {
  if (x < kColumnWidth) {
    return false;
  }
  const int idx = clampInt(x / kColumnWidth, 0, kTerrainCols - 1);
  if (y <= terrain[idx]) {
    return false;
  }
  return idx > 0 && terrain[idx - 1] - terrain[idx] == 10;
}

bool isGroundPixel(const int x, const int y,
                   const std::array<int16_t, kTerrainCols>& terrain) {
  const int idx = clampInt(x / kColumnWidth, 0, kTerrainCols - 1);
  return y > terrain[idx];
}

void resetTitleTerrain() {
  titleState.terrain.fill(220);
}

void saveSettings() {
  prefs.putString("player", settings.player);
  prefs.putString("rival", settings.rival);
  prefs.putString("commentator", settings.commentator);
  prefs.putBool("meter", settings.showMeter);
  prefs.putUChar("volume", settings.volume);
  prefs.putBool("vibration", settings.vibration);
  prefs.putInt("highScore", highScore);
  prefs.putInt("totalMeters", totalMeters);
}

void loadSettings() {
  prefs.begin("chariso", false);
  settings.player = prefs.getString("player", settings.player);
  settings.rival = prefs.getString("rival", settings.rival);
  settings.commentator = prefs.getString("commentator", settings.commentator);
  settings.showMeter = prefs.getBool("meter", settings.showMeter);
  settings.volume = prefs.getUChar("volume", settings.volume);
  settings.vibration = prefs.getBool("vibration", settings.vibration);
  highScore = prefs.getInt("highScore", 0);
  totalMeters = prefs.getInt("totalMeters", 0);
}

void pushMessage(const String& message, const int timer = 180) {
  playState.message = message;
  playState.messageTimer = timer;
}

void resetPlayState(const bool turbo) {
  playState = {};
  playState.turbo = turbo;
  playState.frameDelay = turbo ? 24 : 40;
  playState.playerX = 102;
  playState.playerY = 200;
  playState.endReason = EndReason::None;
  playState.totalBeforeRun = totalMeters;
  playState.terrain.fill(230);
  pushMessage("A/D move  ENTER or BtnA jump", 220);
}

void beginTitleScene() {
  currentScene = Scene::Title;
  resetTitleTerrain();
  titleState.lastTick = millis();
}

void beginSetupScene() {
  currentScene = Scene::Setup;
  setupSelection = 0;
}

void beginEditScene(const char* label, String* target) {
  currentScene = Scene::Edit;
  editState.label = label;
  editState.target = target;
  editState.buffer = *target;
}

void finishRun() {
  if (playState.finished) {
    return;
  }
  playState.finished = true;
  totalMeters = clampInt(totalMeters + playState.count, 0, INT32_MAX);
  if (playState.count > highScore) {
    highScore = playState.count;
    playState.newHighScore = true;
  }
  saveSettings();
  currentScene = Scene::GameOver;
  beep(playState.newHighScore ? 1320 : 660, playState.newHighScore ? 140 : 80);
}

void startGame(const bool turbo) {
  resetPlayState(turbo);
  playState.lastTick = millis();
  currentScene = Scene::Play;
  beep(turbo ? 1040 : 880, 60);
}

void updateTitleTerrain() {
  for (int i = 0; i < kTerrainCols - 1; ++i) {
    titleState.terrain[i] = titleState.terrain[i + 1];
  }
  if (random(10) != 0) {
    int idx = kTerrainCols - 2;
    while (idx > 0 && titleState.terrain[idx] == 240) {
      --idx;
    }
    titleState.terrain[kTerrainCols - 1] = randomTerrainStep(titleState.terrain[idx]);
    if (titleState.terrain[kTerrainCols - 1] < 200) {
      titleState.terrain[kTerrainCols - 1] = 200;
    }
  } else {
    titleState.terrain[kTerrainCols - 1] = 240;
  }
}

void spawnPresetIfNeeded() {
  if (playState.presetId != 0 || playState.gapId != 0 || playState.terrain[47] == 240) {
    return;
  }
  if (playState.count > 0 && playState.count % 75 == 0) {
    playState.presetId = random(1, 9);
    playState.presetIndex = 47;
  } else if (playState.count > 0 && playState.count % 32 == 0 && random(3) == 0) {
    playState.gapId = random(1, 4);
    playState.gapIndex = 4;
  }
}

void updateTerrainColumn() {
  for (int i = 0; i < kTerrainCols - 1; ++i) {
    playState.terrain[i] = playState.terrain[i + 1];
  }

  if (playState.presetId > 0) {
    if (playState.presetIndex == 47) {
      const int base = playState.terrain[46];
      if (playState.presetId == 1 || playState.presetId == 2) {
        if (base >= 120) {
          playState.obstacleX = 235;
          playState.obstacleY = base;
        }
      } else if (playState.presetId == 3) {
        playState.rollerActive = true;
        playState.rollerX = 235;
        playState.rollerY = base;
      } else if (playState.presetId == 4) {
        playState.gustX = 280;
        playState.gustY = base + 2;
      } else if (playState.presetId == 5) {
        playState.bannerActive = true;
        playState.bannerX = 290;
        playState.bannerY = base - 6;
      } else if (playState.presetId == 8) {
        playState.gateX = 235;
        playState.gateY = base;
      }
    }

    if (playState.presetIndex >= 0) {
      const int base = playState.terrain[46];
      int next = base - kTerrainPresets[playState.presetId - 1][47 - playState.presetIndex];
      next = clampInt(next, 50, 240);
      if (next != 240) {
        next += random(0, 2);
        next = clampInt(next, 50, 240);
      }
      playState.terrain[47] = next;
      --playState.presetIndex;
      if (playState.presetIndex < 0) {
        playState.presetId = 0;
        playState.presetIndex = 0;
      }
      return;
    }
  }

  if (playState.gapId > 0) {
    const int base = playState.terrain[46];
    int next = base - kGapPresets[playState.gapId - 1][4 - playState.gapIndex];
    next = clampInt(next, 50, 240);
    if (next != 240) {
      next += random(0, 2);
      next = clampInt(next, 50, 240);
    }
    playState.terrain[47] = next;
    --playState.gapIndex;
    if (playState.gapIndex < 0) {
      playState.gapId = 0;
      playState.gapIndex = 0;
    }
    return;
  }

  const int gapChance = playState.terrain[46] == 240 ? 3 : 20;
  if (random(gapChance) != 0) {
    int idx = 46;
    while (idx > 0 && playState.terrain[idx] == 240) {
      --idx;
    }
    const int base = playState.terrain[idx];
    if (playState.terrainTrend == 0) {
      playState.terrain[47] = randomTerrainStep(base);
    } else if (playState.terrainTrend == 1) {
      playState.terrain[47] = base - random(1, 3);
      if (playState.terrain[47] < 50) {
        playState.terrainTrend = 0;
      }
    } else {
      playState.terrain[47] = base + random(1, 3);
      if (playState.terrain[47] > 230) {
        playState.terrainTrend = 0;
      }
    }
    playState.terrain[47] = clampInt(playState.terrain[47], 50, 230);
  } else if (playState.terrain[46] != 240) {
    playState.terrain[47] = 240;
  } else {
    playState.terrain[47] = 230;
  }
}

void updateWorldObjects() {
  if (playState.obstacleX > -5) {
    playState.obstacleX -= 5;
    if (playState.playerX > playState.obstacleX && playState.playerX < playState.obstacleX + 20 &&
        playState.playerY > playState.obstacleY) {
      playState.playerVel = 15;
      playState.playerAirState = 1;
      beep(900, 25);
    }
    if (playState.rivalX > playState.obstacleX && playState.rivalX < playState.obstacleX + 20 &&
        playState.rivalY > playState.obstacleY) {
      playState.rivalVel = 15;
      playState.rivalAirState = 1;
    }
  }

  if (playState.gateX > -230) {
    playState.gateX -= 5;
    if ((playState.playerX > playState.gateX + 20 && playState.playerX < playState.gateX + 81) ||
        (playState.playerX > playState.gateX + 87 && playState.playerX < playState.gateX + 143) ||
        (playState.playerX > playState.gateX + 169 && playState.playerX < playState.gateX + 230)) {
      playState.playerAirState = 0;
      if (playState.playerY > playState.gateY - 15) {
        --playState.gatePlayerVel;
      }
      if (playState.playerY < playState.gateY - 15) {
        ++playState.gatePlayerVel;
      }
      playState.playerY += playState.gatePlayerVel;
    }
    if (playState.count > 500 &&
        ((playState.rivalX > playState.gateX + 20 && playState.rivalX < playState.gateX + 81) ||
         (playState.rivalX > playState.gateX + 87 && playState.rivalX < playState.gateX + 143) ||
         (playState.rivalX > playState.gateX + 169 && playState.rivalX < playState.gateX + 230))) {
      playState.rivalAirState = 0;
      if (playState.rivalY > playState.gateY - 15) {
        --playState.gateRivalVel;
      }
      if (playState.rivalY < playState.gateY - 15) {
        ++playState.gateRivalVel;
      }
      playState.rivalY += playState.gateRivalVel;
    }
    if (playState.gateX <= -230) {
      playState.gatePlayerVel = 0;
      playState.gateRivalVel = 0;
    }
  }

  if (playState.bannerActive) {
    playState.bannerX -= 5;
    if (playState.bannerX < -230) {
      playState.bannerActive = false;
    }
  }

  if (playState.gustX > -150) {
    if (playState.playerX > playState.gustX && playState.playerX < playState.gustX + 150 &&
        playState.playerAirState == 0 && playState.playerX > 20) {
      --playState.playerX;
    }
    if (playState.rivalX > playState.gustX && playState.rivalX < playState.gustX + 150 &&
        playState.rivalAirState == 0) {
      --playState.rivalX;
      playState.rivalVel = 5;
      playState.rivalAirState = 1;
    }
    playState.gustX -= 5;
  }

  if (playState.rollerActive) {
    playState.rollerFrame = (playState.rollerFrame + 1) % 4;
    playState.rollerX -= 5;
    if (playState.rollerX < -230) {
      playState.rollerActive = false;
    }
  }
}

void updatePlayerPhysics() {
  if (playState.playerAirState != 0) {
    playState.playerY -= playState.playerVel--;
    if (playState.playerY < 0) {
      playState.playerY = 0;
      playState.playerVel = 0;
    }
    if (isGroundPixel(playState.playerX, playState.playerY, playState.terrain)) {
      if (terrainUnder(playState.playerX, playState.terrain) == 240) {
        playState.endReason = EndReason::Fall;
        finishRun();
      } else if (isGroundPixel(playState.playerX + 5, playState.playerY + playState.playerVel + 1,
                               playState.terrain) &&
                 terrainUnder(playState.playerX - 5, playState.terrain) == 240) {
        playState.endReason = EndReason::Cliff;
        finishRun();
      } else {
        playState.playerAirState = 0;
        playState.playerVel = 0;
        playState.playerY = supportedGroundY(playState.playerX, playState.terrain);
      }
    }
  } else if (terrainUnder(playState.playerX + 5, playState.terrain) -
                 terrainUnder(playState.playerX, playState.terrain) >
             5 ||
             terrainUnder(playState.playerX, playState.terrain) == 240) {
    playState.playerVel = 0;
    ++playState.playerAirState;
  } else {
    playState.playerY = supportedGroundY(playState.playerX, playState.terrain);
  }

  if (isCliffPixel(playState.playerX, playState.playerY - 2, playState.terrain)) {
    playState.endReason = EndReason::Cliff;
    finishRun();
  }
}

void updateRivalPhysics() {
  if (playState.count <= 500 || playState.rivalX <= 5) {
    return;
  }

  if (playState.rivalAirState == 0 && terrainUnder(playState.rivalX + 8, playState.terrain) == 240 &&
      terrainUnder(playState.rivalX + 2, playState.terrain) == 240 &&
      terrainUnder(playState.rivalX + 5, playState.terrain) == 240) {
    playState.rivalVel = 5;
    ++playState.rivalAirState;
  } else if (playState.rivalAirState == 0 &&
             (isCliffPixel(playState.rivalX + 8, playState.rivalY - 5, playState.terrain) ||
              isCliffPixel(playState.rivalX + 2, playState.rivalY - 5, playState.terrain) ||
              isCliffPixel(playState.rivalX + 5, playState.rivalY - 5, playState.terrain))) {
    playState.rivalVel = 5;
    ++playState.rivalAirState;
  }

  if (playState.rivalAirState != 0) {
    playState.rivalY -= playState.rivalVel--;
    if (isGroundPixel(playState.rivalX, playState.rivalY, playState.terrain)) {
      playState.rivalVel = 0;
      playState.rivalAirState = 0;
      playState.rivalY = supportedGroundY(playState.rivalX, playState.terrain);
    }
  } else if (terrainUnder(playState.rivalX + 5, playState.terrain) -
                 terrainUnder(playState.rivalX, playState.terrain) >
             5) {
    playState.rivalVel = 0;
    ++playState.rivalAirState;
  } else {
    playState.rivalY = supportedGroundY(playState.rivalX, playState.terrain);
  }

  if (playState.rivalX > 5) {
    if (isGroundPixel(playState.rivalX + 5, playState.rivalY + playState.rivalVel + 1,
                      playState.terrain) &&
        terrainUnder(playState.rivalX - 5, playState.terrain) == 240) {
      playState.rivalX = -5;
      pushMessage(String(settings.rival) + " fell away!", 160);
      playState.rivalMessageState = 3;
    } else if (playState.rivalY == 240) {
      playState.rivalX = -5;
      pushMessage(String(settings.rival) + " crashed out!", 160);
      playState.rivalMessageState = 2;
    } else if (isCliffPixel(playState.rivalX, playState.rivalY - 2, playState.terrain)) {
      playState.rivalX = -5;
      pushMessage(String(settings.rival) + " hit the cliff!", 160);
      playState.rivalMessageState = 2;
    }
  }
}

void updateRivalPosition() {
  if (playState.count == 500) {
    pushMessage(String(settings.rival) + " is coming!", 160);
  }

  if (playState.count > 500) {
    if (playState.count % 90 == 0) {
      if (playState.rivalX > 5) {
        if (playState.rivalImpulse == 0) {
          if (random(10) != 0) {
            playState.rivalImpulse = (random(5) == 0) ? random(-10, 0) : random(0, 20);
          } else {
            playState.rivalImpulse = (playState.rivalX < 50) ? 50 : (playState.rivalX > 150 ? -50 : 30);
          }
        }
      } else {
        playState.rivalImpulse = 10;
        pushMessage(String(settings.rival) + " restarts the chase!", 160);
      }
      playState.terrainTrend = random(-1, 2);
    }

    if (playState.rivalImpulse != 0 && playState.count % 3 == 0) {
      if (playState.rivalImpulse > 0) {
        if (playState.rivalX < 150) {
          ++playState.rivalX;
          --playState.rivalImpulse;
        } else {
          playState.rivalImpulse = 0;
        }
      } else {
        if (playState.rivalX > 10) {
          --playState.rivalX;
          ++playState.rivalImpulse;
        } else {
          playState.rivalImpulse = 0;
        }
      }
    }

    if (!playState.finished && playState.playerX < playState.rivalX) {
      playState.endReason = EndReason::Overtaken;
      finishRun();
    }
  }
}

void updatePlayFrame() {
  ++playState.count;

  if (playState.messageTimer > 0) {
    --playState.messageTimer;
  } else {
    playState.message = "";
  }

  if (playState.count % 15 == 0) {
    playState.rollerFrame = (playState.rollerFrame + 1) % 4;
  }

  spawnPresetIfNeeded();
  updateTerrainColumn();
  updateWorldObjects();
  updateRivalPosition();
  updatePlayerPhysics();
  updateRivalPhysics();
}

void handleJump() {
  if (playState.playerAirState < 2) {
    const int diff = terrainUnder(playState.playerX, playState.terrain) -
                     terrainUnder(playState.playerX - 7, playState.terrain);
    if (diff > 9) {
      return;
    }
    playState.playerVel = 5;
    ++playState.playerAirState;
    beep(1400, 18);
  }
}

void handleShove() {
  if (playState.count > 500 && playState.playerX - playState.rivalX < 10 &&
      playState.playerX - playState.rivalX >= 0) {
    ++playState.rivalSpark;
    playState.playerX += random(-1, 2);
    playState.rivalX += random(-1, 2);
    pushMessage("Handlebar clash!", 40);
    if (playState.rivalSpark > 5) {
      playState.rivalImpulse = -20;
      playState.rivalSpark = 1;
      beep(1800, 30);
    }
  }
}

void drawBike(int x, int y, uint16_t bodyColor, bool crashed) {
  const int sx = worldToScreenX(x);
  const int sy = worldToScreenY(y);
  const int wheelR = worldToScreenW(2);
  const int dx = worldToScreenW(5);
  const int riderY = worldToScreenH(7);
  frameBuffer.drawCircle(sx - dx, sy, wheelR, bodyColor);
  frameBuffer.drawCircle(sx + dx, sy, wheelR, bodyColor);
  if (crashed) {
    frameBuffer.drawLine(sx - dx, sy - riderY, sx + dx, sy + wheelR, bodyColor);
    frameBuffer.drawLine(sx + dx, sy - riderY, sx - dx, sy + wheelR, bodyColor);
    return;
  }
  frameBuffer.drawLine(sx - dx, sy, sx, sy - riderY / 2, bodyColor);
  frameBuffer.drawLine(sx, sy - riderY / 2, sx + dx, sy, bodyColor);
  frameBuffer.drawLine(sx, sy - riderY / 2, sx, sy - riderY, bodyColor);
  frameBuffer.drawLine(sx, sy - riderY, sx + dx / 2, sy - riderY - worldToScreenH(3), bodyColor);
  frameBuffer.fillCircle(sx + dx / 2, sy - riderY - worldToScreenH(5), worldToScreenW(2), bodyColor);
}

void drawTerrain(const std::array<int16_t, kTerrainCols>& terrain) {
  const uint16_t cliffColor = rgb(32, 32, 32);
  const uint16_t grassColor = rgb(80, 180, 60);
  for (int i = 0; i < kTerrainCols; ++i) {
    const int x = worldToScreenX(i * kColumnWidth);
    const int y = worldToScreenY(terrain[i]);
    const int w = worldToScreenW(kColumnWidth);
    const int h = frameBuffer.height() - y;
    const bool cliff = i > 0 && terrain[i - 1] - terrain[i] >= 10;
    frameBuffer.fillRect(x, y, w, h, cliff ? cliffColor : BLACK);
    if (terrain[i] != 240) {
      frameBuffer.drawFastHLine(x, y, w, grassColor);
    }
  }
}

void drawWorldObjects() {
  if (playState.obstacleX > -5) {
    frameBuffer.fillRect(worldToScreenX(playState.obstacleX), worldToScreenY(playState.obstacleY - 10),
                         worldToScreenW(10), worldToScreenH(10), rgb(255, 160, 0));
  }
  if (playState.gustX > -150) {
    frameBuffer.fillRect(worldToScreenX(playState.gustX), worldToScreenY(playState.gustY - 8),
                         worldToScreenW(150), worldToScreenH(8), rgb(120, 180, 255));
  }
  if (playState.bannerActive) {
    frameBuffer.fillRect(worldToScreenX(playState.bannerX), worldToScreenY(playState.bannerY),
                         worldToScreenW(124), worldToScreenH(18), rgb(255, 230, 0));
    frameBuffer.setTextColor(BLACK, rgb(255, 230, 0));
    frameBuffer.drawString("GO!", worldToScreenX(playState.bannerX + 48),
                           worldToScreenY(playState.bannerY + 3));
  }
  if (playState.gateX > -230) {
    const uint16_t gateColor = rgb(110, 110, 110);
    frameBuffer.fillRect(worldToScreenX(playState.gateX + 20), worldToScreenY(playState.gateY - 14),
                         worldToScreenW(61), worldToScreenH(14), gateColor);
    frameBuffer.fillRect(worldToScreenX(playState.gateX + 87), worldToScreenY(playState.gateY - 14),
                         worldToScreenW(56), worldToScreenH(14), gateColor);
    frameBuffer.fillRect(worldToScreenX(playState.gateX + 169), worldToScreenY(playState.gateY - 14),
                         worldToScreenW(61), worldToScreenH(14), gateColor);
  }
  if (playState.rollerActive) {
    const int baseX = worldToScreenX(playState.rollerX);
    const int y = worldToScreenY(229);
    const uint16_t rollerColor = playState.rollerFrame % 2 == 0 ? rgb(0, 150, 255) : rgb(0, 90, 180);
    frameBuffer.fillRect(baseX + worldToScreenX(20), y, worldToScreenW(46), worldToScreenH(11), rollerColor);
    frameBuffer.fillRect(baseX + worldToScreenX(102), y, worldToScreenW(46), worldToScreenH(11), rollerColor);
    frameBuffer.fillRect(baseX + worldToScreenX(184), y, worldToScreenW(46), worldToScreenH(11), rollerColor);
  }
}

void renderTitle() {
  frameBuffer.fillScreen(WHITE);
  drawTerrain(titleState.terrain);
  frameBuffer.setTextColor(BLACK, WHITE);
  frameBuffer.setTextDatum(top_left);
  frameBuffer.drawString("CHARISO ADV", 8, 8);
  frameBuffer.setTextColor(RED, WHITE);
  frameBuffer.drawString(String(highScore) + " m", 8, 44);
  frameBuffer.setTextColor(BLACK, WHITE);
  frameBuffer.drawString("ENTER/BtnA: start", 8, frameBuffer.height() - 44);
  frameBuffer.drawString("S: setup", 8, frameBuffer.height() - 30);
  if (highScore >= 2000) {
    frameBuffer.drawString("T: turbo mode", 8, frameBuffer.height() - 16);
  }
}

void renderPlay() {
  frameBuffer.fillScreen(WHITE);
  drawTerrain(playState.terrain);
  drawWorldObjects();
  drawBike(playState.playerX, playState.playerY, RED, playState.finished);
  if (playState.count > 500 && playState.rivalX > -4) {
    drawBike(playState.rivalX, playState.rivalY, rgb(0, 80, 220), false);
  }

  frameBuffer.setTextColor(BLACK, WHITE);
  frameBuffer.drawString(String(playState.count) + " m", 8, 6);
  frameBuffer.drawString("HI " + String(highScore), 88, 6);
  if (settings.showMeter) {
    frameBuffer.drawString("TOTAL " + String(playState.totalBeforeRun + playState.count), 152, 6);
  }
  if (playState.turbo) {
    frameBuffer.drawString("TURBO", frameBuffer.width() - 52, 6);
  }
  const int hintY = frameBuffer.height() > 170 ? 22 : 18;
  frameBuffer.drawString("A/D move  J shove  Q quit", 8, hintY);
  if (playState.message.length() > 0) {
    const int y = hintY + 16;
    frameBuffer.drawString(playState.message, 8, y);
  }
}

void renderSetup() {
  frameBuffer.fillScreen(WHITE);
  frameBuffer.setTextColor(BLACK, WHITE);
  frameBuffer.drawString("SETUP", 8, 8);
  for (int i = 0; i < 6; ++i) {
    const int y = 30 + i * 20;
    const bool selected = i == setupSelection;
    if (selected) {
      frameBuffer.fillRect(4, y - 2, frameBuffer.width() - 8, 18, rgb(220, 240, 255));
    }
    frameBuffer.setTextColor(BLACK, selected ? rgb(220, 240, 255) : WHITE);
    String value;
    switch (i) {
      case 0: value = settings.player; break;
      case 1: value = settings.rival; break;
      case 2: value = settings.commentator; break;
      case 3: value = settings.showMeter ? "on" : "off"; break;
      case 4: value = String(settings.volume); break;
      case 5: value = settings.vibration ? "on" : "off"; break;
    }
    frameBuffer.drawString(String(selected ? ">" : " ") + kSetupItems[i] + ": " + value, 8, y);
  }
  frameBuffer.setTextColor(BLACK, WHITE);
  frameBuffer.drawString("W/S select  A/D change", 8, frameBuffer.height() - 28);
  frameBuffer.drawString("ENTER edit  Q back", 8, frameBuffer.height() - 14);
}

void renderEdit() {
  frameBuffer.fillScreen(WHITE);
  frameBuffer.setTextColor(BLACK, WHITE);
  frameBuffer.drawString("EDIT " + editState.label, 8, 8);
  frameBuffer.drawRect(8, 32, frameBuffer.width() - 16, 28, BLACK);
  frameBuffer.drawString(editState.buffer + "_", 12, 40);
  frameBuffer.drawString("Type on keyboard, DEL erase", 8, 78);
  frameBuffer.drawString("ENTER or BtnA saves", 8, 94);
}

const char* endReasonText(const EndReason reason) {
  switch (reason) {
    case EndReason::Overtaken:
      return "The rival overtook you.";
    case EndReason::Cliff:
      return "You hit the cliff.";
    case EndReason::Fall:
      return "You fell into the valley.";
    case EndReason::GiveUp:
      return "You retired from the run.";
    default:
      return "Run finished.";
  }
}

void renderGameOver() {
  frameBuffer.fillScreen(WHITE);
  frameBuffer.setTextColor(BLACK, WHITE);
  frameBuffer.drawString("GAME OVER", 8, 8);
  frameBuffer.drawString(endReasonText(playState.endReason), 8, 30);
  frameBuffer.drawString("Score: " + String(playState.count) + " m", 8, 54);
  frameBuffer.drawString("High : " + String(highScore) + " m", 8, 70);
  frameBuffer.drawString("Total: " + String(totalMeters) + " m", 8, 86);
  if (playState.newHighScore) {
    frameBuffer.setTextColor(RED, WHITE);
    frameBuffer.drawString("NEW RECORD!", 8, 108);
    frameBuffer.setTextColor(BLACK, WHITE);
  }
  frameBuffer.drawString("ENTER/BtnA: title", 8, frameBuffer.height() - 28);
  frameBuffer.drawString("S: setup", 8, frameBuffer.height() - 14);
}

void renderCurrentScene() {
  switch (currentScene) {
    case Scene::Title:
      renderTitle();
      break;
    case Scene::Play:
      renderPlay();
      break;
    case Scene::Setup:
      renderSetup();
      break;
    case Scene::Edit:
      renderEdit();
      break;
    case Scene::GameOver:
      renderGameOver();
      break;
  }
}

void applySetupDelta(const int delta) {
  switch (setupSelection) {
    case 3:
      settings.showMeter = !settings.showMeter;
      break;
    case 4:
      settings.volume = clampInt(static_cast<int>(settings.volume) + delta, 0, 10);
      break;
    case 5:
      settings.vibration = !settings.vibration;
      break;
    default:
      return;
  }
  beep(1200, 18);
  saveSettings();
}

void handleSetupEnter() {
  switch (setupSelection) {
    case 0:
      beginEditScene("Player", &settings.player);
      break;
    case 1:
      beginEditScene("Rival", &settings.rival);
      break;
    case 2:
      beginEditScene("Commentator", &settings.commentator);
      break;
    default:
      applySetupDelta(1);
      break;
  }
}

void handleSceneKeys(const Keyboard_Class::KeysState& status) {
  switch (currentScene) {
    case Scene::Title:
      if (status.enter) {
        startGame(false);
      }
      for (char raw : status.word) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        if (c == 's') {
          beginSetupScene();
          beep(1100, 18);
        } else if (c == 't' && highScore >= 2000) {
          startGame(true);
        }
      }
      break;

    case Scene::Play:
      if (status.enter || status.space) {
        handleJump();
      }
      for (char raw : status.word) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        if (c == 'j') {
          handleShove();
        } else if (c == 'q') {
          playState.endReason = EndReason::GiveUp;
          finishRun();
        }
      }
      break;

    case Scene::Setup:
      if (status.enter) {
        handleSetupEnter();
      }
      if (status.del) {
        saveSettings();
        beginTitleScene();
      }
      for (char raw : status.word) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        if (c == 'w') {
          setupSelection = (setupSelection + 5) % 6;
          beep(900, 12);
        } else if (c == 's') {
          setupSelection = (setupSelection + 1) % 6;
          beep(900, 12);
        } else if (c == 'a') {
          applySetupDelta(-1);
        } else if (c == 'd') {
          applySetupDelta(1);
        } else if (c == 'q') {
          saveSettings();
          beginTitleScene();
        }
      }
      break;

    case Scene::Edit:
      for (char raw : status.word) {
        if (editState.buffer.length() < 18 && raw >= 32 && raw <= 126) {
          editState.buffer += raw;
          beep(1300, 8);
        }
      }
      if (status.del && editState.buffer.length() > 0) {
        editState.buffer.remove(editState.buffer.length() - 1);
        beep(800, 8);
      }
      if (status.enter && editState.target != nullptr) {
        if (editState.buffer.length() > 0) {
          *editState.target = editState.buffer;
          saveSettings();
        }
        beginSetupScene();
      }
      break;

    case Scene::GameOver:
      if (status.enter) {
        beginTitleScene();
      }
      for (char raw : status.word) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        if (c == 's') {
          beginSetupScene();
        }
      }
      break;
  }
}

void handleContinuousPlayInput() {
  if (currentScene != Scene::Play || playState.finished) {
    return;
  }
  if (playState.count % 2 == 0) {
    if (M5Cardputer.Keyboard.isKeyPressed('d') || M5Cardputer.Keyboard.isKeyPressed('D')) {
      if (playState.playerX < 220) {
        ++playState.playerX;
      }
    } else if (M5Cardputer.Keyboard.isKeyPressed('a') || M5Cardputer.Keyboard.isKeyPressed('A')) {
      if (playState.playerX > 20) {
        --playState.playerX;
      }
    }
  }
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextFont(1);
  frameBuffer.setColorDepth(16);
  frameBuffer.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  frameBuffer.setTextSize(1);
  frameBuffer.setTextFont(1);
  frameBuffer.setTextDatum(top_left);
  randomSeed(micros());
  loadSettings();
  beginTitleScene();
  lastRenderTick = millis();
}

void loop() {
  M5Cardputer.update();

  if (M5Cardputer.BtnA.wasPressed()) {
    switch (currentScene) {
      case Scene::Title:
        startGame(false);
        break;
      case Scene::Play:
        handleJump();
        break;
      case Scene::Setup:
        handleSetupEnter();
        break;
      case Scene::Edit:
        if (editState.target != nullptr) {
          if (editState.buffer.length() > 0) {
            *editState.target = editState.buffer;
            saveSettings();
          }
        }
        beginSetupScene();
        break;
      case Scene::GameOver:
        beginTitleScene();
        break;
    }
  }

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    handleSceneKeys(status);
  }

  handleContinuousPlayInput();

  const uint32_t now = millis();
  if (currentScene == Scene::Title) {
    if (now - titleState.lastTick >= kTitleStepMs) {
      titleState.lastTick = now;
      updateTitleTerrain();
    }
  } else if (currentScene == Scene::Play) {
    if (now - playState.lastTick >= playState.frameDelay) {
      playState.lastTick = now;
      updatePlayFrame();
    }
  }

  if (now - lastRenderTick >= kRenderIntervalMs) {
    lastRenderTick = now;
    renderCurrentScene();
    frameBuffer.pushSprite(0, 0);
  }
  delay(1);
}
