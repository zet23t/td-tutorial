#include "td_main.h"
#include <raymath.h>

static TowerTypeConfig towerTypeConfigs[TOWER_TYPE_COUNT] = {
    [TOWER_TYPE_BASE] = {
        .maxHealth = 10,
    },
    [TOWER_TYPE_ARCHER] = {
        .cooldown = 0.5f,
        .damage = 3.0f,
        .range = 3.0f,
        .cost = 6,
        .maxHealth = 10,
        .projectileSpeed = 4.0f,
        .projectileType = PROJECTILE_TYPE_ARROW,
    },
    [TOWER_TYPE_BALLISTA] = {
        .cooldown = 1.5f,
        .damage = 6.0f,
        .range = 6.0f,
        .cost = 9,
        .maxHealth = 10,
        .projectileSpeed = 6.0f,
        .projectileType = PROJECTILE_TYPE_ARROW,
    },
    [TOWER_TYPE_CATAPULT] = {
        .cooldown = 0.5f,
        .damage = 2.0f,
        .range = 4.0f,
        .areaDamageRadius = 1.0f,
        .cost = 10,
        .maxHealth = 10,
        .projectileSpeed = 4.0f,
        .projectileType = PROJECTILE_TYPE_ARROW,
    },
    [TOWER_TYPE_WALL] = {
        .cost = 2,
        .maxHealth = 10,
    },
};

Tower towers[TOWER_MAX_COUNT];
int towerCount = 0;

Model towerModels[TOWER_TYPE_COUNT];

// definition of our archer unit
SpriteUnit archerUnit = {
    .srcRect = {0, 0, 16, 16},
    .offset = {7, 1},
    .frameCount = 1,
    .frameDuration = 0.0f,
    .srcWeaponIdleRect = {16, 0, 6, 16},
    .srcWeaponIdleOffset = {8, 0},
    .srcWeaponCooldownRect = {22, 0, 11, 16},
    .srcWeaponCooldownOffset = {10, 0},
};

void DrawSpriteUnit(SpriteUnit unit, Vector3 position, float t, int flip, int phase)
{
  float xScale = flip ? -1.0f : 1.0f;
  Camera3D camera = currentLevel->camera;
  float size = 0.5f;
  Vector2 offset = (Vector2){ unit.offset.x / 16.0f * size, unit.offset.y / 16.0f * size * xScale };
  Vector2 scale = (Vector2){ unit.srcRect.width / 16.0f * size, unit.srcRect.height / 16.0f * size };
  // we want the sprite to face the camera, so we need to calculate the up vector
  Vector3 forward = Vector3Subtract(camera.target, camera.position);
  Vector3 up = {0, 1, 0};
  Vector3 right = Vector3CrossProduct(forward, up);
  up = Vector3Normalize(Vector3CrossProduct(right, forward));

  Rectangle srcRect = unit.srcRect;
  if (unit.frameCount > 1)
  {
    srcRect.x += (int)(t / unit.frameDuration) % unit.frameCount * srcRect.width;
  }
  if (flip)
  {
    srcRect.x += srcRect.width;
    srcRect.width = -srcRect.width;
  }
  DrawBillboardPro(camera, spriteSheet, srcRect, position, up, scale, offset, 0, WHITE);

  if (phase == SPRITE_UNIT_PHASE_WEAPON_COOLDOWN && unit.srcWeaponCooldownRect.width > 0)
  {
    offset = (Vector2){ unit.srcWeaponCooldownOffset.x / 16.0f * size, unit.srcWeaponCooldownOffset.y / 16.0f * size };
    scale = (Vector2){ unit.srcWeaponCooldownRect.width / 16.0f * size, unit.srcWeaponCooldownRect.height / 16.0f * size };
    srcRect = unit.srcWeaponCooldownRect;
    if (flip)
    {
      // position.x = flip * scale.x * 0.5f;
      srcRect.x += srcRect.width;
      srcRect.width = -srcRect.width;
      offset.x = scale.x - offset.x;
    }
    DrawBillboardPro(camera, spriteSheet, srcRect, position, up, scale, offset, 0, WHITE);
  }
  else if (phase == SPRITE_UNIT_PHASE_WEAPON_IDLE && unit.srcWeaponIdleRect.width > 0)
  {
    offset = (Vector2){ unit.srcWeaponIdleOffset.x / 16.0f * size, unit.srcWeaponIdleOffset.y / 16.0f * size };
    scale = (Vector2){ unit.srcWeaponIdleRect.width / 16.0f * size, unit.srcWeaponIdleRect.height / 16.0f * size };
    srcRect = unit.srcWeaponIdleRect;
    if (flip)
    {
      // position.x = flip * scale.x * 0.5f;
      srcRect.x += srcRect.width;
      srcRect.width = -srcRect.width;
      offset.x = scale.x - offset.x;
    }
    DrawBillboardPro(camera, spriteSheet, srcRect, position, up, scale, offset, 0, WHITE);
  }
}

void TowerInit()
{
  for (int i = 0; i < TOWER_MAX_COUNT; i++)
  {
    towers[i] = (Tower){0};
  }
  towerCount = 0;

  towerModels[TOWER_TYPE_BASE] = LoadModel("data/keep.glb");
  towerModels[TOWER_TYPE_WALL] = LoadModel("data/wall-0000.glb");

  for (int i = 0; i < TOWER_TYPE_COUNT; i++)
  {
    if (towerModels[i].materials)
    {
      // assign the palette texture to the material of the model (0 is not used afaik)
      towerModels[i].materials[1].maps[MATERIAL_MAP_DIFFUSE].texture = palette;
    }
  }
}

