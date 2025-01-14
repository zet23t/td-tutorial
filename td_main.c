#include "td_main.h"
#include <raymath.h>
#include <stdlib.h>
#include <math.h>

//# Variables
GUIState guiState = {0};
GameTime gameTime = {0};

Model floorTileAModel = {0};
Model floorTileBModel = {0};
Model treeModel[2] = {0};
Model firTreeModel[2] = {0};
Model rockModels[5] = {0};
Model grassPatchModel[1] = {0};

Texture2D palette, spriteSheet;

Level levels[] = {
  [0] = {
    .state = LEVEL_STATE_BUILDING,
    .initialGold = 20,
    .waves[0] = {
      .enemyType = ENEMY_TYPE_MINION,
      .wave = 0,
      .count = 10,
      .interval = 2.5f,
      .delay = 1.0f,
      .spawnPosition = {0, 6},
    },
    .waves[1] = {
      .enemyType = ENEMY_TYPE_MINION,
      .wave = 1,
      .count = 20,
      .interval = 1.5f,
      .delay = 1.0f,
      .spawnPosition = {0, 6},
    },
    .waves[2] = {
      .enemyType = ENEMY_TYPE_MINION,
      .wave = 2,
      .count = 30,
      .interval = 1.2f,
      .delay = 1.0f,
      .spawnPosition = {0, 6},
    }
  },
};

Level *currentLevel = levels;

//# Game

static Model LoadGLBModel(char *filename)
{
  Model model = LoadModel(TextFormat("data/%s.glb",filename));
  if (model.materialCount > 1)
  {
    model.materials[1].maps[MATERIAL_MAP_DIFFUSE].texture = palette;
  }
  return model;
}

void LoadAssets()
{
  // load a sprite sheet that contains all units
  spriteSheet = LoadTexture("data/spritesheet.png");
  SetTextureFilter(spriteSheet, TEXTURE_FILTER_BILINEAR);

  // we'll use a palette texture to colorize the all buildings and environment art
  palette = LoadTexture("data/palette.png");
  // The texture uses gradients on very small space, so we'll enable bilinear filtering
  SetTextureFilter(palette, TEXTURE_FILTER_BILINEAR);

  floorTileAModel = LoadGLBModel("floor-tile-a");
  floorTileBModel = LoadGLBModel("floor-tile-b");
  treeModel[0] = LoadGLBModel("leaftree-large-1-a");
  treeModel[1] = LoadGLBModel("leaftree-large-1-b");
  firTreeModel[0] = LoadGLBModel("firtree-1-a");
  firTreeModel[1] = LoadGLBModel("firtree-1-b");
  rockModels[0] = LoadGLBModel("rock-1");
  rockModels[1] = LoadGLBModel("rock-2");
  rockModels[2] = LoadGLBModel("rock-3");
  rockModels[3] = LoadGLBModel("rock-4");
  rockModels[4] = LoadGLBModel("rock-5");
  grassPatchModel[0] = LoadGLBModel("grass-patch-1");
}

void InitLevel(Level *level)
{
  level->seed = (int)(GetTime() * 100.0f);

  TowerInit();
  EnemyInit();
  ProjectileInit();
  ParticleInit();
  TowerTryAdd(TOWER_TYPE_BASE, 0, 0);

  level->placementMode = 0;
  level->state = LEVEL_STATE_BUILDING;
  level->nextState = LEVEL_STATE_NONE;
  level->playerGold = level->initialGold;
  level->currentWave = 0;

  Camera *camera = &level->camera;
  camera->position = (Vector3){4.0f, 8.0f, 8.0f};
  camera->target = (Vector3){0.0f, 0.0f, 0.0f};
  camera->up = (Vector3){0.0f, 1.0f, 0.0f};
  camera->fovy = 10.0f;
  camera->projection = CAMERA_ORTHOGRAPHIC;
}

void DrawLevelHud(Level *level)
{
  const char *text = TextFormat("Gold: %d", level->playerGold);
  Font font = GetFontDefault();
  DrawTextEx(font, text, (Vector2){GetScreenWidth() - 120, 10}, font.baseSize * 2.0f, 2.0f, BLACK);
  DrawTextEx(font, text, (Vector2){GetScreenWidth() - 122, 8}, font.baseSize * 2.0f, 2.0f, YELLOW);
}

