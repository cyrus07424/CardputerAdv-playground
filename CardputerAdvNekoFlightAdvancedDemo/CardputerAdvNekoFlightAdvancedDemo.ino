#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>

namespace app_config {
constexpr double FMAX = 50000.0;
constexpr int GSCALE = 32;
constexpr int PMAX = 4;
constexpr double G = -9.8;
constexpr double DT = 0.1;
constexpr uint32_t FRAME_INTERVAL_MS = 33;
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int16_t HUD_HEIGHT = 18;
constexpr int16_t FOOTER_Y = 124;
constexpr int16_t RETICLE_RADIUS = 8;
constexpr double CAMERA_SCALE = 42.0;
constexpr bool SHOW_AAM_LABEL = false;
constexpr bool SHOW_GUN_HEAT_LABEL = false;
}

struct Vec3 {
  double x;
  double y;
  double z;

  Vec3() : x(0.0), y(0.0), z(0.0) {}
  Vec3(double ax, double ay, double az) : x(ax), y(ay), z(az) {}

  Vec3& set(double ax, double ay, double az) {
    x = ax;
    y = ay;
    z = az;
    return *this;
  }

  Vec3& set(const Vec3& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    return *this;
  }

  Vec3& add(const Vec3& other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
  }

  Vec3& setPlus(const Vec3& a, const Vec3& b) {
    x = a.x + b.x;
    y = a.y + b.y;
    z = a.z + b.z;
    return *this;
  }

  Vec3& addCons(const Vec3& a, double c) {
    x += a.x * c;
    y += a.y * c;
    z += a.z * c;
    return *this;
  }

  Vec3& sub(const Vec3& other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
  }

  Vec3& setMinus(const Vec3& a, const Vec3& b) {
    x = a.x - b.x;
    y = a.y - b.y;
    z = a.z - b.z;
    return *this;
  }

  Vec3& subCons(const Vec3& a, double c) {
    x -= a.x * c;
    y -= a.y * c;
    z -= a.z * c;
    return *this;
  }

  Vec3& cons(double c) {
    x *= c;
    y *= c;
    z *= c;
    return *this;
  }

  Vec3& consInv(double c) {
    if (fabs(c) < 1e-9) {
      return *this;
    }
    x /= c;
    y /= c;
    z /= c;
    return *this;
  }

  Vec3& setCons(const Vec3& a, double c) {
    x = a.x * c;
    y = a.y * c;
    z = a.z * c;
    return *this;
  }

  Vec3& setConsInv(const Vec3& a, double c) {
    if (fabs(c) < 1e-9) {
      x = y = z = 0.0;
      return *this;
    }
    x = a.x / c;
    y = a.y / c;
    z = a.z / c;
    return *this;
  }

  double abs2() const {
    return x * x + y * y + z * z;
  }

  double abs() const {
    return sqrt(abs2());
  }
};

class GameWorld;
class Plane;

class Wing {
 public:
  Vec3 pVel;
  Vec3 xVel;
  Vec3 yVel;
  Vec3 zVel;
  double mass;
  double sVal;
  Vec3 fVel;
  double aAngle;
  double bAngle;
  Vec3 vVel;
  double tVal;

  Vec3 m_ti;
  Vec3 m_ni;
  Vec3 m_vp;
  Vec3 m_vp2;
  Vec3 m_wx;
  Vec3 m_wy;
  Vec3 m_wz;
  Vec3 m_qx;
  Vec3 m_qy;
  Vec3 m_qz;

  Wing();
  void calc(Plane& plane, double ve, int no, bool boost);
};

class Bullet {
 public:
  Vec3 pVel;
  Vec3 opVel;
  Vec3 vVel;
  int use;
  int bom;

  Vec3 m_a;
  Vec3 m_b;
  Vec3 m_vv;

  Bullet();
  void move(GameWorld& world, Plane& plane);
};

class Missile {
 public:
  static constexpr int MOMAX = 50;

  Vec3 pVel;
  Vec3 opVel[MOMAX];
  Vec3 vpVel;
  Vec3 aVel;
  int use;
  int bom;
  int bomm;
  int count;
  int targetNo;

  Vec3 m_a0;

  Missile();
  void homing(GameWorld& world, Plane& plane);
  void calcMotor();
  void move(GameWorld& world, Plane& plane);
};

class Plane {
 public:
  static constexpr int BMAX = 20;
  static constexpr int MMMAX = 4;
  static constexpr int WMAX = 6;
  static constexpr int MAXT = 50;

  double cosa, cosb, cosc, sina, sinb, sinc;
  double y00, y01, y02;
  double y10, y11, y12;
  double y20, y21, y22;

  double my00, my01, my02;
  double my10, my11, my12;
  double my20, my21, my22;

  bool use;
  int no;
  Wing wing[WMAX];
  Vec3 pVel;
  Vec3 vpVel;
  Vec3 vVel;
  Vec3 gVel;
  Vec3 aVel;
  Vec3 vaVel;
  Vec3 gcVel;
  double height;
  double gHeightValue;
  double mass;
  Vec3 iMass;
  bool onGround;
  double aoa;

  Vec3 stickPos;
  Vec3 stickVel;
  double stickR;
  double stickA;
  int power;
  int throttle;
  bool boost;
  bool gunShoot;
  bool aamShoot;
  int level;
  int target;

  Bullet bullet[BMAX];
  int gunTarget;
  int targetSx;
  int targetSy;
  double targetDis;
  double gunTime;
  double gunX;
  double gunY;
  double gunVx;
  double gunVy;
  int gunTemp;
  bool heatWait;

  Missile aam[MMMAX];
  int aamTarget[MMMAX];

  Plane();
  void posInit();
  void checkTrans();
  void checkTransM(const Vec3& p);
  void change_w2l(const Vec3& pw, Vec3& pl) const;
  void change_l2w(const Vec3& pl, Vec3& pw) const;
  void change_mw2l(const Vec3& pw, Vec3& pl) const;
  void change_ml2w(const Vec3& pl, Vec3& pw) const;
  void lockCheck(GameWorld& world);
  void move(GameWorld& world, bool autof);
  void keyScan(GameWorld& world);
  void moveCalc(GameWorld& world);
  void autoFlight(GameWorld& world);
  void moveBullet(GameWorld& world);
  void moveAam(GameWorld& world);
};

struct ControlState {
  bool shoot = false;
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool boost = false;
};

struct ProjectedPoint {
  int16_t x;
  int16_t y;
  double z;
  bool visible;
};

class GameWorld {
 public:
  Plane plane[app_config::PMAX];
  Vec3 camerapos;
  Vec3 ground_pos[app_config::GSCALE][app_config::GSCALE];
  Vec3 obj[20][3];
  bool obj_initialized;
  bool auto_flight;
  bool started;
  bool paused;
  bool chrome_visible;
  bool menu_visible;
  bool ui_attitude_visible;
  bool ui_reticle_visible;
  bool ui_lock_box_visible;
  bool ui_enemy_arrows_visible;
  bool ui_speed_visible;
  bool ui_altitude_visible;
  bool ui_tgt_visible;
  bool ui_aam_ammo_visible;
  bool ui_gun_heat_visible;
  bool ui_header_visible;
  bool ui_mode_banner_visible;
  bool ui_footer_visible;
  int menu_index;
  int menu_scroll;
  int screen_width;
  int screen_height;
  int center_x;
  int center_y;
  ControlState control;
  bool prev_toggle_auto;
  uint32_t frame_counter;

  GameWorld();
  void init();
  void objInit();
  void update();
  void draw(M5Canvas& canvas);
  void clear(M5Canvas& canvas);
  void change3d(const Plane& plane_ref, const Vec3& sp, Vec3& cp) const;
  double gHeight(double px, double py) const;
  void gGrad(double px, double py, Vec3& p) const;
  void writeGround(M5Canvas& canvas);
  void writePlane(M5Canvas& canvas);
  void writeGun(M5Canvas& canvas, Plane& aplane);
  void writeAam(M5Canvas& canvas, Plane& aplane);
  void drawSline(M5Canvas& canvas, const Vec3& p0, const Vec3& p1, uint16_t color = TFT_WHITE);
  void drawBlined(M5Canvas& canvas, const Vec3& p0, const Vec3& p1);
  void drawBline(M5Canvas& canvas, const Vec3& p0, const Vec3& p1);
  void drawMline(M5Canvas& canvas, const Vec3& p0, const Vec3& p1, uint16_t color);
  void drawAline(M5Canvas& canvas, const Vec3& p0, const Vec3& p1);
  void drawPoly(M5Canvas& canvas, const Vec3& p0, const Vec3& p1, const Vec3& p2,
                uint16_t color = TFT_WHITE);
  void fillBarc(M5Canvas& canvas, const Vec3& p);
};

M5Canvas g_canvas(&M5Cardputer.Display);
GameWorld g_world;
bool g_needs_redraw = true;
uint32_t g_last_frame_ms = 0;
bool g_prev_enter = false;
bool g_prev_reset = false;
bool g_prev_esc = false;
bool g_prev_fn = false;
bool g_prev_menu_up = false;
bool g_prev_menu_down = false;

double rand_unit() {
  return static_cast<double>(random(0, 1000000L)) / 1000000.0;
}

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t key_code) {
  for (const auto key : status.hid_keys) {
    if (key == key_code) {
      return true;
    }
  }
  return false;
}

void reset_stage_preserve_ui(GameWorld& world) {
  const bool chrome_visible = world.chrome_visible;
  const bool menu_visible = world.menu_visible;
  const bool ui_attitude_visible = world.ui_attitude_visible;
  const bool ui_reticle_visible = world.ui_reticle_visible;
  const bool ui_lock_box_visible = world.ui_lock_box_visible;
  const bool ui_enemy_arrows_visible = world.ui_enemy_arrows_visible;
  const bool ui_speed_visible = world.ui_speed_visible;
  const bool ui_altitude_visible = world.ui_altitude_visible;
  const bool ui_tgt_visible = world.ui_tgt_visible;
  const bool ui_aam_ammo_visible = world.ui_aam_ammo_visible;
  const bool ui_gun_heat_visible = world.ui_gun_heat_visible;
  const bool ui_header_visible = world.ui_header_visible;
  const bool ui_mode_banner_visible = world.ui_mode_banner_visible;
  const bool ui_footer_visible = world.ui_footer_visible;
  const int menu_index = world.menu_index;
  const int menu_scroll = world.menu_scroll;

  world.init();

  world.chrome_visible = chrome_visible;
  world.menu_visible = menu_visible;
  world.ui_attitude_visible = ui_attitude_visible;
  world.ui_reticle_visible = ui_reticle_visible;
  world.ui_lock_box_visible = ui_lock_box_visible;
  world.ui_enemy_arrows_visible = ui_enemy_arrows_visible;
  world.ui_speed_visible = ui_speed_visible;
  world.ui_altitude_visible = ui_altitude_visible;
  world.ui_tgt_visible = ui_tgt_visible;
  world.ui_aam_ammo_visible = ui_aam_ammo_visible;
  world.ui_gun_heat_visible = ui_gun_heat_visible;
  world.ui_header_visible = ui_header_visible;
  world.ui_mode_banner_visible = ui_mode_banner_visible;
  world.ui_footer_visible = ui_footer_visible;
  world.menu_index = menu_index;
  world.menu_scroll = menu_scroll;
  g_needs_redraw = true;
}

