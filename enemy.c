#include "td_main.h"
#include <raymath.h>
#include <stdlib.h>
#include <math.h>

EnemyClassConfig enemyClassConfigs[] = {
    [ENEMY_TYPE_MINION] = {
      .health = 10.0f, 
      .speed = 0.6f, 
      .radius = 0.25f, 
      .maxAcceleration = 1.0f,
      .explosionDamage = 1.0f,
      .requiredContactTime = 0.5f,
      .explosionRange = 1.0f,
      .explosionPushbackPower = 0.25f,
      .goldValue = 1,
    },
};

Enemy enemies[ENEMY_MAX_COUNT];
int enemyCount = 0;

SpriteUnit enemySprites[] = {
    [ENEMY_TYPE_MINION] = {
      .srcRect = {0, 16, 16, 16},
      .offset = {8.0f, 0.0f},
      .frameCount = 6,
      .frameDuration = 0.1f,
    },
};

void EnemyInit()
{
  for (int i = 0; i < ENEMY_MAX_COUNT; i++)
  {
    enemies[i] = (Enemy){0};
  }
  enemyCount = 0;
}

float EnemyGetCurrentMaxSpeed(Enemy *enemy)
{
  return enemyClassConfigs[enemy->enemyType].speed;
}

float EnemyGetMaxHealth(Enemy *enemy)
{
  return enemyClassConfigs[enemy->enemyType].health;
}

int EnemyGetNextPosition(int16_t currentX, int16_t currentY, int16_t *nextX, int16_t *nextY)
{
  int16_t castleX = 0;
  int16_t castleY = 0;
  int16_t dx = castleX - currentX;
  int16_t dy = castleY - currentY;
  if (dx == 0 && dy == 0)
  {
    *nextX = currentX;
    *nextY = currentY;
    return 1;
  }
  Vector2 gradient = PathFindingGetGradient((Vector3){currentX, 0, currentY});

  if (gradient.x == 0 && gradient.y == 0)
  {
    *nextX = currentX;
    *nextY = currentY;
    return 1;
  }

  if (fabsf(gradient.x) > fabsf(gradient.y))
  {
    *nextX = currentX + (int16_t)(gradient.x > 0.0f ? 1 : -1);
    *nextY = currentY;
    return 0;
  }
  *nextX = currentX;
  *nextY = currentY + (int16_t)(gradient.y > 0.0f ? 1 : -1);
  return 0;
}


// this function predicts the movement of the unit for the next deltaT seconds
Vector2 EnemyGetPosition(Enemy *enemy, float deltaT, Vector2 *velocity, int *waypointPassedCount)
{
  const float pointReachedDistance = 0.25f;
  const float pointReachedDistance2 = pointReachedDistance * pointReachedDistance;
  const float maxSimStepTime = 0.015625f;
  
  float maxAcceleration = enemyClassConfigs[enemy->enemyType].maxAcceleration;
  float maxSpeed = EnemyGetCurrentMaxSpeed(enemy);
  int16_t nextX = enemy->nextX;
  int16_t nextY = enemy->nextY;
  Vector2 position = enemy->simPosition;
  int passedCount = 0;
  for (float t = 0.0f; t < deltaT; t += maxSimStepTime)
  {
    float stepTime = fminf(deltaT - t, maxSimStepTime);
    Vector2 target = (Vector2){nextX, nextY};
    float speed = Vector2Length(*velocity);
    // draw the target position for debugging
    DrawCubeWires((Vector3){target.x, 0.2f, target.y}, 0.1f, 0.4f, 0.1f, RED);
    Vector2 lookForwardPos = Vector2Add(position, Vector2Scale(*velocity, speed));
    if (Vector2DistanceSqr(target, lookForwardPos) <= pointReachedDistance2)
    {
      // we reached the target position, let's move to the next waypoint
      EnemyGetNextPosition(nextX, nextY, &nextX, &nextY);
      target = (Vector2){nextX, nextY};
      // track how many waypoints we passed
      passedCount++;
    }
    
    // acceleration towards the target
    Vector2 unitDirection = Vector2Normalize(Vector2Subtract(target, lookForwardPos));
    Vector2 acceleration = Vector2Scale(unitDirection, maxAcceleration * stepTime);
    *velocity = Vector2Add(*velocity, acceleration);

    // limit the speed to the maximum speed
    if (speed > maxSpeed)
    {
      *velocity = Vector2Scale(*velocity, maxSpeed / speed);
    }

    // move the enemy
    position = Vector2Add(position, Vector2Scale(*velocity, stepTime));
  }

  if (waypointPassedCount)
  {
    (*waypointPassedCount) = passedCount;
  }

  return position;
}