void DrawLevelReportLostWave(Level *level)
{
  BeginMode3D(level->camera);
  DrawLevelGround(level);
  TowerDraw();
  EnemyDraw();
  ProjectileDraw();
  ParticleDraw();
  guiState.isBlocked = 0;
  EndMode3D();

  TowerDrawHealthBars(level->camera);

  const char *text = "Wave lost";
  int textWidth = MeasureText(text, 20);
  DrawText(text, (GetScreenWidth() - textWidth) * 0.5f, 20, 20, WHITE);

  if (Button("Reset level", 20, GetScreenHeight() - 40, 160, 30, 0))
  {
    level->nextState = LEVEL_STATE_RESET;
  }
}

int HasLevelNextWave(Level *level)
{
  for (int i = 0; i < 10; i++)
  {
    EnemyWave *wave = &level->waves[i];
    if (wave->wave == level->currentWave)
    {
      return 1;
    }
  }
  return 0;
}

void DrawLevelReportWonWave(Level *level)
{
  BeginMode3D(level->camera);
  DrawLevelGround(level);
  TowerDraw();
  EnemyDraw();
  ProjectileDraw();
  ParticleDraw();
  guiState.isBlocked = 0;
  EndMode3D();

  TowerDrawHealthBars(level->camera);

  const char *text = "Wave won";
  int textWidth = MeasureText(text, 20);
  DrawText(text, (GetScreenWidth() - textWidth) * 0.5f, 20, 20, WHITE);


  if (Button("Reset level", 20, GetScreenHeight() - 40, 160, 30, 0))
  {
    level->nextState = LEVEL_STATE_RESET;
  }

  if (HasLevelNextWave(level))
  {
    if (Button("Prepare for next wave", GetScreenWidth() - 300, GetScreenHeight() - 40, 300, 30, 0))
    {
      level->nextState = LEVEL_STATE_BUILDING;
    }
  }
  else {
    if (Button("Level won", GetScreenWidth() - 300, GetScreenHeight() - 40, 300, 30, 0))
    {
      level->nextState = LEVEL_STATE_WON_LEVEL;
    }
  }
}

void DrawBuildingBuildButton(Level *level, int x, int y, int width, int height, uint8_t towerType, const char *name)
{
  static ButtonState buttonStates[8] = {0};
  int cost = GetTowerCosts(towerType);
  const char *text = TextFormat("%s: %d", name, cost);
  buttonStates[towerType].isSelected = level->placementMode == towerType;
  buttonStates[towerType].isDisabled = level->playerGold < cost;
  if (Button(text, x, y, width, height, &buttonStates[towerType]))
  {
    level->placementMode = buttonStates[towerType].isSelected ? 0 : towerType;
  }
}

float GetRandomFloat(float min, float max)
{
  int random = GetRandomValue(0, 0xfffffff);
  return ((float)random / (float)0xfffffff) * (max - min) + min;
}