bool contains_char_key(const Keyboard_Class::KeysState& status, char key_code) {
  for (const auto key : status.word) {
    if (key == key_code) {
      return true;
    }
  }
  return false;
}

Wing::Wing()
    : mass(0.0),
      sVal(0.0),
      aAngle(0.0),
      bAngle(0.0),
      tVal(0.0) {}

Bullet::Bullet() : use(0), bom(0) {}

Missile::Missile()
    : use(-1),
      bom(0),
      bomm(0),
      count(0),
      targetNo(-1) {}

Plane::Plane()
    : cosa(0.0),
      cosb(0.0),
      cosc(0.0),
      sina(0.0),
      sinb(0.0),
      sinc(0.0),
      y00(0.0),
      y01(0.0),
      y02(0.0),
      y10(0.0),
      y11(0.0),
      y12(0.0),
      y20(0.0),
      y21(0.0),
      y22(0.0),
      my00(0.0),
      my01(0.0),
      my02(0.0),
      my10(0.0),
      my11(0.0),
      my12(0.0),
      my20(0.0),
      my21(0.0),
      my22(0.0),
      use(false),
      no(0),
      height(0.0),
      gHeightValue(0.0),
      mass(0.0),
      onGround(false),
      aoa(0.0),
      stickR(0.0),
      stickA(0.0),
      power(0),
      throttle(0),
      boost(false),
      gunShoot(false),
      aamShoot(false),
      level(0),
      target(-1),
      gunTarget(-1),
      targetSx(-1000),
      targetSy(0),
      targetDis(0.0),
      gunTime(1.0),
      gunX(0.0),
      gunY(0.0),
      gunVx(0.0),
      gunVy(0.0),
      gunTemp(0),
      heatWait(false) {
  posInit();
}

GameWorld::GameWorld()
    : obj_initialized(false),
      auto_flight(true),
      started(false),
      paused(false),
      chrome_visible(true),
      menu_visible(false),
      ui_attitude_visible(true),
      ui_reticle_visible(true),
      ui_lock_box_visible(true),
      ui_enemy_arrows_visible(true),
      ui_speed_visible(true),
      ui_altitude_visible(true),
      ui_tgt_visible(true),
      ui_aam_ammo_visible(true),
      ui_gun_heat_visible(true),
      ui_header_visible(true),
      ui_mode_banner_visible(true),
      ui_footer_visible(true),
      menu_index(0),
      menu_scroll(0),
      screen_width(app_config::SCREEN_W),
      screen_height(app_config::SCREEN_H),
      center_x(app_config::SCREEN_W / 2),
      center_y(72),
      prev_toggle_auto(false),
      frame_counter(0) {}

void Plane::posInit() {
  pVel.x = (rand_unit() - 0.5) * 1000.0 - 8000.0;
  pVel.y = (rand_unit() - 0.5) * 1000.0 - 1100.0;
  pVel.z = 5000.0;
  gHeightValue = 0.0;
  height = 5000.0;
  vpVel.x = 200.0;
  vpVel.y = 0.0;
  vpVel.z = 0.0;
  aVel.set(0.0, 0.0, PI / 2.0);
  gVel.set(0.0, 0.0, 0.0);
  vaVel.set(0.0, 0.0, 0.0);
  vVel.set(0.0, 0.0, 0.0);
  power = 5;
  throttle = 5;
  heatWait = false;
  gunTemp = 0;
  gcVel.set(pVel);
  target = -2;
  onGround = false;
  gunX = 0.0;
  gunY = 100.0;
  gunVx = 0.0;
  gunVy = 0.0;
  boost = false;
  aoa = 0.0;
  stickPos.set(0.0, 0.0, 0.0);
  stickVel.set(0.0, 0.0, 0.0);
  stickR = 0.1;
  stickA = 0.1;
  gunTarget = -1;
  targetDis = 0.0;
  gunTime = 1.0;

  const double wa = 45.0 * PI / 180.0;

  wing[0].pVel.set(3.0, 0.1, 0.0);
  wing[0].xVel.set(cos(wa), -sin(wa), 0.0);
  wing[0].yVel.set(sin(wa), cos(wa), 0.0);
  wing[0].zVel.set(0.0, 0.0, 1.0);

  wing[1].pVel.set(-3.0, 0.1, 0.0);
  wing[1].xVel.set(cos(wa), sin(wa), 0.0);
  wing[1].yVel.set(-sin(wa), cos(wa), 0.0);
  wing[1].zVel.set(0.0, 0.0, 1.0);

  wing[2].pVel.set(0.0, -10.0, 2.0);
  wing[2].xVel.set(1.0, 0.0, 0.0);
  wing[2].yVel.set(0.0, 1.0, 0.0);
  wing[2].zVel.set(0.0, 0.0, 1.0);

  wing[3].pVel.set(0.0, -10.0, 0.0);
  wing[3].xVel.set(0.0, 0.0, 1.0);
  wing[3].yVel.set(0.0, 1.0, 0.0);
  wing[3].zVel.set(1.0, 0.0, 0.0);

  wing[4].pVel.set(5.0, 0.0, 0.0);
  wing[4].xVel.set(1.0, 0.0, 0.0);
  wing[4].yVel.set(0.0, 1.0, 0.0);
  wing[4].zVel.set(0.0, 0.0, 1.0);

  wing[5].pVel.set(-5.0, 0.0, 0.0);
  wing[5].xVel.set(1.0, 0.0, 0.0);
  wing[5].yVel.set(0.0, 1.0, 0.0);
  wing[5].zVel.set(0.0, 0.0, 1.0);

  wing[0].mass = 200.0;
  wing[1].mass = 200.0;
  wing[2].mass = 50.0;
  wing[3].mass = 50.0;
  wing[4].mass = 300.0;
  wing[5].mass = 300.0;

  wing[0].sVal = 30.0;
  wing[1].sVal = 30.0;
  wing[2].sVal = 2.0;
  wing[3].sVal = 2.0;
  wing[4].sVal = 0.0;
  wing[5].sVal = 0.0;

  wing[0].tVal = 0.1;
  wing[1].tVal = 0.1;
  wing[2].tVal = 0.1;
  wing[3].tVal = 0.1;
  wing[4].tVal = 1000.0;
  wing[5].tVal = 1000.0;

  mass = 0.0;
  iMass.set(1000.0, 1000.0, 4000.0);
  const double m_i = 1.0;
  for (int i = 0; i < WMAX; ++i) {
    mass += wing[i].mass;
    wing[i].aAngle = 0.0;
    wing[i].bAngle = 0.0;
    wing[i].vVel.set(0.0, 0.0, 1.0);
    iMass.x += wing[i].mass * (fabs(wing[i].pVel.x) + 1.0) * m_i * m_i;
    iMass.y += wing[i].mass * (fabs(wing[i].pVel.y) + 1.0) * m_i * m_i;
    iMass.z += wing[i].mass * (fabs(wing[i].pVel.z) + 1.0) * m_i * m_i;
  }

  for (int i = 0; i < BMAX; ++i) {
    bullet[i].use = 0;
    bullet[i].bom = 0;
  }
  for (int i = 0; i < MMMAX; ++i) {
    aam[i].use = -1;
    aam[i].bom = 0;
    aam[i].count = 0;
    aamTarget[i] = -1;
  }
}

void Plane::checkTrans() {
  const double x = aVel.x;
  sina = sin(x);
  cosa = cos(x);
  if (cosa < 1e-9 && cosa > 0.0) {
    cosa = 1e-9;
  }
  if (cosa > -1e-9 && cosa < 0.0) {
    cosa = -1e-9;
  }
  sinb = sin(aVel.y);
  cosb = cos(aVel.y);
  sinc = sin(aVel.z);
  cosc = cos(aVel.z);
  const double sasc = sina * sinc;
  const double sacc = sina * cosc;

  y00 = cosb * cosc - sasc * sinb;
  y01 = -cosb * sinc - sacc * sinb;
  y02 = -sinb * cosa;
  y10 = cosa * sinc;
  y11 = cosa * cosc;
  y12 = -sina;
  y20 = sinb * cosc + sasc * cosb;
  y21 = -sinb * sinc + sacc * cosb;
  y22 = cosb * cosa;
}

void Plane::checkTransM(const Vec3& p) {
  double mcosa = cos(p.x);
  const double msina = sin(p.x);
  if (mcosa < 1e-9 && mcosa > 0.0) {
    mcosa = 1e-9;
  }
  if (mcosa > -1e-9 && mcosa < 0.0) {
    mcosa = -1e-9;
  }
  const double msinb = sin(p.y);
  const double mcosb = cos(p.y);
  const double msinc = sin(p.z);
  const double mcosc = cos(p.z);
  const double msasc = msina * msinc;
  const double msacc = msina * mcosc;

  my00 = mcosb * mcosc - msasc * msinb;
  my01 = -mcosb * msinc - msacc * msinb;
  my02 = -msinb * mcosa;
  my10 = mcosa * msinc;
  my11 = mcosa * mcosc;
  my12 = -msina;
  my20 = msinb * mcosc + msasc * mcosb;
  my21 = -msinb * msinc + msacc * mcosb;
  my22 = mcosb * mcosa;
}

void Plane::change_w2l(const Vec3& pw, Vec3& pl) const {
  pl.x = pw.x * y00 + pw.y * y01 + pw.z * y02;
  pl.y = pw.x * y10 + pw.y * y11 + pw.z * y12;
  pl.z = pw.x * y20 + pw.y * y21 + pw.z * y22;
}

void Plane::change_l2w(const Vec3& pl, Vec3& pw) const {
  pw.x = pl.x * y00 + pl.y * y10 + pl.z * y20;
  pw.y = pl.x * y01 + pl.y * y11 + pl.z * y21;
  pw.z = pl.x * y02 + pl.y * y12 + pl.z * y22;
}

void Plane::change_mw2l(const Vec3& pw, Vec3& pl) const {
  pl.x = pw.x * my00 + pw.y * my01 + pw.z * my02;
  pl.y = pw.x * my10 + pw.y * my11 + pw.z * my12;
  pl.z = pw.x * my20 + pw.y * my21 + pw.z * my22;
}

void Plane::change_ml2w(const Vec3& pl, Vec3& pw) const {
  pw.x = pl.x * my00 + pl.y * my10 + pl.z * my20;
  pw.y = pl.x * my01 + pl.y * my11 + pl.z * my21;
  pw.z = pl.x * my02 + pl.y * my12 + pl.z * my22;
}