void EnemyDraw()
{
  for (int i = 0; i < enemyCount; i++)
  {
    Enemy enemy = enemies[i];
    if (enemy.enemyType == ENEMY_TYPE_NONE)
    {
      continue;
    }

    Vector2 position = EnemyGetPosition(&enemy, gameTime.time - enemy.startMovingTime, &enemy.simVelocity, 0);
    
    // don't draw any trails for now; might replace this with footprints later
    // if (enemy.movePathCount > 0)
    // {
    //   Vector3 p = {enemy.movePath[0].x, 0.2f, enemy.movePath[0].y};
    //   DrawLine3D(p, (Vector3){position.x, 0.2f, position.y}, GREEN);
    // }
    // for (int j = 1; j < enemy.movePathCount; j++)
    // {
    //   Vector3 p = {enemy.movePath[j - 1].x, 0.2f, enemy.movePath[j - 1].y};
    //   Vector3 q = {enemy.movePath[j].x, 0.2f, enemy.movePath[j].y};
    //   DrawLine3D(p, q, GREEN);
    // }

    switch (enemy.enemyType)
    {
    case ENEMY_TYPE_MINION:
      DrawSpriteUnit(enemySprites[ENEMY_TYPE_MINION], (Vector3){position.x, 0.0f, position.y}, 
        enemy.walkedDistance, 0, 0);
      break;
    }
  }
}

void EnemyTriggerExplode(Enemy *enemy, Tower *tower, Vector3 explosionSource)
{
  // damage the tower
  float explosionDamge = enemyClassConfigs[enemy->enemyType].explosionDamage;
  float explosionRange = enemyClassConfigs[enemy->enemyType].explosionRange;
  float explosionPushbackPower = enemyClassConfigs[enemy->enemyType].explosionPushbackPower;
  float explosionRange2 = explosionRange * explosionRange;
  tower->damage += enemyClassConfigs[enemy->enemyType].explosionDamage;
  // explode the enemy
  if (tower->damage >= TowerGetMaxHealth(tower))
  {
    tower->towerType = TOWER_TYPE_NONE;
  }

  ParticleAdd(PARTICLE_TYPE_EXPLOSION, 
    explosionSource, 
    (Vector3){0, 0.1f, 0}, 1.0f);

  enemy->enemyType = ENEMY_TYPE_NONE;

  // push back enemies & dealing damage
  for (int i = 0; i < enemyCount; i++)
  {
    Enemy *other = &enemies[i];
    if (other->enemyType == ENEMY_TYPE_NONE)
    {
      continue;
    }
    float distanceSqr = Vector2DistanceSqr(enemy->simPosition, other->simPosition);
    if (distanceSqr > 0 && distanceSqr < explosionRange2)
    {
      Vector2 direction = Vector2Normalize(Vector2Subtract(other->simPosition, enemy->simPosition));
      other->simPosition = Vector2Add(other->simPosition, Vector2Scale(direction, explosionPushbackPower));
      EnemyAddDamage(other, explosionDamge);
    }
  }
}