void DrawLevelGround(Level *level)
{
  // draw checkerboard ground pattern
  for (int x = -5; x <= 5; x += 1)
  {
    for (int y = -5; y <= 5; y += 1)
    {
      Model *model = (x + y) % 2 == 0 ? &floorTileAModel : &floorTileBModel;
      DrawModel(*model, (Vector3){x, 0.0f, y}, 1.0f, WHITE);
    }
  }

  int oldSeed = GetRandomValue(0, 0xfffffff);
  SetRandomSeed(level->seed);
  // increase probability for trees via duplicated entries
  Model borderModels[64];
  int maxRockCount = GetRandomValue(2, 6);
  int maxTreeCount = GetRandomValue(10, 20);
  int maxFirTreeCount = GetRandomValue(5, 10);
  int maxLeafTreeCount = maxTreeCount - maxFirTreeCount;
  int grassPatchCount = GetRandomValue(5, 30);

  int modelCount = 0;
  for (int i = 0; i < maxRockCount && modelCount < 63; i++)
  {
    borderModels[modelCount++] = rockModels[GetRandomValue(0, 5)];
  }
  for (int i = 0; i < maxLeafTreeCount && modelCount < 63; i++)
  {
    borderModels[modelCount++] = treeModel[GetRandomValue(0, 1)];
  }
  for (int i = 0; i < maxFirTreeCount && modelCount < 63; i++)
  {
    borderModels[modelCount++] = firTreeModel[GetRandomValue(0, 1)];
  }
  for (int i = 0; i < grassPatchCount && modelCount < 63; i++)
  {
    borderModels[modelCount++] = grassPatchModel[0];
  }

  // draw some objects around the border of the map
  Vector3 up = {0, 1, 0};
  // a pseudo random number generator to get the same result every time
  const float wiggle = 0.75f;
  const int layerCount = 3;
  for (int layer = 0; layer < layerCount; layer++)
  {
    int layerPos = 6 + layer;
    for (int x = -6 + layer; x <= 6 + layer; x += 1)
    {
      DrawModelEx(borderModels[GetRandomValue(0, modelCount - 1)], 
        (Vector3){x + GetRandomFloat(0.0f, wiggle), 0.0f, -layerPos + GetRandomFloat(0.0f, wiggle)}, 
        up, GetRandomFloat(0.0f, 360), Vector3One(), WHITE);
      DrawModelEx(borderModels[GetRandomValue(0, modelCount - 1)], 
        (Vector3){x + GetRandomFloat(0.0f, wiggle), 0.0f, layerPos + GetRandomFloat(0.0f, wiggle)}, 
        up, GetRandomFloat(0.0f, 360), Vector3One(), WHITE);
    }

    for (int z = -5 + layer; z <= 5 + layer; z += 1)
    {
      DrawModelEx(borderModels[GetRandomValue(0, modelCount - 1)], 
        (Vector3){-layerPos + GetRandomFloat(0.0f, wiggle), 0.0f, z + GetRandomFloat(0.0f, wiggle)}, 
        up, GetRandomFloat(0.0f, 360), Vector3One(), WHITE);
      DrawModelEx(borderModels[GetRandomValue(0, modelCount - 1)], 
        (Vector3){layerPos + GetRandomFloat(0.0f, wiggle), 0.0f, z + GetRandomFloat(0.0f, wiggle)}, 
        up, GetRandomFloat(0.0f, 360), Vector3One(), WHITE);
    }
  }

  SetRandomSeed(oldSeed);
}

void DrawLevelBuildingState(Level *level)
{
  BeginMode3D(level->camera);
  DrawLevelGround(level);
  TowerDraw();
  EnemyDraw();
  ProjectileDraw();
  ParticleDraw();

  Ray ray = GetScreenToWorldRay(GetMousePosition(), level->camera);
  float planeDistance = ray.position.y / -ray.direction.y;
  float planeX = ray.direction.x * planeDistance + ray.position.x;
  float planeY = ray.direction.z * planeDistance + ray.position.z;
  int16_t mapX = (int16_t)floorf(planeX + 0.5f);
  int16_t mapY = (int16_t)floorf(planeY + 0.5f);
  if (level->placementMode && !guiState.isBlocked && mapX >= -5 && mapX <= 5 && mapY >= -5 && mapY <= 5)
  {
    DrawCubeWires((Vector3){mapX, 0.2f, mapY}, 1.0f, 0.4f, 1.0f, RED);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
      if (TowerTryAdd(level->placementMode, mapX, mapY))
      {
        level->playerGold -= GetTowerCosts(level->placementMode);
        level->placementMode = TOWER_TYPE_NONE;
      }
    }
  }

  guiState.isBlocked = 0;

  EndMode3D();

  TowerDrawHealthBars(level->camera);

  static ButtonState buildWallButtonState = {0};
  static ButtonState buildGunButtonState = {0};
  buildWallButtonState.isSelected = level->placementMode == TOWER_TYPE_WALL;
  buildGunButtonState.isSelected = level->placementMode == TOWER_TYPE_ARCHER;

  DrawBuildingBuildButton(level, 10, 10, 110, 30, TOWER_TYPE_WALL, "Wall");
  DrawBuildingBuildButton(level, 10, 50, 110, 30, TOWER_TYPE_ARCHER, "Archer");
  DrawBuildingBuildButton(level, 10, 90, 110, 30, TOWER_TYPE_BALLISTA, "Ballista");
  DrawBuildingBuildButton(level, 10, 130, 110, 30, TOWER_TYPE_CATAPULT, "Catapult");

  if (Button("Reset level", 20, GetScreenHeight() - 40, 160, 30, 0))
  {
    level->nextState = LEVEL_STATE_RESET;
  }
  
  if (Button("Begin waves", GetScreenWidth() - 160, GetScreenHeight() - 40, 160, 30, 0))
  {
    level->nextState = LEVEL_STATE_BATTLE;
  }

  const char *text = "Building phase";
  int textWidth = MeasureText(text, 20);
  DrawText(text, (GetScreenWidth() - textWidth) * 0.5f, 20, 20, WHITE);
}