void Wing::calc(Plane& plane, double ve, int no, bool boost_active) {
  double ff = 0.0;
  fVel.set(0.0, 0.0, 0.0);

  m_vp.x = plane.vVel.x + pVel.y * plane.vaVel.z - pVel.z * plane.vaVel.y;
  m_vp.y = plane.vVel.y + pVel.z * plane.vaVel.x - pVel.x * plane.vaVel.z;
  m_vp.z = plane.vVel.z + pVel.x * plane.vaVel.y - pVel.y * plane.vaVel.x;

  double sinv = sin(bAngle);
  double cosv = cos(bAngle);

  m_qx.x = xVel.x * cosv - zVel.x * sinv;
  m_qx.y = xVel.y * cosv - zVel.y * sinv;
  m_qx.z = xVel.z * cosv - zVel.z * sinv;
  m_qy.set(yVel);
  m_qz.x = xVel.x * sinv + zVel.x * cosv;
  m_qz.y = xVel.y * sinv + zVel.y * cosv;
  m_qz.z = xVel.z * sinv + zVel.z * cosv;

  sinv = sin(aAngle);
  cosv = cos(aAngle);

  m_wx.set(m_qx);
  m_wy.x = m_qy.x * cosv - m_qz.x * sinv;
  m_wy.y = m_qy.y * cosv - m_qz.y * sinv;
  m_wy.z = m_qy.z * cosv - m_qz.z * sinv;
  m_wz.x = m_qy.x * sinv + m_qz.x * cosv;
  m_wz.y = m_qy.y * sinv + m_qz.y * cosv;
  m_wz.z = m_qy.z * sinv + m_qz.z * cosv;

  if (sVal > 0.0) {
    double vv = m_vp.abs();
    if (vv < 1e-6) {
      vv = 1e-6;
    }
    m_ti.setConsInv(m_vp, vv);

    const double dx = m_wx.x * m_vp.x + m_wx.y * m_vp.y + m_wx.z * m_vp.z;
    const double dy = m_wy.x * m_vp.x + m_wy.y * m_vp.y + m_wy.z * m_vp.z;
    const double dz = m_wz.x * m_vp.x + m_wz.y * m_vp.y + m_wz.z * m_vp.z;
    const double rr = sqrt(dx * dx + dy * dy);

    if (rr > 0.001) {
      m_vp2.x = (m_wx.x * dx + m_wy.x * dy) / rr;
      m_vp2.y = (m_wx.y * dx + m_wy.y * dy) / rr;
      m_vp2.z = (m_wx.z * dx + m_wy.z * dy) / rr;
    } else {
      m_vp2.x = m_wx.x * dx + m_wy.x * dy;
      m_vp2.y = m_wx.y * dx + m_wy.y * dy;
      m_vp2.z = m_wx.z * dx + m_wy.z * dy;
    }

    m_ni.x = m_wz.x * rr - m_vp2.x * dz;
    m_ni.y = m_wz.y * rr - m_vp2.y * dz;
    m_ni.z = m_wz.z * rr - m_vp2.z * dz;

    double ni_abs = m_ni.abs();
    if (ni_abs < 1e-6) {
      ni_abs = 1e-6;
    }
    m_ni.consInv(ni_abs);

    const double at = -atan2(dz, dy);
    if (no == 0) {
      plane.aoa = at;
    }

    double cl;
    double cd;
    if (fabs(at) < 0.4) {
      cl = at * 4.0;
      cd = at * at + 0.05;
    } else {
      cl = 0.0;
      cd = 0.4 * 0.4 + 0.05;
    }

    const double drag = 0.5 * ni_abs * ni_abs * cd * ve * sVal;
    const double lift = 0.5 * rr * rr * cl * ve * sVal;
    fVel.x = lift * m_ni.x - drag * m_ti.x;
    fVel.y = lift * m_ni.y - drag * m_ti.y;
    fVel.z = lift * m_ni.z - drag * m_ti.z;
  }

  if (tVal > 0.0) {
    if (boost_active) {
      ff = (5.0 * 10.0) / 0.9 * ve * 4.8 * tVal;
    } else {
      ff = plane.power / 0.9 * ve * 4.8 * tVal;
    }
    if (plane.height < 20.0) {
      ff *= (1.0 + (20.0 - plane.height) / 40.0);
    }
    fVel.addCons(m_wy, ff);
  }

  vVel.set(m_wy);
}

void Plane::lockCheck(GameWorld& world) {
  Vec3 a;
  Vec3 b;
  int nno[MMMAX];
  double dis[MMMAX];

  for (int m = 0; m < MMMAX; ++m) {
    dis[m] = 1e30;
    nno[m] = -1;
  }

  for (int m = 0; m < app_config::PMAX; ++m) {
    if (m == no || !world.plane[m].use) {
      continue;
    }

    a.setMinus(pVel, world.plane[m].pVel);
    const double near_dis = a.abs2();
    if (near_dis >= 1e8) {
      continue;
    }

    change_w2l(a, b);
    if (b.y <= 0.0 && sqrt(b.x * b.x + b.z * b.z) < -b.y * 0.24) {
      for (int m1 = 0; m1 < MMMAX; ++m1) {
        if (near_dis < dis[m1]) {
          for (int m2 = MMMAX - 1; m2 > m1; --m2) {
            dis[m2] = dis[m2 - 1];
            nno[m2] = nno[m2 - 1];
          }
          dis[m1] = near_dis;
          nno[m1] = m;
          break;
        }
      }
    }
  }

  for (int m1 = 1; m1 < 4; ++m1) {
    if (nno[m1] < 0) {
      nno[m1] = nno[0];
      dis[m1] = dis[0];
    }
  }

  for (int m1 = 4; m1 < MMMAX; ++m1) {
    nno[m1] = nno[m1 % 4];
    dis[m1] = dis[m1 % 4];
  }

  for (int m1 = 0; m1 < MMMAX; ++m1) {
    aamTarget[m1] = nno[m1];
  }

  gunTarget = nno[0];
  targetDis = dis[0] < 1e20 ? sqrt(dis[0]) : 0.0;
}

void Plane::move(GameWorld& world, bool autof) {
  checkTrans();
  lockCheck(world);

  if (no == 0 && !autof) {
    keyScan(world);
  } else {
    autoFlight(world);
  }

  moveCalc(world);
  moveBullet(world);
  moveAam(world);
}

void Plane::keyScan(GameWorld& world) {
  stickVel.set(0.0, 0.0, 0.0);
  boost = false;
  gunShoot = world.control.shoot;
  aamShoot = world.control.shoot;

  if (world.control.boost) {
    boost = true;
  }

  if (world.control.up) {
    stickVel.x = 1.0;
  }
  if (world.control.down) {
    stickVel.x = -1.0;
  }
  if (world.control.left) {
    stickVel.y = -1.0;
  }
  if (world.control.right) {
    stickVel.y = 1.0;
  }

  if (stickPos.z > 1.0) {
    stickPos.z = 1.0;
  }
  if (stickPos.z < -1.0) {
    stickPos.z = -1.0;
  }

  stickPos.addCons(stickVel, stickA);
  stickPos.subCons(stickPos, stickR);

  const double r = sqrt(stickPos.x * stickPos.x + stickPos.y * stickPos.y);
  if (r > 1.0) {
    stickPos.x /= r;
    stickPos.y /= r;
  }
}

void Plane::moveCalc(GameWorld& world) {
  Vec3 dm;

  targetSx = -1000;
  targetSy = 0;
  if (gunTarget >= 0 && world.plane[gunTarget].use) {
    world.change3d(*this, world.plane[gunTarget].pVel, dm);
    if (dm.x > 0.0 && dm.x < world.screen_width && dm.y > 0.0 && dm.y < world.screen_height) {
      targetSx = static_cast<int>(dm.x);
      targetSy = static_cast<int>(dm.y);
    }
  }

  gHeightValue = world.gHeight(pVel.x, pVel.y);
  height = pVel.z - gHeightValue;

  double ve;
  if (pVel.z < 5000.0) {
    ve = 0.12492 - 0.000008 * pVel.z;
  } else {
    ve = (0.12492 - 0.04) - 0.000002 * (pVel.z - 5000.0);
  }
  if (ve < 0.0) {
    ve = 0.0;
  }

  wing[0].aAngle = -stickPos.y * 1.5 / 180.0 * PI;
  wing[1].aAngle = stickPos.y * 1.5 / 180.0 * PI;
  wing[2].aAngle = -stickPos.x * 6.0 / 180.0 * PI;
  wing[3].aAngle = stickPos.z * 6.0 / 180.0 * PI;
  wing[0].bAngle = wing[1].bAngle = wing[2].bAngle = wing[3].bAngle = 0.0;
  wing[4].aAngle = wing[4].bAngle = 0.0;
  wing[5].aAngle = wing[5].bAngle = 0.0;

  change_w2l(vpVel, vVel);
  onGround = height < 5.0;

  Vec3 af;
  Vec3 am;
  af.set(0.0, 0.0, 0.0);
  am.set(0.0, 0.0, 0.0);

  aoa = 0.0;
  for (int m = 0; m < WMAX; ++m) {
    wing[m].calc(*this, ve, m, boost);
    af.x += (wing[m].fVel.x * y00 + wing[m].fVel.y * y10 + wing[m].fVel.z * y20);
    af.y += (wing[m].fVel.x * y01 + wing[m].fVel.y * y11 + wing[m].fVel.z * y21);
    af.z += (wing[m].fVel.x * y02 + wing[m].fVel.y * y12 + wing[m].fVel.z * y22) + wing[m].mass * app_config::G;
    am.x -= (wing[m].pVel.y * wing[m].fVel.z - wing[m].pVel.z * wing[m].fVel.y);
    am.y -= (wing[m].pVel.z * wing[m].fVel.x - wing[m].pVel.x * wing[m].fVel.z);
    am.z -= (wing[m].pVel.x * wing[m].fVel.y - wing[m].pVel.y * wing[m].fVel.x);
  }

  vaVel.x += am.x / iMass.x * app_config::DT;
  vaVel.y += am.y / iMass.y * app_config::DT;
  vaVel.z += am.z / iMass.z * app_config::DT;

  aVel.x += (vaVel.x * cosb + vaVel.z * sinb) * app_config::DT;
  aVel.y += (vaVel.y + (vaVel.x * sinb - vaVel.z * cosb) * sina / cosa) * app_config::DT;
  aVel.z += (-vaVel.x * sinb + vaVel.z * cosb) / cosa * app_config::DT;

  for (int q = 0; q < 3 && aVel.x >= PI / 2.0; ++q) {
    aVel.x = PI - aVel.x;
    aVel.y += PI;
    aVel.z += PI;
  }
  for (int q = 0; q < 3 && aVel.x < -PI / 2.0; ++q) {
    aVel.x = -PI - aVel.x;
    aVel.y += PI;
    aVel.z += PI;
  }
  for (int q = 0; q < 3 && aVel.y >= PI; ++q) {
    aVel.y -= PI * 2.0;
  }
  for (int q = 0; q < 3 && aVel.y < -PI; ++q) {
    aVel.y += PI * 2.0;
  }
  for (int q = 0; q < 3 && aVel.z >= PI * 2.0; ++q) {
    aVel.z -= PI * 2.0;
  }
  for (int q = 0; q < 3 && aVel.z < 0.0; ++q) {
    aVel.z += PI * 2.0;
  }

  gVel.setConsInv(af, mass);
  vpVel.x -= vpVel.x * vpVel.x * 0.00002;
  vpVel.y -= vpVel.y * vpVel.y * 0.00002;
  vpVel.z -= vpVel.z * vpVel.z * 0.00002;

  world.gGrad(pVel.x, pVel.y, dm);
  if (onGround) {
    gVel.x -= dm.x * 10.0;
    gVel.y -= dm.y * 10.0;
    const double vz = dm.x * vpVel.x + dm.y * vpVel.y;
    vpVel.z = vz;
  }

  if (boost) {
    gVel.x += (rand_unit() - 0.5) * 5.0;
    gVel.y += (rand_unit() - 0.5) * 5.0;
    gVel.z += (rand_unit() - 0.5) * 5.0;
  }

  vpVel.addCons(gVel, app_config::DT);
  pVel.addCons(vpVel, app_config::DT);

  if (height < 2.0) {
    pVel.z = gHeightValue + 2.0;
    height = 2.0;
    vpVel.z *= -0.1;
  }

  if (height < 5.0 &&
      (fabs(vpVel.z) > 50.0 || fabs(aVel.y) > 20.0 * PI / 180.0 || aVel.x > 10.0 * PI / 180.0)) {
    reset_stage_preserve_ui(world);
  }
}

