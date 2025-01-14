#ifndef TD_TUT_2_MAIN_H
#define TD_TUT_2_MAIN_H

#include <inttypes.h>

#include "raylib.h"
#include "preferred_size.h"

//# Declarations

#define ENEMY_MAX_PATH_COUNT 8
#define ENEMY_MAX_COUNT 400
#define ENEMY_TYPE_NONE 0
#define ENEMY_TYPE_MINION 1

#define PARTICLE_MAX_COUNT 400
#define PARTICLE_TYPE_NONE 0
#define PARTICLE_TYPE_EXPLOSION 1

typedef struct Particle
{
  uint8_t particleType;
  float spawnTime;
  float lifetime;
  Vector3 position;
  Vector3 velocity;
} Particle;

#define TOWER_MAX_COUNT 400
enum TowerType
{
  TOWER_TYPE_NONE,
  TOWER_TYPE_BASE,
  TOWER_TYPE_ARCHER,
  TOWER_TYPE_BALLISTA,
  TOWER_TYPE_CATAPULT,
  TOWER_TYPE_WALL,
  TOWER_TYPE_COUNT
};

typedef struct TowerTypeConfig
{
  float cooldown;
  float damage;
  float range;
  float areaDamageRadius;
  float projectileSpeed;
  uint8_t cost;
  uint8_t projectileType;
  uint16_t maxHealth;
} TowerTypeConfig;

typedef struct Tower
{
  int16_t x, y;
  uint8_t towerType;
  Vector2 lastTargetPosition;
  float cooldown;
  float damage;
} Tower;

typedef struct GameTime
{
  float time;
  float deltaTime;
} GameTime;

typedef struct ButtonState {
  char isSelected;
  char isDisabled;
} ButtonState;

typedef struct GUIState {
  int isBlocked;
} GUIState;

typedef enum LevelState
{
  LEVEL_STATE_NONE,
  LEVEL_STATE_BUILDING,
  LEVEL_STATE_BATTLE,
  LEVEL_STATE_WON_WAVE,
  LEVEL_STATE_LOST_WAVE,
  LEVEL_STATE_WON_LEVEL,
  LEVEL_STATE_RESET,
} LevelState;

typedef struct EnemyWave {
  uint8_t enemyType;
  uint8_t wave;
  uint16_t count;
  float interval;
  float delay;
  Vector2 spawnPosition;

  uint16_t spawned;
  float timeToSpawnNext;
} EnemyWave;

typedef struct Level
{
  int seed;
  LevelState state;
  LevelState nextState;
  Camera3D camera;
  int placementMode;

  int initialGold;
  int playerGold;

  EnemyWave waves[10];
  int currentWave;
  float waveEndTimer;
} Level;

typedef struct DeltaSrc
{
  char x, y;
} DeltaSrc;

typedef struct PathfindingMap
{
  int width, height;
  float scale;
  float *distances;
  long *towerIndex; 
  DeltaSrc *deltaSrc;
  float maxDistance;
  Matrix toMapSpace;
  Matrix toWorldSpace;
} PathfindingMap;

// when we execute the pathfinding algorithm, we need to store the active nodes
// in a queue. Each node has a position, a distance from the start, and the
// position of the node that we came from.
typedef struct PathfindingNode
{
  int16_t x, y, fromX, fromY;
  float distance;
} PathfindingNode;

typedef struct EnemyId
{
  uint16_t index;
  uint16_t generation;
} EnemyId;

typedef struct EnemyClassConfig
{
  float speed;
  float health;
  float radius;
  float maxAcceleration;
  float requiredContactTime;
  float explosionDamage;
  float explosionRange;
  float explosionPushbackPower;
  int goldValue;
} EnemyClassConfig;

typedef struct Enemy
{
  int16_t currentX, currentY;
  int16_t nextX, nextY;
  Vector2 simPosition;
  Vector2 simVelocity;
  uint16_t generation;
  float walkedDistance;
  float startMovingTime;
  float damage, futureDamage;
  float contactTime;
  uint8_t enemyType;
  uint8_t movePathCount;
  Vector2 movePath[ENEMY_MAX_PATH_COUNT];
} Enemy;