void InitBattleStateConditions(Level *level)
{
  level->state = LEVEL_STATE_BATTLE;
  level->nextState = LEVEL_STATE_NONE;
  level->waveEndTimer = 0.0f;
  for (int i = 0; i < 10; i++)
  {
    EnemyWave *wave = &level->waves[i];
    wave->spawned = 0;
    wave->timeToSpawnNext = wave->delay;
  }
}

void DrawLevelBattleState(Level *level)
{
  BeginMode3D(level->camera);
  DrawLevelGround(level);
  TowerDraw();
  EnemyDraw();
  ProjectileDraw();
  ParticleDraw();
  guiState.isBlocked = 0;
  EndMode3D();

  EnemyDrawHealthbars(level->camera);
  TowerDrawHealthBars(level->camera);

  if (Button("Reset level", 20, GetScreenHeight() - 40, 160, 30, 0))
  {
    level->nextState = LEVEL_STATE_RESET;
  }

  int maxCount = 0;
  int remainingCount = 0;
  for (int i = 0; i < 10; i++)
  {
    EnemyWave *wave = &level->waves[i];
    if (wave->wave != level->currentWave)
    {
      continue;
    }
    maxCount += wave->count;
    remainingCount += wave->count - wave->spawned;
  }
  int aliveCount = EnemyCount();
  remainingCount += aliveCount;

  const char *text = TextFormat("Battle phase: %03d%%", 100 - remainingCount * 100 / maxCount);
  int textWidth = MeasureText(text, 20);
  DrawText(text, (GetScreenWidth() - textWidth) * 0.5f, 20, 20, WHITE);
}

void DrawLevel(Level *level)
{
  switch (level->state)
  {
    case LEVEL_STATE_BUILDING: DrawLevelBuildingState(level); break;
    case LEVEL_STATE_BATTLE: DrawLevelBattleState(level); break;
    case LEVEL_STATE_WON_WAVE: DrawLevelReportWonWave(level); break;
    case LEVEL_STATE_LOST_WAVE: DrawLevelReportLostWave(level); break;
    default: break;
  }

  DrawLevelHud(level);
}

void UpdateLevel(Level *level)
{
  if (level->state == LEVEL_STATE_BATTLE)
  {
    int activeWaves = 0;
    for (int i = 0; i < 10; i++)
    {
      EnemyWave *wave = &level->waves[i];
      if (wave->spawned >= wave->count || wave->wave != level->currentWave)
      {
        continue;
      }
      activeWaves++;
      wave->timeToSpawnNext -= gameTime.deltaTime;
      if (wave->timeToSpawnNext <= 0.0f)
      {
        Enemy *enemy = EnemyTryAdd(wave->enemyType, wave->spawnPosition.x, wave->spawnPosition.y);
        if (enemy)
        {
          wave->timeToSpawnNext = wave->interval;
          wave->spawned++;
        }
      }
    }
    if (GetTowerByType(TOWER_TYPE_BASE) == 0) {
      level->waveEndTimer += gameTime.deltaTime;
      if (level->waveEndTimer >= 2.0f)
      {
        level->nextState = LEVEL_STATE_LOST_WAVE;
      }
    }
    else if (activeWaves == 0 && EnemyCount() == 0)
    {
      level->waveEndTimer += gameTime.deltaTime;
      if (level->waveEndTimer >= 2.0f)
      {
        level->nextState = LEVEL_STATE_WON_WAVE;
      }
    }
  }

  PathFindingMapUpdate();
  EnemyUpdate();
  TowerUpdate();
  ProjectileUpdate();
  ParticleUpdate();

  if (level->nextState == LEVEL_STATE_RESET)
  {
    InitLevel(level);
  }
  
  if (level->nextState == LEVEL_STATE_BATTLE)
  {
    InitBattleStateConditions(level);
  }
  
  if (level->nextState == LEVEL_STATE_WON_WAVE)
  {
    level->currentWave++;
    level->state = LEVEL_STATE_WON_WAVE;
  }
  
  if (level->nextState == LEVEL_STATE_LOST_WAVE)
  {
    level->state = LEVEL_STATE_LOST_WAVE;
  }

  if (level->nextState == LEVEL_STATE_BUILDING)
  {
    level->state = LEVEL_STATE_BUILDING;
  }

  if (level->nextState == LEVEL_STATE_WON_LEVEL)
  {
    // make something of this later
    InitLevel(level);
  }

  level->nextState = LEVEL_STATE_NONE;
}