void Plane::autoFlight(GameWorld& world) {
  gunShoot = false;
  aamShoot = false;

  if (target < 0 || !world.plane[target].use) {
    return;
  }

  power = 4;
  throttle = power;
  stickPos.z = 0.0;

  if (level < 0) {
    level = 0;
  }

  Vec3 dm_p;
  Vec3 dm_a;
  dm_p.setMinus(pVel, world.plane[target].pVel);
  change_w2l(dm_p, dm_a);

  double mm = level >= 20 ? 1.0 : (level + 1) * 0.05;
  stickVel.x = 0.0;
  stickVel.y = 0.0;

  double m = sqrt(dm_a.x * dm_a.x + dm_a.z * dm_a.z);
  if (m < 1e-6) {
    m = 1e-6;
  }

  if (level > 8 && gunTime < 1.0) {
    power = world.plane[target].power;
  } else {
    power = 9;
  }

  if (dm_a.z < 0.0) {
    stickVel.x = dm_a.z / m * mm;
  }
  stickVel.y = -dm_a.x / m * mm * 0.4;

  if (stickVel.y > 1.0) {
    stickVel.y = 1.0;
  }
  if (stickVel.y < -1.0) {
    stickVel.y = -1.0;
  }

  stickPos.x += stickVel.x;
  stickPos.y += stickVel.y;
  if (stickPos.x > 1.0) {
    stickPos.x = 1.0;
  }
  if (stickPos.x < -1.0) {
    stickPos.x = -1.0;
  }
  if (stickPos.y > 1.0) {
    stickPos.y = 1.0;
  }
  if (stickPos.y < -1.0) {
    stickPos.y = -1.0;
  }

  if (height < 1000.0 || height + vpVel.z * 8.0 < 0.0) {
    stickPos.y = -aVel.y;
    if (fabs(aVel.y) < PI / 2.0) {
      stickPos.x = -1.0;
    } else {
      stickPos.x = 0.0;
    }
  }

  m = sqrt(stickPos.x * stickPos.x + stickPos.y * stickPos.y);
  if (m > mm) {
    stickPos.x *= mm / m;
    stickPos.y *= mm / m;
  }

  if (gunTarget == target && gunTime < 1.0 && !heatWait && gunTemp < MAXT) {
    gunShoot = true;
  }
  if (gunTarget == target) {
    aamShoot = true;
  }

  if (fabs(aoa) > 0.35) {
    stickPos.x = 0.0;
  }
}

void Plane::moveBullet(GameWorld& world) {
  Vec3 sc;
  Vec3 a;
  Vec3 b;
  Vec3 c;
  Vec3 dm;
  Vec3 oi;
  Vec3 ni;

  dm.set(gunX * 400.0 / 200.0, 400.0, gunY * 400.0 / 200.0);
  change_l2w(dm, oi);
  oi.add(vpVel);
  gunTime = 1.0;

  dm.set(8.0, 10.0, -2.0);
  change_l2w(dm, ni);

  if (gunTarget >= 0 && targetDis > 0.0) {
    gunTime = targetDis / (oi.abs() * 1.1);
  }
  if (gunTime > 1.0) {
    gunTime = 1.0;
  }

  gcVel.x = pVel.x + ni.x + (oi.x - gVel.x * gunTime) * gunTime;
  gcVel.y = pVel.y + ni.y + (oi.y - gVel.y * gunTime) * gunTime;
  gcVel.z = pVel.z + ni.z + (oi.z + (-9.8 - gVel.z) * gunTime / 2.0) * gunTime;

  world.change3d(*this, gcVel, sc);

  if (gunTarget >= 0) {
    c.set(world.plane[gunTarget].pVel);
    c.addCons(world.plane[gunTarget].vpVel, gunTime);
    world.change3d(*this, c, a);
    world.change3d(*this, world.plane[gunTarget].pVel, b);
    sc.x += b.x - a.x;
    sc.y += b.y - a.y;
  }

  if (targetSx > -1000) {
    double xx = targetSx - sc.x;
    double yy = targetSy - sc.y;
    double mm = sqrt(xx * xx + yy * yy);
    if (mm > 20.0) {
      xx = xx / mm * 20.0;
      yy = yy / mm * 20.0;
    }
    gunVx += xx;
    gunVy -= yy;
  } else {
    gunVx += (-gunX) * 0.08;
    gunVy += (20.0 - gunY) * 0.08;
  }
  gunX += gunVx * 100.0 / 300.0;
  gunY += gunVy * 100.0 / 300.0;
  gunVx -= gunVx * 0.3;
  gunVy -= gunVy * 0.3;

  const double y = gunY - 20.0;
  const double r = sqrt(gunX * gunX + gunY * gunY);
  if (r > 100.0) {
    const double x = gunX * 100.0 / r;
    const double yy = y * 100.0 / r;
    gunX = x;
    gunY = yy + 20.0;
    gunVx = 0.0;
    gunVy = 0.0;
  }

  for (int i = 0; i < BMAX; ++i) {
    if (bullet[i].use != 0) {
      bullet[i].move(world, *this);
    }
  }

  if (gunShoot && !heatWait && gunTemp < Plane::MAXT) {
    ++gunTemp;
    for (int i = 0; i < BMAX; ++i) {
      if (bullet[i].use == 0) {
        bullet[i].vVel.setPlus(vpVel, oi);
        const double aa = rand_unit();
        bullet[i].pVel.setPlus(pVel, ni);
        bullet[i].pVel.addCons(bullet[i].vVel, 0.1 * aa);
        bullet[i].opVel.set(bullet[i].pVel);
        bullet[i].bom = 0;
        bullet[i].use = 15;
        break;
      }
    }

    if (gunTemp >= Plane::MAXT) {
      heatWait = true;
    }
  } else if (gunTemp > 0) {
    gunTemp--;
  }

  if (gunTemp <= 0) {
    gunTemp = 0;
    heatWait = false;
  }
}

void Plane::moveAam(GameWorld& world) {
  Vec3 dm;
  Vec3 ni;
  Vec3 oi;

  for (int k = 0; k < MMMAX; ++k) {
    if (aam[k].use > 0) {
      aam[k].move(world, *this);
    }
    if (aam[k].use == 0) {
      aam[k].use = -1;
    }
  }

  if (!aamShoot || targetDis <= 50.0) {
    return;
  }

  int k;
  for (k = 0; k < MMMAX; ++k) {
    if (aam[k].use < 0 && aamTarget[k] >= 0) {
      break;
    }
  }
  if (k == MMMAX) {
    return;
  }

  Missile& ap = aam[k];

  switch (k % 4) {
    case 0:
      dm.x = 6.0;
      dm.z = 1.0;
      break;
    case 1:
      dm.x = -6.0;
      dm.z = 1.0;
      break;
    case 2:
      dm.x = 6.0;
      dm.z = -1.0;
      break;
    default:
      dm.x = -6.0;
      dm.z = -1.0;
      break;
  }
  dm.y = 2.0;
  change_l2w(dm, ni);

  const double v3 = 5.0;
  const double vx = rand_unit() * v3;
  const double vy = rand_unit() * v3;
  switch (k % 4) {
    case 0:
      dm.x = vx;
      dm.z = vy;
      break;
    case 1:
      dm.x = -vx;
      dm.z = vy;
      break;
    case 2:
      dm.x = vx;
      dm.z = -vy;
      break;
    default:
      dm.x = -vx;
      dm.z = -vy;
      break;
  }
  dm.y = 40.0;
  change_l2w(dm, oi);

  ap.pVel.setPlus(pVel, ni);
  ap.vpVel.setPlus(vpVel, oi);

  switch (k % 4) {
    case 0:
      dm.x = 8.0;
      dm.z = 11.0;
      break;
    case 1:
      dm.x = -8.0;
      dm.z = 11.0;
      break;
    case 2:
      dm.x = 5.0;
      dm.z = 9.0;
      break;
    default:
      dm.x = -5.0;
      dm.z = 9.0;
      break;
  }
  dm.y = 50.0;
  change_l2w(dm, oi);
  const double v = oi.abs();
  ap.aVel.setConsInv(oi, v);
  ap.use = 100;
  ap.count = 0;
  ap.bom = 0;
  ap.targetNo = aamTarget[k];
}

void Bullet::move(GameWorld& world, Plane& plane) {
  vVel.z += app_config::G * app_config::DT;
  opVel.set(pVel);
  pVel.addCons(vVel, app_config::DT);
  use--;

  if (plane.gunTarget > -1) {
    m_a.setMinus(pVel, world.plane[plane.gunTarget].pVel);
    m_b.setMinus(opVel, world.plane[plane.gunTarget].pVel);
    m_vv.setCons(vVel, app_config::DT);

    const double v0 = m_vv.abs();
    const double l = m_a.abs() + m_b.abs();
    if (l < v0 * 1.05) {
      bom = 1;
      use = 10;
      m_vv.x = (m_a.x + m_b.x) / 2.0;
      m_vv.y = (m_a.y + m_b.y) / 2.0;
      m_vv.z = (m_a.z + m_b.z) / 2.0;
      double len = m_vv.abs();
      if (len < 1e-6) {
        len = 1e-6;
      }
      m_vv.consInv(len);
      vVel.addCons(m_vv, v0 / 0.1);
      vVel.cons(0.1);
    }
  }

  const double gh = world.gHeight(pVel.x, pVel.y);
  if (pVel.z < gh) {
    vVel.z = fabs(vVel.z);
    pVel.z = gh;
    vVel.x += (rand_unit() - 0.5) * 50.0;
    vVel.y += (rand_unit() - 0.5) * 50.0;
    vVel.x *= 0.5;
    vVel.y *= 0.5;
    vVel.z *= 0.1;
  }
}