static void TowerGunUpdate(Tower *tower)
{
  TowerTypeConfig config = towerTypeConfigs[tower->towerType];
  if (tower->cooldown <= 0.0f)
  {
    Enemy *enemy = EnemyGetClosestToCastle(tower->x, tower->y, config.range);
    if (enemy)
    {
      tower->cooldown = config.cooldown;
      // shoot the enemy; determine future position of the enemy
      float bulletSpeed = config.projectileSpeed;
      float bulletDamage = config.damage;
      Vector2 velocity = enemy->simVelocity;
      Vector2 futurePosition = EnemyGetPosition(enemy, gameTime.time - enemy->startMovingTime, &velocity, 0);
      Vector2 towerPosition = {tower->x, tower->y};
      float eta = Vector2Distance(towerPosition, futurePosition) / bulletSpeed;
      for (int i = 0; i < 8; i++) {
        velocity = enemy->simVelocity;
        futurePosition = EnemyGetPosition(enemy, gameTime.time - enemy->startMovingTime + eta, &velocity, 0);
        float distance = Vector2Distance(towerPosition, futurePosition);
        float eta2 = distance / bulletSpeed;
        if (fabs(eta - eta2) < 0.01f) {
          break;
        }
        eta = (eta2 + eta) * 0.5f;
      }
      ProjectileTryAdd(PROJECTILE_TYPE_ARROW, enemy, 
        (Vector3){towerPosition.x, 1.33f, towerPosition.y}, 
        (Vector3){futurePosition.x, 0.25f, futurePosition.y},
        bulletSpeed, bulletDamage);
      enemy->futureDamage += bulletDamage;
      tower->lastTargetPosition = futurePosition;
    }
  }
  else
  {
    tower->cooldown -= gameTime.deltaTime;
  }
}

Tower *TowerGetAt(int16_t x, int16_t y)
{
  for (int i = 0; i < towerCount; i++)
  {
    if (towers[i].x == x && towers[i].y == y && towers[i].towerType != TOWER_TYPE_NONE)
    {
      return &towers[i];
    }
  }
  return 0;
}

Tower *TowerTryAdd(uint8_t towerType, int16_t x, int16_t y)
{
  if (towerCount >= TOWER_MAX_COUNT)
  {
    return 0;
  }

  Tower *tower = TowerGetAt(x, y);
  if (tower)
  {
    return 0;
  }

  tower = &towers[towerCount++];
  tower->x = x;
  tower->y = y;
  tower->towerType = towerType;
  tower->cooldown = 0.0f;
  tower->damage = 0.0f;
  return tower;
}

Tower *GetTowerByType(uint8_t towerType)
{
  for (int i = 0; i < towerCount; i++)
  {
    if (towers[i].towerType == towerType)
    {
      return &towers[i];
    }
  }
  return 0;
}

int GetTowerCosts(uint8_t towerType)
{
  return towerTypeConfigs[towerType].cost;
}

float TowerGetMaxHealth(Tower *tower)
{
  return towerTypeConfigs[tower->towerType].maxHealth;
}

void TowerDraw()
{
  for (int i = 0; i < towerCount; i++)
  {
    Tower tower = towers[i];
    if (tower.towerType == TOWER_TYPE_NONE)
    {
      continue;
    }

    switch (tower.towerType)
    {
    case TOWER_TYPE_ARCHER:
      {
        Vector2 screenPosTower = GetWorldToScreen((Vector3){tower.x, 0.0f, tower.y}, currentLevel->camera);
        Vector2 screenPosTarget = GetWorldToScreen((Vector3){tower.lastTargetPosition.x, 0.0f, tower.lastTargetPosition.y}, currentLevel->camera);
        DrawModel(towerModels[TOWER_TYPE_WALL], (Vector3){tower.x, 0.0f, tower.y}, 1.0f, WHITE);
        DrawSpriteUnit(archerUnit, (Vector3){tower.x, 1.0f, tower.y}, 0, screenPosTarget.x > screenPosTower.x, 
          tower.cooldown > 0.2f ? SPRITE_UNIT_PHASE_WEAPON_COOLDOWN : SPRITE_UNIT_PHASE_WEAPON_IDLE);
      }
      break;
    case TOWER_TYPE_BALLISTA:
      DrawCube((Vector3){tower.x, 0.5f, tower.y}, 1.0f, 1.0f, 1.0f, BROWN);
      break;
    case TOWER_TYPE_CATAPULT:
      DrawCube((Vector3){tower.x, 0.5f, tower.y}, 1.0f, 1.0f, 1.0f, DARKGRAY);
      break;
    default:
      if (towerModels[tower.towerType].materials)
      {
        DrawModel(towerModels[tower.towerType], (Vector3){tower.x, 0.0f, tower.y}, 1.0f, WHITE);
      } else {
        DrawCube((Vector3){tower.x, 0.5f, tower.y}, 1.0f, 1.0f, 1.0f, LIGHTGRAY);
      }
      break;
    }
  }
}

void TowerUpdate()
{
  for (int i = 0; i < towerCount; i++)
  {
    Tower *tower = &towers[i];
    switch (tower->towerType)
    {
    case TOWER_TYPE_BALLISTA:
    case TOWER_TYPE_ARCHER:
      TowerGunUpdate(tower);
      break;
    }
  }
}

void TowerDrawHealthBars(Camera3D camera)
{
  for (int i = 0; i < towerCount; i++)
  {
    Tower *tower = &towers[i];
    if (tower->towerType == TOWER_TYPE_NONE || tower->damage <= 0.0f)
    {
      continue;
    }
    
    Vector3 position = (Vector3){tower->x, 0.5f, tower->y};
    float maxHealth = TowerGetMaxHealth(tower);
    float health = maxHealth - tower->damage;
    float healthRatio = health / maxHealth;
    
    DrawHealthBar(camera, position, healthRatio, GREEN, 35.0f);
  }
}