// a unit that uses sprites to be drawn
#define SPRITE_UNIT_PHASE_WEAPON_IDLE 0
#define SPRITE_UNIT_PHASE_WEAPON_COOLDOWN 1
typedef struct SpriteUnit
{
  Rectangle srcRect;
  Vector2 offset;
  int frameCount;
  float frameDuration;
  Rectangle srcWeaponIdleRect;
  Vector2 srcWeaponIdleOffset;
  Rectangle srcWeaponCooldownRect;
  Vector2 srcWeaponCooldownOffset;
} SpriteUnit;

#define PROJECTILE_MAX_COUNT 1200
#define PROJECTILE_TYPE_NONE 0
#define PROJECTILE_TYPE_ARROW 1

typedef struct Projectile
{
  uint8_t projectileType;
  float shootTime;
  float arrivalTime;
  float distance;
  float damage;
  Vector3 position;
  Vector3 target;
  Vector3 directionNormal;
  EnemyId targetEnemy;
} Projectile;

//# Function declarations
float TowerGetMaxHealth(Tower *tower);
int Button(const char *text, int x, int y, int width, int height, ButtonState *state);
int EnemyAddDamage(Enemy *enemy, float damage);

//# Enemy functions
void EnemyInit();
void EnemyDraw();
void EnemyTriggerExplode(Enemy *enemy, Tower *tower, Vector3 explosionSource);
void EnemyUpdate();
float EnemyGetCurrentMaxSpeed(Enemy *enemy);
float EnemyGetMaxHealth(Enemy *enemy);
int EnemyGetNextPosition(int16_t currentX, int16_t currentY, int16_t *nextX, int16_t *nextY);
Vector2 EnemyGetPosition(Enemy *enemy, float deltaT, Vector2 *velocity, int *waypointPassedCount);
EnemyId EnemyGetId(Enemy *enemy);
Enemy *EnemyTryResolve(EnemyId enemyId);
Enemy *EnemyTryAdd(uint8_t enemyType, int16_t currentX, int16_t currentY);
int EnemyAddDamage(Enemy *enemy, float damage);
Enemy* EnemyGetClosestToCastle(int16_t towerX, int16_t towerY, float range);
int EnemyCount();
void EnemyDrawHealthbars(Camera3D camera);

//# Tower functions
void TowerInit();
Tower *TowerGetAt(int16_t x, int16_t y);
Tower *TowerTryAdd(uint8_t towerType, int16_t x, int16_t y);
Tower *GetTowerByType(uint8_t towerType);
int GetTowerCosts(uint8_t towerType);
float TowerGetMaxHealth(Tower *tower);
void TowerDraw();
void TowerUpdate();
void TowerDrawHealthBars(Camera3D camera);
void DrawSpriteUnit(SpriteUnit unit, Vector3 position, float t, int flip, int phase);

//# Particles
void ParticleInit();
void ParticleAdd(uint8_t particleType, Vector3 position, Vector3 velocity, float lifetime);
void ParticleUpdate();
void ParticleDraw();

//# Projectiles
void ProjectileInit();
void ProjectileDraw();
void ProjectileUpdate();
Projectile *ProjectileTryAdd(uint8_t projectileType, Enemy *enemy, Vector3 position, Vector3 target, float speed, float damage);

//# Pathfinding map
void PathfindingMapInit(int width, int height, Vector3 translate, float scale);
float PathFindingGetDistance(int mapX, int mapY);
Vector2 PathFindingGetGradient(Vector3 world);
int PathFindingFromWorldToMapPosition(Vector3 worldPosition, int16_t *mapX, int16_t *mapY);
void PathFindingMapUpdate();
void PathFindingMapDraw();

//# UI
void DrawHealthBar(Camera3D camera, Vector3 position, float healthRatio, Color barColor, float healthBarWidth);

//# Level
void DrawLevelGround(Level *level);

//# variables
extern Level *currentLevel;
extern Enemy enemies[ENEMY_MAX_COUNT];
extern int enemyCount;
extern EnemyClassConfig enemyClassConfigs[];

extern GUIState guiState;
extern GameTime gameTime;
extern Tower towers[TOWER_MAX_COUNT];
extern int towerCount;

extern Texture2D palette, spriteSheet;

#endif