void Missile::homing(GameWorld& world, Plane&) {
  if (targetNo >= 0 && use < 85) {
    double v = vpVel.abs();
    if (fabs(v) < 1.0) {
      v = 1.0;
    }

    Plane& tp = world.plane[targetNo];
    m_a0.setMinus(tp.pVel, pVel);
    double l = m_a0.abs();
    if (l < 0.001) {
      l = 0.001;
    }

    m_a0.setMinus(tp.vpVel, vpVel);
    const double m = m_a0.abs();
    double t0 = l / v * (1.0 - m / 801.0);
    if (t0 < 0.0) {
      t0 = 0.0;
    }
    if (t0 > 5.0) {
      t0 = 5.0;
    }

    m_a0.x = tp.pVel.x + tp.vpVel.x * t0 - (pVel.x + vpVel.x * t0);
    m_a0.y = tp.pVel.y + tp.vpVel.y * t0 - (pVel.y + vpVel.y * t0);
    m_a0.z = tp.pVel.z + tp.vpVel.z * t0 - (pVel.z + vpVel.z * t0);

    double tr = ((85) - use) * 0.02 + 0.5;
    if (tr > 0.1) {
      tr = 0.1;
    }
    if (tr < 1.0) {
      l = m_a0.abs();
      aVel.addCons(m_a0, l * tr * 10.0);
    } else {
      aVel.set(m_a0);
    }

    double a_abs = aVel.abs();
    if (a_abs < 1e-6) {
      a_abs = 1e-6;
    }
    aVel.consInv(a_abs);
  }
}

void Missile::calcMotor() {
  if (use < 95) {
    const double aa = 1.0 / 20.0;
    const double bb = 1.0 - aa;
    const double v = vpVel.abs();
    vpVel.x = aVel.x * v * aa + vpVel.x * bb;
    vpVel.y = aVel.y * v * aa + vpVel.y * bb;
    vpVel.z = aVel.z * v * aa + vpVel.z * bb;
    vpVel.addCons(aVel, 10.0);
  }
}

void Missile::move(GameWorld& world, Plane& plane) {
  if (bom > 0) {
    count = 0;
    bom--;
    if (bom < 0) {
      use = 0;
    }
    return;
  }

  vpVel.z += app_config::G * app_config::DT;
  homing(world, plane);
  calcMotor();
  opVel[use % MOMAX].set(pVel);
  pVel.addCons(vpVel, app_config::DT);
  use--;

  if (targetNo >= 0) {
    Plane& tp = world.plane[targetNo];
    m_a0.setMinus(pVel, tp.pVel);
    if (m_a0.abs() < 10.0) {
      bom = 10;
    }
  }

  const double gh = world.gHeight(pVel.x, pVel.y);
  if (pVel.z < gh) {
    bom = 10;
    pVel.z = gh + 3.0;
  }
  if (count < MOMAX) {
    count++;
  }
}

void GameWorld::init() {
  screen_width = M5Cardputer.Display.width();
  screen_height = M5Cardputer.Display.height();
  center_x = screen_width / 2;
  center_y = 72;
  objInit();

  for (int i = 0; i < app_config::PMAX; ++i) {
    plane[i].posInit();
    plane[i].no = i;
  }

  plane[0].target = 2;
  plane[1].target = 2;
  plane[2].target = 1;
  plane[3].target = 1;
  plane[0].use = true;
  plane[1].use = true;
  plane[2].use = true;
  plane[3].use = true;
  plane[0].level = 20;
  plane[1].level = 10;
  plane[2].level = 20;
  plane[3].level = 30;

  control = ControlState{};
  camerapos.set(plane[0].pVel);
  frame_counter = 0;
  chrome_visible = true;
  started = true;
  paused = false;
}

void GameWorld::objInit() {
  if (obj_initialized) {
    return;
  }
  obj_initialized = true;

  obj[0][0].set(-0.0, -2.0, 0.0); obj[0][1].set(0.0, 4.0, 0.0); obj[0][2].set(6.0, -2.0, 0.0);
  obj[1][0].set(0.0, -3.0, 1.5); obj[1][1].set(2.0, -3.0, 0.0); obj[1][2].set(0.0, 8.0, 0.0);
  obj[2][0].set(2.0, 0.0, 0.0); obj[2][1].set(3.0, 0.0, -0.5); obj[2][2].set(3.5, 0.0, 0.0);
  obj[3][0].set(3.0, 0.0, 0.0); obj[3][1].set(3.0, -1.0, -1.5); obj[3][2].set(3.0, 0.0, -2.0);
  obj[4][0].set(3.0, -1.0, -2.0); obj[4][1].set(3.0, 2.0, -2.0); obj[4][2].set(3.5, 1.0, -2.5);
  obj[5][0].set(1.0, 0.0, -6.0); obj[5][1].set(2.0, 4.0, -6.0); obj[5][2].set(2.0, -2.0, 0.0);
  obj[6][0].set(3.0, 0.0, -6.0); obj[6][1].set(2.0, 4.0, -6.0); obj[6][2].set(2.0, -2.0, 0.0);
  obj[7][0].set(2.0, 1.0, 0.0); obj[7][1].set(2.0, -3.0, 4.0); obj[7][2].set(2.0, -3.0, -2.0);
  obj[8][0].set(1.0, 0.0, 0.0); obj[8][1].set(0.0, 0.0, -1.0); obj[8][2].set(0.0, 1.0, 0.0);
  obj[9][0].set(0.0, -2.0, 0.0); obj[9][1].set(0.0, 4.0, 0.0); obj[9][2].set(-6.0, -2.0, 0.0);
  obj[10][0].set(0.0, -3.0, 1.5); obj[10][1].set(-2.0, -3.0, 0.0); obj[10][2].set(0.0, 8.0, 0.0);
  obj[11][0].set(-2.0, 0.0, 0.0); obj[11][1].set(-3.0, 0.0, -0.5); obj[11][2].set(-3.5, 0.0, 0.0);
  obj[12][0].set(-3.0, 0.0, 0.0); obj[12][1].set(-3.0, -1.0, -1.5); obj[12][2].set(-3.0, 0.0, -2.0);
  obj[13][0].set(-3.0, -1.0, -2.0); obj[13][1].set(-3.0, 2.0, -2.0); obj[13][2].set(-3.5, 1.0, -2.5);
  obj[14][0].set(-1.0, 0.0, -6.0); obj[14][1].set(-2.0, 4.0, -6.0); obj[14][2].set(-2.0, -2.0, 0.0);
  obj[15][0].set(-3.0, 0.0, -6.0); obj[15][1].set(-2.0, 4.0, -6.0); obj[15][2].set(-2.0, -2.0, 0.0);
  obj[16][0].set(-2.0, 1.0, 0.0); obj[16][1].set(-2.0, -3.0, 4.0); obj[16][2].set(-2.0, -3.0, -2.0);
  obj[17][0].set(-1.0, 0.0, 0.0); obj[17][1].set(0.0, 0.0, -1.0); obj[17][2].set(0.0, 1.0, 0.0);
  obj[18][0].set(3.0, 0.0, -2.0); obj[18][1].set(3.0, 0.0, -1.5); obj[18][2].set(3.0, 7.0, -2.0);
}

void GameWorld::change3d(const Plane& plane_ref, const Vec3& sp, Vec3& cp) const {
  const double x = sp.x - camerapos.x;
  const double y = sp.y - camerapos.y;
  const double z = sp.z - camerapos.z;

  const double x1 = x * plane_ref.y00 + y * plane_ref.y01 + z * plane_ref.y02;
  const double y1 = x * plane_ref.y10 + y * plane_ref.y11 + z * plane_ref.y12;
  const double z1 = x * plane_ref.y20 + y * plane_ref.y21 + z * plane_ref.y22;

  if (y1 > 10.0) {
    const double perspective = app_config::CAMERA_SCALE / (y1 / 10.0);
    cp.x = x1 * perspective + center_x;
    cp.y = -z1 * perspective + center_y;
    cp.z = y1 * 10.0;
  } else {
    cp.x = -10000.0;
    cp.y = -10000.0;
    cp.z = 1.0;
  }
}

double GameWorld::gHeight(double, double) const {
  return 0.0;
}

void GameWorld::gGrad(double, double, Vec3& p) const {
  p.x = 0.0;
  p.y = 0.0;
  p.z = 0.0;
}

void GameWorld::update() {
  if (!started || paused) {
    return;
  }

  if (control.shoot || control.left || control.right || control.up || control.down || control.boost) {
    auto_flight = false;
  }

  plane[0].move(*this, auto_flight);
  for (int i = 1; i < app_config::PMAX; ++i) {
    plane[i].move(*this, true);
  }
  camerapos.set(plane[0].pVel);
  frame_counter++;
}

void GameWorld::drawSline(M5Canvas& canvas, const Vec3& p0, const Vec3& p1, uint16_t color) {
  if (p0.x > -10000.0 && p0.x < 30000.0 && p0.y > -10000.0 && p0.y < 30000.0 &&
      p1.x > -10000.0 && p1.x < 30000.0 && p1.y > -10000.0 && p1.y < 30000.0) {
    canvas.drawLine(static_cast<int16_t>(p0.x), static_cast<int16_t>(p0.y),
                    static_cast<int16_t>(p1.x), static_cast<int16_t>(p1.y), color);
  }
}

void GameWorld::drawBlined(M5Canvas& canvas, const Vec3& p0, const Vec3& p1) {
  if (p0.x > -1000.0 && p1.x > -1000.0) {
    drawSline(canvas, p0, p1, TFT_YELLOW);
  }
}

void GameWorld::drawBline(M5Canvas& canvas, const Vec3& p0, const Vec3& p1) {
  if (p0.x > -1000.0 && p1.x > -1000.0) {
    drawSline(canvas, p0, p1, TFT_YELLOW);
    Vec3 a(p0.x + 1.0, p0.y, 0.0);
    Vec3 b(p1.x + 1.0, p1.y, 0.0);
    drawSline(canvas, a, b, TFT_YELLOW);
  }
}