void EnemyUpdate()
{
  const float castleX = 0;
  const float castleY = 0;
  const float maxPathDistance2 = 0.25f * 0.25f;
  
  for (int i = 0; i < enemyCount; i++)
  {
    Enemy *enemy = &enemies[i];
    if (enemy->enemyType == ENEMY_TYPE_NONE)
    {
      continue;
    }

    int waypointPassedCount = 0;
    Vector2 prevPosition = enemy->simPosition;
    enemy->simPosition = EnemyGetPosition(enemy, gameTime.time - enemy->startMovingTime, &enemy->simVelocity, &waypointPassedCount);
    enemy->startMovingTime = gameTime.time;
    enemy->walkedDistance += Vector2Distance(prevPosition, enemy->simPosition);
    // track path of unit
    if (enemy->movePathCount == 0 || Vector2DistanceSqr(enemy->simPosition, enemy->movePath[0]) > maxPathDistance2)
    {
      for (int j = ENEMY_MAX_PATH_COUNT - 1; j > 0; j--)
      {
        enemy->movePath[j] = enemy->movePath[j - 1];
      }
      enemy->movePath[0] = enemy->simPosition;
      if (++enemy->movePathCount > ENEMY_MAX_PATH_COUNT)
      {
        enemy->movePathCount = ENEMY_MAX_PATH_COUNT;
      }
    }

    if (waypointPassedCount > 0)
    {
      enemy->currentX = enemy->nextX;
      enemy->currentY = enemy->nextY;
      if (EnemyGetNextPosition(enemy->currentX, enemy->currentY, &enemy->nextX, &enemy->nextY) &&
        Vector2DistanceSqr(enemy->simPosition, (Vector2){castleX, castleY}) <= 0.25f * 0.25f)
      {
        // enemy reached the castle; remove it
        enemy->enemyType = ENEMY_TYPE_NONE;
        continue;
      }
    }
  }

  // handle collisions between enemies
  for (int i = 0; i < enemyCount - 1; i++)
  {
    Enemy *enemyA = &enemies[i];
    if (enemyA->enemyType == ENEMY_TYPE_NONE)
    {
      continue;
    }
    for (int j = i + 1; j < enemyCount; j++)
    {
      Enemy *enemyB = &enemies[j];
      if (enemyB->enemyType == ENEMY_TYPE_NONE)
      {
        continue;
      }
      float distanceSqr = Vector2DistanceSqr(enemyA->simPosition, enemyB->simPosition);
      float radiusA = enemyClassConfigs[enemyA->enemyType].radius;
      float radiusB = enemyClassConfigs[enemyB->enemyType].radius;
      float radiusSum = radiusA + radiusB;
      if (distanceSqr < radiusSum * radiusSum && distanceSqr > 0.001f)
      {
        // collision
        float distance = sqrtf(distanceSqr);
        float overlap = radiusSum - distance;
        // move the enemies apart, but softly; if we have a clog of enemies,
        // moving them perfectly apart can cause them to jitter
        float positionCorrection = overlap / 5.0f;
        Vector2 direction = (Vector2){
            (enemyB->simPosition.x - enemyA->simPosition.x) / distance * positionCorrection,
            (enemyB->simPosition.y - enemyA->simPosition.y) / distance * positionCorrection};
        enemyA->simPosition = Vector2Subtract(enemyA->simPosition, direction);
        enemyB->simPosition = Vector2Add(enemyB->simPosition, direction);
      }
    }
  }

  // handle collisions between enemies and towers
  for (int i = 0; i < enemyCount; i++)
  {
    Enemy *enemy = &enemies[i];
    if (enemy->enemyType == ENEMY_TYPE_NONE)
    {
      continue;
    }
    enemy->contactTime -= gameTime.deltaTime;
    if (enemy->contactTime < 0.0f)
    {
      enemy->contactTime = 0.0f;
    }

    float enemyRadius = enemyClassConfigs[enemy->enemyType].radius;
    // linear search over towers; could be optimized by using path finding tower map,
    // but for now, we keep it simple
    for (int j = 0; j < towerCount; j++)
    {
      Tower *tower = &towers[j];
      if (tower->towerType == TOWER_TYPE_NONE)
      {
        continue;
      }
      float distanceSqr = Vector2DistanceSqr(enemy->simPosition, (Vector2){tower->x, tower->y});
      float combinedRadius = enemyRadius + 0.708; // sqrt(0.5^2 + 0.5^2), corner-center distance of square with side length 1
      if (distanceSqr > combinedRadius * combinedRadius)
      {
        continue;
      }
      // potential collision; square / circle intersection
      float dx = tower->x - enemy->simPosition.x;
      float dy = tower->y - enemy->simPosition.y;
      float absDx = fabsf(dx);
      float absDy = fabsf(dy);
      Vector3 contactPoint = {0};
      if (absDx <= 0.5f && absDx <= absDy) {
        // vertical collision; push the enemy out horizontally
        float overlap = enemyRadius + 0.5f - absDy;
        if (overlap < 0.0f)
        {
          continue;
        }
        float direction = dy > 0.0f ? -1.0f : 1.0f;
        enemy->simPosition.y += direction * overlap;
        contactPoint = (Vector3){enemy->simPosition.x, 0.2f, tower->y + direction * 0.5f};
      }
      else if (absDy <= 0.5f && absDy <= absDx)
      {
        // horizontal collision; push the enemy out vertically
        float overlap = enemyRadius + 0.5f - absDx;
        if (overlap < 0.0f)
        {
          continue;
        }
        float direction = dx > 0.0f ? -1.0f : 1.0f;
        enemy->simPosition.x += direction * overlap;
        contactPoint = (Vector3){tower->x + direction * 0.5f, 0.2f, enemy->simPosition.y};
      }
      else
      {
        // possible collision with a corner
        float cornerDX = dx > 0.0f ? -0.5f : 0.5f;
        float cornerDY = dy > 0.0f ? -0.5f : 0.5f;
        float cornerX = tower->x + cornerDX;
        float cornerY = tower->y + cornerDY;
        float cornerDistanceSqr = Vector2DistanceSqr(enemy->simPosition, (Vector2){cornerX, cornerY});
        if (cornerDistanceSqr > enemyRadius * enemyRadius)
        {
          continue;
        }
        // push the enemy out along the diagonal
        float cornerDistance = sqrtf(cornerDistanceSqr);
        float overlap = enemyRadius - cornerDistance;
        float directionX = cornerDistance > 0.0f ? (cornerX - enemy->simPosition.x) / cornerDistance : -cornerDX;
        float directionY = cornerDistance > 0.0f ? (cornerY - enemy->simPosition.y) / cornerDistance : -cornerDY;
        enemy->simPosition.x -= directionX * overlap;
        enemy->simPosition.y -= directionY * overlap;
        contactPoint = (Vector3){cornerX, 0.2f, cornerY};
      }

      if (enemyClassConfigs[enemy->enemyType].explosionDamage > 0.0f)
      {
        enemy->contactTime += gameTime.deltaTime * 2.0f; // * 2 to undo the subtraction above
        if (enemy->contactTime >= enemyClassConfigs[enemy->enemyType].requiredContactTime)
        {
          EnemyTriggerExplode(enemy, tower, contactPoint);
        }
      }
    }
  }
}