float nextSpawnTime = 0.0f;

void ResetGame()
{
  InitLevel(currentLevel);
}

void InitGame()
{
  TowerInit();
  EnemyInit();
  ProjectileInit();
  ParticleInit();
  PathfindingMapInit(20, 20, (Vector3){-10.0f, 0.0f, -10.0f}, 1.0f);

  currentLevel = levels;
  InitLevel(currentLevel);
}

//# Immediate GUI functions

void DrawHealthBar(Camera3D camera, Vector3 position, float healthRatio, Color barColor, float healthBarWidth)
{
  const float healthBarHeight = 6.0f;
  const float healthBarOffset = 15.0f;
  const float inset = 2.0f;
  const float innerWidth = healthBarWidth - inset * 2;
  const float innerHeight = healthBarHeight - inset * 2;

  Vector2 screenPos = GetWorldToScreen(position, camera);
  float centerX = screenPos.x - healthBarWidth * 0.5f;
  float topY = screenPos.y - healthBarOffset;
  DrawRectangle(centerX, topY, healthBarWidth, healthBarHeight, BLACK);
  float healthWidth = innerWidth * healthRatio;
  DrawRectangle(centerX + inset, topY + inset, healthWidth, innerHeight, barColor);
}

int Button(const char *text, int x, int y, int width, int height, ButtonState *state)
{
  Rectangle bounds = {x, y, width, height};
  int isPressed = 0;
  int isSelected = state && state->isSelected;
  int isDisabled = state && state->isDisabled;
  if (CheckCollisionPointRec(GetMousePosition(), bounds) && !guiState.isBlocked && !isDisabled)
  {
    Color color = isSelected ? DARKGRAY : GRAY;
    DrawRectangle(x, y, width, height, color);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
      isPressed = 1;
    }
    guiState.isBlocked = 1;
  }
  else
  {
    Color color = isSelected ? WHITE : LIGHTGRAY;
    DrawRectangle(x, y, width, height, color);
  }
  Font font = GetFontDefault();
  Vector2 textSize = MeasureTextEx(font, text, font.baseSize * 2.0f, 1);
  Color textColor = isDisabled ? GRAY : BLACK;
  DrawTextEx(font, text, (Vector2){x + width / 2 - textSize.x / 2, y + height / 2 - textSize.y / 2}, font.baseSize * 2.0f, 1, textColor);
  return isPressed;
}

//# Main game loop

void GameUpdate()
{
  float dt = GetFrameTime();
  // cap maximum delta time to 0.1 seconds to prevent large time steps
  if (dt > 0.1f) dt = 0.1f;
  gameTime.time += dt;
  gameTime.deltaTime = dt;

  UpdateLevel(currentLevel);
}

int main(void)
{
  int screenWidth, screenHeight;
  GetPreferredSize(&screenWidth, &screenHeight);
  InitWindow(screenWidth, screenHeight, "Tower defense");
  SetTargetFPS(30);

  LoadAssets();
  InitGame();

  while (!WindowShouldClose())
  {
    if (IsPaused()) {
      // canvas is not visible in browser - do nothing
      continue;
    }

    BeginDrawing();
    ClearBackground((Color){0x4E, 0x63, 0x26, 0xFF});

    GameUpdate();
    DrawLevel(currentLevel);

    EndDrawing();
  }

  CloseWindow();

  return 0;
}