static uint16_t gray565(uint8_t level) {
  const uint16_t r = static_cast<uint16_t>((static_cast<uint32_t>(level) * 31U + 127U) / 255U);
  const uint16_t g = static_cast<uint16_t>((static_cast<uint32_t>(level) * 63U + 127U) / 255U);
  const uint16_t b = static_cast<uint16_t>((static_cast<uint32_t>(level) * 31U + 127U) / 255U);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

void GameWorld::drawMline(M5Canvas& canvas, const Vec3& p0, const Vec3& p1, uint16_t color) {
  if (p0.x > -1000.0 && p1.x > -1000.0) {
    drawSline(canvas, p0, p1, color);
  }
}

void GameWorld::drawAline(M5Canvas& canvas, const Vec3& p0, const Vec3& p1) {
  if (p0.x > -1000.0 && p1.x > -1000.0) {
    drawSline(canvas, p0, p1, TFT_WHITE);
    Vec3 a(p0.x + 1.0, p0.y, 0.0);
    Vec3 b(p1.x + 1.0, p1.y, 0.0);
    drawSline(canvas, a, b, TFT_WHITE);
  }
}

void GameWorld::drawPoly(
    M5Canvas& canvas, const Vec3& p0, const Vec3& p1, const Vec3& p2, uint16_t color) {
  drawSline(canvas, p0, p1, color);
  drawSline(canvas, p1, p2, color);
  drawSline(canvas, p2, p0, color);
}

void GameWorld::fillBarc(M5Canvas& canvas, const Vec3& p) {
  if (p.x >= -100.0) {
    int rr = static_cast<int>(2000.0 / p.z) + 2;
    if (rr > 40) {
      rr = 40;
    }
    canvas.fillCircle(static_cast<int16_t>(p.x), static_cast<int16_t>(p.y), rr / 2, TFT_ORANGE);
  }
}

void GameWorld::writeGround(M5Canvas& canvas) {
  Vec3 p;
  const double step = app_config::FMAX * 2.0 / app_config::GSCALE;
  const int dx = static_cast<int>(plane[0].pVel.x / step);
  const int dy = static_cast<int>(plane[0].pVel.y / step);
  const double sx = dx * step;
  const double sy = dy * step;

  double my = -app_config::FMAX;
  for (int j = 0; j < app_config::GSCALE; ++j) {
    double mx = -app_config::FMAX;
    for (int i = 0; i < app_config::GSCALE; ++i) {
      p.x = mx + sx;
      p.y = my + sy;
      p.z = gHeight(mx + sx, my + sy);
      change3d(plane[0], p, ground_pos[j][i]);
      mx += step;
    }
    my += step;
  }

  for (int j = 0; j < app_config::GSCALE; ++j) {
    for (int i = 0; i < app_config::GSCALE - 1; ++i) {
      drawSline(canvas, ground_pos[j][i], ground_pos[j][i + 1], TFT_DARKGREEN);
    }
  }
  for (int i = 0; i < app_config::GSCALE; ++i) {
    for (int j = 0; j < app_config::GSCALE - 1; ++j) {
      drawSline(canvas, ground_pos[j][i], ground_pos[j + 1][i], TFT_DARKGREEN);
    }
  }
}

void GameWorld::writeGun(M5Canvas& canvas, Plane& aplane) {
  Vec3 dm;
  Vec3 dm2;
  Vec3 cp;

  for (int j = 0; j < Plane::BMAX; ++j) {
    Bullet& bp = aplane.bullet[j];
    if (bp.use > 0) {
      dm.x = bp.pVel.x + bp.vVel.x * 0.005;
      dm.y = bp.pVel.y + bp.vVel.y * 0.005;
      dm.z = bp.pVel.z + bp.vVel.z * 0.005;
      change3d(plane[0], dm, cp);
      dm.x = bp.pVel.x + bp.vVel.x * 0.04;
      dm.y = bp.pVel.y + bp.vVel.y * 0.04;
      dm.z = bp.pVel.z + bp.vVel.z * 0.04;
      change3d(plane[0], dm, dm2);
      drawBline(canvas, cp, dm2);

      change3d(plane[0], bp.pVel, cp);
      dm.x = bp.pVel.x + bp.vVel.x * 0.05;
      dm.y = bp.pVel.y + bp.vVel.y * 0.05;
      dm.z = bp.pVel.z + bp.vVel.z * 0.05;
      change3d(plane[0], dm, dm2);
      drawBlined(canvas, cp, dm2);
    }

    if (bp.bom > 0) {
      change3d(plane[0], bp.opVel, cp);
      fillBarc(canvas, cp);
      bp.bom--;
    }
  }
}

void GameWorld::writeAam(M5Canvas& canvas, Plane& aplane) {
  Vec3 dm;
  Vec3 cp;

  for (int j = 0; j < Plane::MMMAX; ++j) {
    Missile& ap = aplane.aam[j];
    if (ap.use >= 0) {
      if (ap.bom <= 0) {
        dm.x = ap.pVel.x + ap.aVel.x * 4.0;
        dm.y = ap.pVel.y + ap.aVel.y * 4.0;
        dm.z = ap.pVel.z + ap.aVel.z * 4.0;
        change3d(plane[0], dm, cp);
        change3d(plane[0], ap.pVel, dm);
        drawAline(canvas, cp, dm);
      }

      int k = (ap.use + Missile::MOMAX + 1) % Missile::MOMAX;
      change3d(plane[0], ap.opVel[k], dm);
      for (int m = 0; m < ap.count; ++m) {
        change3d(plane[0], ap.opVel[k], cp);
        const float t = ap.count > 1 ? static_cast<float>(m) / static_cast<float>(ap.count - 1) : 0.0f;
        const uint8_t gray = static_cast<uint8_t>(255.0f - t * 175.0f);
        drawMline(canvas, dm, cp, gray565(gray));
        k = (k + Missile::MOMAX + 1) % Missile::MOMAX;
        dm.set(cp);
      }
    }

    if (ap.bom > 0) {
      change3d(plane[0], ap.pVel, cp);
      fillBarc(canvas, cp);
    }
  }
}

void GameWorld::writePlane(M5Canvas& canvas) {
  Vec3 p0;
  Vec3 p1;
  Vec3 p2;
  Vec3 s0;
  Vec3 s1;
  Vec3 s2;

  for (int i = 0; i < app_config::PMAX; ++i) {
    if (!plane[i].use) {
      continue;
    }

    writeGun(canvas, plane[i]);
    writeAam(canvas, plane[i]);
    plane[0].checkTransM(plane[i].aVel);

    if (i != 0) {
      for (int j = 0; j < 19; ++j) {
        plane[0].change_ml2w(obj[j][0], p0);
        plane[0].change_ml2w(obj[j][1], p1);
        plane[0].change_ml2w(obj[j][2], p2);
        p0.add(plane[i].pVel);
        p1.add(plane[i].pVel);
        p2.add(plane[i].pVel);
        change3d(plane[0], p0, s0);
        change3d(plane[0], p1, s1);
        change3d(plane[0], p2, s2);
        drawPoly(canvas, s0, s1, s2, TFT_OLIVE);
      }
    }
  }
}

void draw_battery_status(M5Canvas& canvas, int16_t width) {
  const int battery_level = M5Cardputer.Power.getBatteryLevel();
  const int capped_level = battery_level < 0 ? 0 : (battery_level > 100 ? 100 : battery_level);
  const int icon_x = width - 22;
  const int icon_y = 2;
  const int icon_w = 14;
  const int icon_h = 7;
  const int fill_w = (icon_w - 4) * capped_level / 100;
  const uint16_t fill_color = capped_level > 20 ? TFT_GREENYELLOW : TFT_ORANGE;

  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(width - 50, 0);
  canvas.printf("%3d%%", capped_level);
  canvas.drawRect(icon_x, icon_y, icon_w, icon_h, TFT_WHITE);
  canvas.fillRect(icon_x + icon_w, icon_y + 2, 2, icon_h - 4, TFT_WHITE);
  if (fill_w > 0) {
    canvas.fillRect(icon_x + 2, icon_y + 2, fill_w, icon_h - 4, fill_color);
  }
}

void GameWorld::clear(M5Canvas& canvas) {
  canvas.fillScreen(TFT_BLACK);
}

void draw_hud_panel(M5Canvas& canvas, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  canvas.drawRoundRect(x, y, w, h, 3, color);
  canvas.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 3, TFT_DARKGREY);
}

void draw_vertical_meter(
    M5Canvas& canvas,
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    const char* label,
    int value,
    int max_value,
    uint16_t accent_color) {
  draw_hud_panel(canvas, x, y, w, h, accent_color);
  canvas.setTextColor(accent_color, TFT_BLACK);
  canvas.setCursor(x + 4, y + 3);
  canvas.print(label);

  const int16_t meter_x = x + 4;
  const int16_t meter_y = y + 13;
  const int16_t meter_w = 8;
  const int16_t meter_h = h - 18;
  canvas.drawRect(meter_x, meter_y, meter_w, meter_h, TFT_DARKGREY);

  int clamped_value = value;
  if (clamped_value < 0) {
    clamped_value = 0;
  }
  if (clamped_value > max_value) {
    clamped_value = max_value;
  }

  const int16_t fill_h =
      static_cast<int16_t>((static_cast<float>(clamped_value) / max_value) * (meter_h - 2));
  if (fill_h > 0) {
    canvas.fillRect(meter_x + 1, meter_y + meter_h - 1 - fill_h, meter_w - 2, fill_h, accent_color);
  }

  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(x + 16, y + 20);
  canvas.printf("%4d", value);
}

void draw_mode_banner(M5Canvas& canvas, bool auto_flight) {
  const char* mode_text = auto_flight ? "AUTO" : "MANUAL";
  const int16_t banner_w = canvas.textWidth(mode_text) + 10;
  const int16_t banner_h = 14;
  const int16_t banner_x = (canvas.width() - banner_w) / 2;
  const int16_t banner_y = 2;
  const uint16_t mode_color = auto_flight ? TFT_YELLOW : TFT_CYAN;
  draw_hud_panel(canvas, banner_x, banner_y, banner_w, banner_h, mode_color);
  canvas.setTextColor(mode_color, TFT_BLACK);
  canvas.setCursor(banner_x + (banner_w - canvas.textWidth(mode_text)) / 2, banner_y + 3);
  canvas.print(mode_text);
}

constexpr int kUiMenuItemCount = 12;
constexpr int kUiMenuVisibleRows = 9;

const char* ui_menu_label(int index) {
  switch (index) {
    case 0:
      return "Attitude HUD";
    case 1:
      return "Reticle";
    case 2:
      return "Lock Box";
    case 3:
      return "Enemy Arrows";
    case 4:
      return "Speed Tape";
    case 5:
      return "Altitude Tape";
    case 6:
      return "TGT Panel";
    case 7:
      return "AAM Ammo";
    case 8:
      return "Gun Heat";
    case 9:
      return "Header";
    case 10:
      return "Mode Banner";
    case 11:
      return "Footer";
    default:
      return "";
  }
}

bool ui_menu_value(const GameWorld& world, int index) {
  switch (index) {
    case 0:
      return world.ui_attitude_visible;
    case 1:
      return world.ui_reticle_visible;
    case 2:
      return world.ui_lock_box_visible;
    case 3:
      return world.ui_enemy_arrows_visible;
    case 4:
      return world.ui_speed_visible;
    case 5:
      return world.ui_altitude_visible;
    case 6:
      return world.ui_tgt_visible;
    case 7:
      return world.ui_aam_ammo_visible;
    case 8:
      return world.ui_gun_heat_visible;
    case 9:
      return world.ui_header_visible;
    case 10:
      return world.ui_mode_banner_visible;
    case 11:
      return world.ui_footer_visible;
    default:
      return false;
  }
}

