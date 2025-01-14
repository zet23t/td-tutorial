#include "td_main.h"
#include <raymath.h>

static Projectile projectiles[PROJECTILE_MAX_COUNT];
static int projectileCount = 0;

void ProjectileInit()
{
  for (int i = 0; i < PROJECTILE_MAX_COUNT; i++)
  {
    projectiles[i] = (Projectile){0};
  }
}

void ProjectileDraw()
{
  for (int i = 0; i < projectileCount; i++)
  {
    Projectile projectile = projectiles[i];
    if (projectile.projectileType == PROJECTILE_TYPE_NONE)
    {
      continue;
    }
    float transition = (gameTime.time - projectile.shootTime) / (projectile.arrivalTime - projectile.shootTime);
    if (transition >= 1.0f)
    {
      continue;
    }
    for (float transitionOffset = 0.0f; transitionOffset < 1.0f; transitionOffset += 0.1f)
    {
      float t = transition + transitionOffset * 0.3f;
      if (t > 1.0f)
      {
        break;
      }
      Vector3 position = Vector3Lerp(projectile.position, projectile.target, t);
      Color color = RED;
      if (projectile.projectileType == PROJECTILE_TYPE_ARROW)
      {
        // make tip red but quickly fade to brown
        color = ColorLerp(BROWN, RED, transitionOffset * transitionOffset);
        // fake a ballista flight path using parabola equation
        float parabolaT = t - 0.5f;
        parabolaT = 1.0f - 4.0f * parabolaT * parabolaT;
        position.y += 0.15f * parabolaT * projectile.distance;
      }

      float size = 0.06f * (transitionOffset + 0.25f);
      DrawCube(position, size, size, size, color);
    }
  }
}

void ProjectileUpdate()
{
  for (int i = 0; i < projectileCount; i++)
  {
    Projectile *projectile = &projectiles[i];
    if (projectile->projectileType == PROJECTILE_TYPE_NONE)
    {
      continue;
    }
    float transition = (gameTime.time - projectile->shootTime) / (projectile->arrivalTime - projectile->shootTime);
    if (transition >= 1.0f)
    {
      projectile->projectileType = PROJECTILE_TYPE_NONE;
      Enemy *enemy = EnemyTryResolve(projectile->targetEnemy);
      if (enemy)
      {
        EnemyAddDamage(enemy, projectile->damage);
      }
      continue;
    }
  }
}

Projectile *ProjectileTryAdd(uint8_t projectileType, Enemy *enemy, Vector3 position, Vector3 target, float speed, float damage)
{
  for (int i = 0; i < PROJECTILE_MAX_COUNT; i++)
  {
    Projectile *projectile = &projectiles[i];
    if (projectile->projectileType == PROJECTILE_TYPE_NONE)
    {
      projectile->projectileType = projectileType;
      projectile->shootTime = gameTime.time;
      float distance = Vector3Distance(position, target);
      projectile->arrivalTime = gameTime.time + distance / speed;
      projectile->damage = damage;
      projectile->position = position;
      projectile->target = target;
      projectile->directionNormal = Vector3Scale(Vector3Subtract(target, position), 1.0f / distance);
      projectile->distance = distance;
      projectile->targetEnemy = EnemyGetId(enemy);
      projectileCount = projectileCount <= i ? i + 1 : projectileCount;
      return projectile;
    }
  }
  return 0;
}