EnemyId EnemyGetId(Enemy *enemy)
{
  return (EnemyId){enemy - enemies, enemy->generation};
}

Enemy *EnemyTryResolve(EnemyId enemyId)
{
  if (enemyId.index >= ENEMY_MAX_COUNT)
  {
    return 0;
  }
  Enemy *enemy = &enemies[enemyId.index];
  if (enemy->generation != enemyId.generation || enemy->enemyType == ENEMY_TYPE_NONE)
  {
    return 0;
  }
  return enemy;
}

Enemy *EnemyTryAdd(uint8_t enemyType, int16_t currentX, int16_t currentY)
{
  Enemy *spawn = 0;
  for (int i = 0; i < enemyCount; i++)
  {
    Enemy *enemy = &enemies[i];
    if (enemy->enemyType == ENEMY_TYPE_NONE)
    {
      spawn = enemy;
      break;
    }
  }

  if (enemyCount < ENEMY_MAX_COUNT && !spawn)
  {
    spawn = &enemies[enemyCount++];
  }

  if (spawn)
  {
    spawn->currentX = currentX;
    spawn->currentY = currentY;
    spawn->nextX = currentX;
    spawn->nextY = currentY;
    spawn->simPosition = (Vector2){currentX, currentY};
    spawn->simVelocity = (Vector2){0, 0};
    spawn->enemyType = enemyType;
    spawn->startMovingTime = gameTime.time;
    spawn->damage = 0.0f;
    spawn->futureDamage = 0.0f;
    spawn->generation++;
    spawn->movePathCount = 0;
    spawn->walkedDistance = 0.0f;
  }

  return spawn;
}

int EnemyAddDamage(Enemy *enemy, float damage)
{
  enemy->damage += damage;
  if (enemy->damage >= EnemyGetMaxHealth(enemy))
  {
    currentLevel->playerGold += enemyClassConfigs[enemy->enemyType].goldValue;
    enemy->enemyType = ENEMY_TYPE_NONE;
    return 1;
  }

  return 0;
}

Enemy* EnemyGetClosestToCastle(int16_t towerX, int16_t towerY, float range)
{
  int16_t castleX = 0;
  int16_t castleY = 0;
  Enemy* closest = 0;
  int16_t closestDistance = 0;
  float range2 = range * range;
  for (int i = 0; i < enemyCount; i++)
  {
    Enemy* enemy = &enemies[i];
    if (enemy->enemyType == ENEMY_TYPE_NONE)
    {
      continue;
    }
    float maxHealth = EnemyGetMaxHealth(enemy);
    if (enemy->futureDamage >= maxHealth)
    {
      // ignore enemies that will die soon
      continue;
    }
    int16_t dx = castleX - enemy->currentX;
    int16_t dy = castleY - enemy->currentY;
    int16_t distance = abs(dx) + abs(dy);
    if (!closest || distance < closestDistance)
    {
      float tdx = towerX - enemy->currentX;
      float tdy = towerY - enemy->currentY;
      float tdistance2 = tdx * tdx + tdy * tdy;
      if (tdistance2 <= range2)
      {
        closest = enemy;
        closestDistance = distance;
      }
    }
  }
  return closest;
}

int EnemyCount()
{
  int count = 0;
  for (int i = 0; i < enemyCount; i++)
  {
    if (enemies[i].enemyType != ENEMY_TYPE_NONE)
    {
      count++;
    }
  }
  return count;
}

void EnemyDrawHealthbars(Camera3D camera)
{
  for (int i = 0; i < enemyCount; i++)
  {
    Enemy *enemy = &enemies[i];
    if (enemy->enemyType == ENEMY_TYPE_NONE || enemy->damage == 0.0f)
    {
      continue;
    }
    Vector3 position = (Vector3){enemy->simPosition.x, 0.5f, enemy->simPosition.y};
    float maxHealth = EnemyGetMaxHealth(enemy);
    float health = maxHealth - enemy->damage;
    float healthRatio = health / maxHealth;
    
    DrawHealthBar(camera, position, healthRatio, GREEN, 15.0f);
  }
}