void toggle_ui_menu_value(GameWorld& world, int index) {
  switch (index) {
    case 0:
      world.ui_attitude_visible = !world.ui_attitude_visible;
      break;
    case 1:
      world.ui_reticle_visible = !world.ui_reticle_visible;
      break;
    case 2:
      world.ui_lock_box_visible = !world.ui_lock_box_visible;
      break;
    case 3:
      world.ui_enemy_arrows_visible = !world.ui_enemy_arrows_visible;
      break;
    case 4:
      world.ui_speed_visible = !world.ui_speed_visible;
      break;
    case 5:
      world.ui_altitude_visible = !world.ui_altitude_visible;
      break;
    case 6:
      world.ui_tgt_visible = !world.ui_tgt_visible;
      break;
    case 7:
      world.ui_aam_ammo_visible = !world.ui_aam_ammo_visible;
      break;
    case 8:
      world.ui_gun_heat_visible = !world.ui_gun_heat_visible;
      break;
    case 9:
      world.ui_header_visible = !world.ui_header_visible;
      break;
    case 10:
      world.ui_mode_banner_visible = !world.ui_mode_banner_visible;
      break;
    case 11:
      world.ui_footer_visible = !world.ui_footer_visible;
      break;
  }
}

void draw_popup_menu(M5Canvas& canvas, const GameWorld& world) {
  if (!world.menu_visible) {
    return;
  }

  const int16_t x = 4;
  const int16_t y = 10;
  const int16_t w = 126;
  const int16_t h = 112;
  canvas.fillRoundRect(x, y, w, h, 4, TFT_BLACK);
  canvas.drawRoundRect(x, y, w, h, 4, TFT_DARKGREY);
  canvas.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 4, TFT_ORANGE);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.setCursor(x + 6, y + 4);
  canvas.print("UI MENU");

  const int visible_start = world.menu_scroll;
  const int visible_end = min(kUiMenuItemCount, visible_start + kUiMenuVisibleRows);
  for (int i = visible_start; i < visible_end; ++i) {
    const int16_t row_y = y + 16 + (i - visible_start) * 10;
    const bool selected = i == world.menu_index;
    if (selected) {
      canvas.fillRect(x + 3, row_y - 1, w - 14, 9, TFT_ORANGE);
    }
    canvas.setTextColor(selected ? TFT_BLACK : TFT_WHITE, TFT_BLACK);
    canvas.setCursor(x + 6, row_y);
    canvas.print(ui_menu_label(i));
    canvas.setCursor(x + 96, row_y);
    canvas.print(ui_menu_value(world, i) ? "ON" : "OFF");
  }

  if (kUiMenuItemCount > kUiMenuVisibleRows) {
    const int16_t track_x = x + w - 8;
    const int16_t track_y = y + 16;
    const int16_t track_h = kUiMenuVisibleRows * 10 - 2;
    canvas.drawRect(track_x, track_y, 4, track_h, TFT_DARKGREY);

    const float ratio = static_cast<float>(kUiMenuVisibleRows) / static_cast<float>(kUiMenuItemCount);
    int16_t thumb_h = static_cast<int16_t>(track_h * ratio);
    if (thumb_h < 8) {
      thumb_h = 8;
    }
    const int scroll_range = kUiMenuItemCount - kUiMenuVisibleRows;
    int thumb_range = track_h - thumb_h - 2;
    if (thumb_range < 1) {
      thumb_range = 1;
    }
    const int16_t thumb_y = track_y + 1 +
                            static_cast<int16_t>((static_cast<float>(world.menu_scroll) / scroll_range) * thumb_range);
    canvas.fillRect(track_x + 1, thumb_y, 2, thumb_h, TFT_ORANGE);
  }
}

void draw_hud_tape(
    M5Canvas& canvas,
    int16_t center_x,
    int16_t center_y,
    int value,
    int step,
    int major_step,
    int range,
    bool left_side,
    const char* label,
    uint16_t color) {
  const int16_t tape_h = 58;
  const int16_t top_y = center_y - tape_h / 2;
  const int16_t box_w = 28;
  const int16_t box_h = 12;
  const int16_t box_x = left_side ? center_x - 34 : center_x + 6;
  const int16_t tick_outer_x = left_side ? box_x + box_w + 7 : box_x - 8;
  const int16_t tick_inner_x = left_side ? tick_outer_x - 8 : tick_outer_x + 8;

  canvas.setTextColor(color);
  canvas.setCursor(box_x + 2, top_y - 8);
  canvas.print(label);

  canvas.drawRect(box_x, center_y - box_h / 2, box_w, box_h, color);
  canvas.setCursor(box_x + 3, center_y - 3);
  canvas.printf("%3d", value);

  const float pixels_per_step = 8.0f;
  const int first_tick = static_cast<int>(floor(static_cast<float>(value - range) / step)) * step;
  const int last_tick = static_cast<int>(ceil(static_cast<float>(value + range) / step)) * step;

  for (int tick_value = first_tick; tick_value <= last_tick; tick_value += step) {
    const float offset_steps = static_cast<float>(tick_value - value) / step;
    const int16_t y = center_y + static_cast<int16_t>(offset_steps * pixels_per_step);
    if (y < top_y || y > top_y + tape_h) {
      continue;
    }

    const bool major = (tick_value % major_step) == 0;
    const int16_t tick_len = major ? 10 : 4;
    const int16_t x0 = left_side ? tick_outer_x : tick_outer_x;
    const int16_t x1 = left_side ? tick_outer_x - tick_len : tick_outer_x + tick_len;
    canvas.drawLine(x0, y, x1, y, color);
  }

  if (left_side) {
    canvas.drawLine(box_x + box_w, center_y, tick_inner_x, center_y, color);
    canvas.drawLine(tick_inner_x, center_y, tick_inner_x, center_y - 10, color);
    canvas.drawLine(tick_inner_x, center_y, tick_inner_x, center_y + 10, color);
  } else {
    canvas.drawLine(box_x, center_y, tick_inner_x, center_y, color);
    canvas.drawLine(tick_inner_x, center_y, tick_inner_x, center_y - 10, color);
    canvas.drawLine(tick_inner_x, center_y, tick_inner_x, center_y + 10, color);
  }
}

void rotate_around_origin(float x, float y, float angle_rad, int16_t& out_x, int16_t& out_y) {
  const float cos_a = cosf(angle_rad);
  const float sin_a = sinf(angle_rad);
  out_x = static_cast<int16_t>(x * cos_a - y * sin_a);
  out_y = static_cast<int16_t>(x * sin_a + y * cos_a);
}

void draw_pitch_ladder(M5Canvas& canvas, int16_t cx, int16_t cy, float pitch_deg, float roll_deg) {
  const float roll_rad = roll_deg * DEG_TO_RAD;
  constexpr float pixels_per_deg = 1.5f;

  for (int mark = -90; mark <= 90; mark += 10) {
    const float y_offset = (pitch_deg - static_cast<float>(mark)) * pixels_per_deg;
    if (fabsf(y_offset) > 30.0f) {
      continue;
    }

    const bool horizon = mark == 0;
    const int16_t half_width = horizon ? 20 : 12;
    const int16_t gap = horizon ? 0 : 4;
    const uint16_t color = horizon ? TFT_GREENYELLOW : TFT_DARKGREY;

    int16_t rx0;
    int16_t ry0;
    int16_t rx1;
    int16_t ry1;

    rotate_around_origin(-half_width, y_offset, roll_rad, rx0, ry0);
    rotate_around_origin(-gap, y_offset, roll_rad, rx1, ry1);
    canvas.drawLine(cx + rx0, cy + ry0, cx + rx1, cy + ry1, color);

    rotate_around_origin(gap, y_offset, roll_rad, rx0, ry0);
    rotate_around_origin(half_width, y_offset, roll_rad, rx1, ry1);
    canvas.drawLine(cx + rx0, cy + ry0, cx + rx1, cy + ry1, color);

    if (!horizon) {
      int16_t tx;
      int16_t ty;
      rotate_around_origin(-half_width - 10, y_offset - 3, roll_rad, tx, ty);
      canvas.setTextColor(TFT_DARKGREY);
      canvas.setCursor(cx + tx, cy + ty);
      canvas.printf("%d", -mark);
    }
  }
}

void draw_attitude_hud(M5Canvas& canvas, const Plane& player) {
  const int16_t cx = app_config::SCREEN_W / 2;
  const int16_t cy = 70;
  const float pitch_deg = static_cast<float>(player.aVel.x * RAD_TO_DEG);
  const float roll_deg = static_cast<float>(player.aVel.y * RAD_TO_DEG);

  draw_pitch_ladder(canvas, cx, cy, pitch_deg, roll_deg);
}

void draw_enemy_direction_arrows(M5Canvas& canvas, const GameWorld& world) {
  const Plane& player = world.plane[0];
  const int16_t cx = app_config::SCREEN_W / 2;
  const int16_t cy = 70;
  const float radius = 44.0f;

  for (int i = 1; i < app_config::PMAX; ++i) {
    if (!world.plane[i].use) {
      continue;
    }

    Vec3 screen;
    Vec3 rel;
    Vec3 local;
    world.change3d(player, world.plane[i].pVel, screen);
    rel.setMinus(world.plane[i].pVel, player.pVel);
    player.change_w2l(rel, local);

    if (screen.x >= 0.0 && screen.x < canvas.width() && screen.y >= 0.0 && screen.y < canvas.height()) {
      continue;
    }

    float dir_x = static_cast<float>(local.x);
    float dir_y = static_cast<float>(-local.z);
    if (local.y < 0.0) {
      dir_x = -dir_x;
      dir_y = -dir_y;
    }

    const float len = sqrtf(dir_x * dir_x + dir_y * dir_y);
    if (len < 1.0f) {
      continue;
    }

    dir_x /= len;
    dir_y /= len;

    const int16_t tip_x = static_cast<int16_t>(cx + dir_x * radius);
    const int16_t tip_y = static_cast<int16_t>(cy + dir_y * radius);
    const int16_t base_x = static_cast<int16_t>(cx + dir_x * (radius - 8.0f));
    const int16_t base_y = static_cast<int16_t>(cy + dir_y * (radius - 8.0f));
    const int16_t left_x = static_cast<int16_t>(base_x - dir_y * 4.0f);
    const int16_t left_y = static_cast<int16_t>(base_y + dir_x * 4.0f);
    const int16_t right_x = static_cast<int16_t>(base_x + dir_y * 4.0f);
    const int16_t right_y = static_cast<int16_t>(base_y - dir_x * 4.0f);

    canvas.fillTriangle(tip_x, tip_y, left_x, left_y, right_x, right_y, TFT_ORANGE);
  }
}

