#include "td_main.h"
#include <raymath.h>

static Particle particles[PARTICLE_MAX_COUNT];
static int particleCount = 0;

void ParticleInit()
{
  for (int i = 0; i < PARTICLE_MAX_COUNT; i++)
  {
    particles[i] = (Particle){0};
  }
  particleCount = 0;
}

static void DrawExplosionParticle(Particle *particle, float transition)
{
  float size = 1.2f * (1.0f - transition);
  Color startColor = WHITE;
  Color endColor = RED;
  Color color = ColorLerp(startColor, endColor, transition);
  DrawCube(particle->position, size, size, size, color);
}

void ParticleAdd(uint8_t particleType, Vector3 position, Vector3 velocity, float lifetime)
{
  if (particleCount >= PARTICLE_MAX_COUNT)
  {
    return;
  }

  int index = -1;
  for (int i = 0; i < particleCount; i++)
  {
    if (particles[i].particleType == PARTICLE_TYPE_NONE)
    {
      index = i;
      break;
    }
  }

  if (index == -1)
  {
    index = particleCount++;
  }

  Particle *particle = &particles[index];
  particle->particleType = particleType;
  particle->spawnTime = gameTime.time;
  particle->lifetime = lifetime;
  particle->position = position;
  particle->velocity = velocity;
}

void ParticleUpdate()
{
  for (int i = 0; i < particleCount; i++)
  {
    Particle *particle = &particles[i];
    if (particle->particleType == PARTICLE_TYPE_NONE)
    {
      continue;
    }

    float age = gameTime.time - particle->spawnTime;

    if (particle->lifetime > age)
    {
      particle->position = Vector3Add(particle->position, Vector3Scale(particle->velocity, gameTime.deltaTime));
    }
    else {
      particle->particleType = PARTICLE_TYPE_NONE;
    }
  }
}

void ParticleDraw()
{
  for (int i = 0; i < particleCount; i++)
  {
    Particle particle = particles[i];
    if (particle.particleType == PARTICLE_TYPE_NONE)
    {
      continue;
    }

    float age = gameTime.time - particle.spawnTime;
    float transition = age / particle.lifetime;
    switch (particle.particleType)
    {
    case PARTICLE_TYPE_EXPLOSION:
      DrawExplosionParticle(&particle, transition);
      break;
    default:
      DrawCube(particle.position, 0.3f, 0.5f, 0.3f, RED);
      break;
    }
  }
}