void draw_reticle(
    M5Canvas& canvas, const Plane& player, bool auto_flight, bool show_reticle, bool show_lock_box) {
  const int cx = app_config::SCREEN_W / 2;
  const int cy = 70;
  if (show_reticle) {
    canvas.drawFastHLine(cx - 6, cy, 12, TFT_DARKGREY);
    canvas.drawFastVLine(cx, cy - 6, 12, TFT_DARKGREY);

    const int gun_x = cx + static_cast<int>(player.gunX * 0.36);
    const int gun_y = cy - static_cast<int>((player.gunY - 20.0) * 0.36);
    const uint16_t reticle_color = auto_flight ? TFT_YELLOW : TFT_CYAN;
    canvas.drawCircle(gun_x, gun_y, app_config::RETICLE_RADIUS, reticle_color);
    canvas.drawFastHLine(gun_x - 14, gun_y, 28, reticle_color);
    canvas.drawFastVLine(gun_x, gun_y - 14, 28, reticle_color);
    canvas.drawCircle(gun_x, gun_y, 2, reticle_color);
  }

  if (show_lock_box && player.targetSx > -1000) {
    canvas.drawRect(player.targetSx - 6, player.targetSy - 6, 12, 12, TFT_RED);
  }
}

int count_remaining_missiles(const Plane& player) {
  int remaining = 0;
  for (int i = 0; i < Plane::MMMAX; ++i) {
    if (player.aam[i].use < 0) {
      ++remaining;
    }
  }
  return remaining;
}

void draw_missile_ammo_hud(M5Canvas& canvas, const Plane& player) {
  const int remaining_missiles = count_remaining_missiles(player);
  char ammo_text[12];
  snprintf(ammo_text, sizeof(ammo_text), "%02d/%02d", remaining_missiles, Plane::MMMAX);
  canvas.setTextFont(0);
  canvas.setTextSize(1);
  canvas.setTextColor(remaining_missiles == 0 ? TFT_RED : TFT_CYAN);
  if constexpr (app_config::SHOW_AAM_LABEL) {
    canvas.setCursor(canvas.width() / 2 - canvas.textWidth("AAM") / 2, canvas.height() - 30);
    canvas.print("AAM");
    canvas.setCursor(canvas.width() / 2 - canvas.textWidth(ammo_text) / 2, canvas.height() - 20);
    canvas.print(ammo_text);
  } else {
    canvas.setCursor(canvas.width() / 2 - canvas.textWidth(ammo_text) / 2, canvas.height() - 30);
    canvas.print(ammo_text);
  }
  canvas.setTextFont(1);
}

void draw_gun_heat_bar(M5Canvas& canvas, const Plane& player) {
  const char* gun_text = "GUN";
  const int16_t bar_w = 32;
  const int16_t bar_h = 3;
  const int16_t bar_x = canvas.width() / 2 - bar_w / 2;
  const int16_t bar_y = canvas.height() - 35;
  int16_t fill_w = static_cast<int16_t>(
      (static_cast<float>(player.gunTemp) / static_cast<float>(Plane::MAXT)) * static_cast<float>(bar_w - 2));
  if (fill_w < 0) {
    fill_w = 0;
  }
  if (fill_w > bar_w - 2) {
    fill_w = bar_w - 2;
  }

  const float heat_ratio = static_cast<float>(player.gunTemp) / static_cast<float>(Plane::MAXT);
  uint16_t heat_color = TFT_RED;
  if (player.heatWait) {
    heat_color = TFT_RED;
  } else if (heat_ratio < 0.4f) {
    heat_color = TFT_CYAN;
  } else if (heat_ratio < 0.7f) {
    heat_color = TFT_GREENYELLOW;
  } else if (heat_ratio < 0.9f) {
    heat_color = TFT_ORANGE;
  }

  canvas.fillRect(bar_x, bar_y, bar_w, bar_h, TFT_BLACK);
  canvas.drawRect(bar_x, bar_y, bar_w, bar_h, heat_color);
  if (fill_w > 0) {
    canvas.fillRect(bar_x + 1, bar_y + 1, fill_w, max<int16_t>(1, bar_h - 1), heat_color);
  }

  if constexpr (app_config::SHOW_GUN_HEAT_LABEL) {
    canvas.setTextFont(0);
    canvas.setTextSize(1);
    canvas.setTextColor(heat_color);
    canvas.setCursor(canvas.width() / 2 - canvas.textWidth(gun_text) / 2, bar_y - 10);
    canvas.print(gun_text);
    canvas.setTextFont(1);
  }
}

void draw_hud(M5Canvas& canvas, const GameWorld& world) {
  const Plane& player = world.plane[0];
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  if (world.chrome_visible && world.ui_mode_banner_visible) {
    draw_mode_banner(canvas, world.auto_flight);
  }

  if (world.chrome_visible && world.ui_header_visible) {
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(0, 0);
    canvas.print("NekoFlight ADV");
    draw_battery_status(canvas, canvas.width());
  }

  if (world.ui_speed_visible) {
    draw_hud_tape(
        canvas,
        canvas.width() / 2 - 26,
        70,
        static_cast<int>(player.vpVel.abs()),
        10,
        50,
        40,
        true,
        "SPD",
        TFT_CYAN);
  }
  if (world.ui_altitude_visible) {
    draw_hud_tape(
        canvas,
        canvas.width() / 2 + 26,
        70,
        static_cast<int>(player.height),
        100,
        500,
        400,
        false,
        "ALT",
        TFT_GREENYELLOW);
  }

  if (world.ui_tgt_visible && player.targetDis > 0.0) {
    const int16_t panel_x = canvas.width() - 60;
    const int16_t panel_y = 22;
    const int16_t panel_w = 36;
    const int16_t panel_h = 21;
    draw_hud_panel(canvas, panel_x, panel_y, panel_w, panel_h, TFT_ORANGE);
    canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
    canvas.setCursor(panel_x + 4, panel_y + 3);
    canvas.print("TGT");
    canvas.setCursor(panel_x + 4, panel_y + 11);
    canvas.printf("%4d", static_cast<int>(player.targetDis));
  }

  if (world.ui_aam_ammo_visible) {
    draw_missile_ammo_hud(canvas, player);
  }
  if (world.ui_gun_heat_visible) {
    draw_gun_heat_bar(canvas, player);
  }

  if (world.chrome_visible && world.ui_footer_visible) {
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(0, app_config::FOOTER_Y);
    canvas.print("A:Fire S:Boost R:Reset Ent:Auto fn:UI");
  }
}

void GameWorld::draw(M5Canvas& canvas) {
  clear(canvas);
  plane[0].checkTrans();
  writeGround(canvas);
  writePlane(canvas);
  if (ui_attitude_visible) {
    draw_attitude_hud(canvas, plane[0]);
  }
  if (ui_enemy_arrows_visible) {
    draw_enemy_direction_arrows(canvas, *this);
  }
  if (ui_reticle_visible || ui_lock_box_visible) {
    draw_reticle(canvas, plane[0], auto_flight, ui_reticle_visible, ui_lock_box_visible);
  }
  draw_hud(canvas, *this);
  draw_popup_menu(canvas, *this);
}

void update_controls() {
  const auto status = M5Cardputer.Keyboard.keysState();
  const bool up = contains_char_key(status, ';');
  const bool down = contains_char_key(status, '.');
  const bool left = contains_char_key(status, ',');
  const bool right = contains_char_key(status, '/');
  const bool shoot = contains_char_key(status, 'a') || contains_char_key(status, 'A');
  const bool boost = contains_char_key(status, 's') || contains_char_key(status, 'S');
  const bool reset = contains_char_key(status, 'r') || contains_char_key(status, 'R');
  const bool toggle_chrome = contains_char_key(status, '`') || contains_char_key(status, '~');
  const bool toggle_menu = status.fn;
  const bool toggle_auto = status.enter;

  if (toggle_menu && !g_prev_fn) {
    g_world.menu_visible = !g_world.menu_visible;
    g_needs_redraw = true;
  }
  g_prev_fn = toggle_menu;

  const bool menu_active = g_world.menu_visible;
  g_world.control.up = menu_active ? false : up;
  g_world.control.down = menu_active ? false : down;
  g_world.control.left = menu_active ? false : left;
  g_world.control.right = menu_active ? false : right;
  g_world.control.shoot = menu_active ? false : shoot;
  g_world.control.boost = menu_active ? false : boost;

  if (menu_active) {
    if (up && !g_prev_menu_up) {
      g_world.menu_index = (g_world.menu_index + kUiMenuItemCount - 1) % kUiMenuItemCount;
      if (g_world.menu_index < g_world.menu_scroll) {
        g_world.menu_scroll = g_world.menu_index;
      } else if (g_world.menu_index >= g_world.menu_scroll + kUiMenuVisibleRows) {
        g_world.menu_scroll = g_world.menu_index - kUiMenuVisibleRows + 1;
      }
      g_needs_redraw = true;
    }
    if (down && !g_prev_menu_down) {
      g_world.menu_index = (g_world.menu_index + 1) % kUiMenuItemCount;
      if (g_world.menu_index < g_world.menu_scroll) {
        g_world.menu_scroll = g_world.menu_index;
      } else if (g_world.menu_index >= g_world.menu_scroll + kUiMenuVisibleRows) {
        g_world.menu_scroll = g_world.menu_index - kUiMenuVisibleRows + 1;
      }
      g_needs_redraw = true;
    }
  } else if (reset && !g_prev_reset) {
    reset_stage_preserve_ui(g_world);
  }
  g_prev_reset = reset;
  g_prev_menu_up = up;
  g_prev_menu_down = down;

  if (toggle_chrome && !g_prev_esc) {
    g_world.chrome_visible = !g_world.chrome_visible;
    g_needs_redraw = true;
  }
  g_prev_esc = toggle_chrome;

  if (toggle_auto && !g_prev_enter) {
    if (menu_active) {
      toggle_ui_menu_value(g_world, g_world.menu_index);
    } else {
      g_world.auto_flight = !g_world.auto_flight;
    }
    g_needs_redraw = true;
  }
  g_prev_enter = toggle_auto;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  randomSeed(static_cast<uint32_t>(micros()));

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);

  g_canvas.setColorDepth(16);
  g_canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  g_world.init();
  g_last_frame_ms = millis();
  Serial.println("Cardputer ADV NekoFlight started");
}

void loop() {
  M5Cardputer.update();
  update_controls();

  const uint32_t now = millis();
  if (now - g_last_frame_ms >= app_config::FRAME_INTERVAL_MS) {
    g_world.update();
    g_world.draw(g_canvas);
    M5Cardputer.Display.startWrite();
    g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
    M5Cardputer.Display.endWrite();
    g_last_frame_ms = now;
    g_needs_redraw = false;
  } else if (g_needs_redraw) {
    g_world.draw(g_canvas);
    M5Cardputer.Display.startWrite();
    g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
    M5Cardputer.Display.endWrite();
    g_needs_redraw = false;
  }
}
