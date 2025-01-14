#include "td_main.h"
#include <raymath.h>

// The queue is a simple array of nodes, we add nodes to the end and remove
// nodes from the front. We keep the array around to avoid unnecessary allocations
static PathfindingNode *pathfindingNodeQueue = 0;
static int pathfindingNodeQueueCount = 0;
static int pathfindingNodeQueueCapacity = 0;

// The pathfinding map stores the distances from the castle to each cell in the map.
static PathfindingMap pathfindingMap = {0};

void PathfindingMapInit(int width, int height, Vector3 translate, float scale)
{
  // transforming between map space and world space allows us to adapt 
  // position and scale of the map without changing the pathfinding data
  pathfindingMap.toWorldSpace = MatrixTranslate(translate.x, translate.y, translate.z);
  pathfindingMap.toWorldSpace = MatrixMultiply(pathfindingMap.toWorldSpace, MatrixScale(scale, scale, scale));
  pathfindingMap.toMapSpace = MatrixInvert(pathfindingMap.toWorldSpace);
  pathfindingMap.width = width;
  pathfindingMap.height = height;
  pathfindingMap.scale = scale;
  pathfindingMap.distances = (float *)MemAlloc(width * height * sizeof(float));
  for (int i = 0; i < width * height; i++)
  {
    pathfindingMap.distances[i] = -1.0f;
  }

  pathfindingMap.towerIndex = (long *)MemAlloc(width * height * sizeof(long));
  pathfindingMap.deltaSrc = (DeltaSrc *)MemAlloc(width * height * sizeof(DeltaSrc));
}

static void PathFindingNodePush(int16_t x, int16_t y, int16_t fromX, int16_t fromY, float distance)
{
  if (pathfindingNodeQueueCount >= pathfindingNodeQueueCapacity)
  {
    pathfindingNodeQueueCapacity = pathfindingNodeQueueCapacity == 0 ? 256 : pathfindingNodeQueueCapacity * 2;
    // we use MemAlloc/MemRealloc to allocate memory for the queue
    // I am not entirely sure if MemRealloc allows passing a null pointer
    // so we check if the pointer is null and use MemAlloc in that case
    if (pathfindingNodeQueue == 0)
    {
      pathfindingNodeQueue = (PathfindingNode *)MemAlloc(pathfindingNodeQueueCapacity * sizeof(PathfindingNode));
    }
    else
    {
      pathfindingNodeQueue = (PathfindingNode *)MemRealloc(pathfindingNodeQueue, pathfindingNodeQueueCapacity * sizeof(PathfindingNode));
    }
  }

  PathfindingNode *node = &pathfindingNodeQueue[pathfindingNodeQueueCount++];
  node->x = x;
  node->y = y;
  node->fromX = fromX;
  node->fromY = fromY;
  node->distance = distance;
}

static PathfindingNode *PathFindingNodePop()
{
  if (pathfindingNodeQueueCount == 0)
  {
    return 0;
  }
  // we return the first node in the queue; we want to return a pointer to the node
  // so we can return 0 if the queue is empty. 
  // We should _not_ return a pointer to the element in the list, because the list
  // may be reallocated and the pointer would become invalid. Or the 
  // popped element is overwritten by the next push operation.
  // Using static here means that the variable is permanently allocated.
  static PathfindingNode node;
  node = pathfindingNodeQueue[0];
  // we shift all nodes one position to the front
  for (int i = 1; i < pathfindingNodeQueueCount; i++)
  {
    pathfindingNodeQueue[i - 1] = pathfindingNodeQueue[i];
  }
  --pathfindingNodeQueueCount;
  return &node;
}

float PathFindingGetDistance(int mapX, int mapY)
{
  if (mapX < 0 || mapX >= pathfindingMap.width || mapY < 0 || mapY >= pathfindingMap.height)
  {
    // when outside the map, we return the manhattan distance to the castle (0,0)
    return fabsf((float)mapX) + fabsf((float)mapY);
  }

  return pathfindingMap.distances[mapY * pathfindingMap.width + mapX];
}

// transform a world position to a map position in the array; 
// returns true if the position is inside the map
int PathFindingFromWorldToMapPosition(Vector3 worldPosition, int16_t *mapX, int16_t *mapY)
{
  Vector3 mapPosition = Vector3Transform(worldPosition, pathfindingMap.toMapSpace);
  *mapX = (int16_t)mapPosition.x;
  *mapY = (int16_t)mapPosition.z;
  return *mapX >= 0 && *mapX < pathfindingMap.width && *mapY >= 0 && *mapY < pathfindingMap.height;
}

void PathFindingMapUpdate()
{
  const int castleX = 0, castleY = 0;
  int16_t castleMapX, castleMapY;
  if (!PathFindingFromWorldToMapPosition((Vector3){castleX, 0.0f, castleY}, &castleMapX, &castleMapY))
  {
    return;
  }
  int width = pathfindingMap.width, height = pathfindingMap.height;

  // reset the distances to -1
  for (int i = 0; i < width * height; i++)
  {
    pathfindingMap.distances[i] = -1.0f;
  }
  // reset the tower indices
  for (int i = 0; i < width * height; i++)
  {
    pathfindingMap.towerIndex[i] = -1;
  }
  // reset the delta src
  for (int i = 0; i < width * height; i++)
  {
    pathfindingMap.deltaSrc[i].x = 0;
    pathfindingMap.deltaSrc[i].y = 0;
  }

  for (int i = 0; i < towerCount; i++)
  {
    Tower *tower = &towers[i];
    if (tower->towerType == TOWER_TYPE_NONE || tower->towerType == TOWER_TYPE_BASE)
    {
      continue;
    }
    int16_t mapX, mapY;
    // technically, if the tower cell scale is not in sync with the pathfinding map scale,
    // this would not work correctly and needs to be refined to allow towers covering multiple cells
    // or having multiple towers in one cell; for simplicity, we assume that the tower covers exactly
    // one cell. For now.
    if (!PathFindingFromWorldToMapPosition((Vector3){tower->x, 0.0f, tower->y}, &mapX, &mapY))
    {
      continue;
    }
    int index = mapY * width + mapX;
    pathfindingMap.towerIndex[index] = i;
  }

  // we start at the castle and add the castle to the queue
  pathfindingMap.maxDistance = 0.0f;
  pathfindingNodeQueueCount = 0;
  PathFindingNodePush(castleMapX, castleMapY, castleMapX, castleMapY, 0.0f);
  PathfindingNode *node = 0;
  while ((node = PathFindingNodePop()))
  {
    if (node->x < 0 || node->x >= width || node->y < 0 || node->y >= height)
    {
      continue;
    }
    int index = node->y * width + node->x;
    if (pathfindingMap.distances[index] >= 0 && pathfindingMap.distances[index] <= node->distance)
    {
      continue;
    }

    int deltaX = node->x - node->fromX;
    int deltaY = node->y - node->fromY;
    // even if the cell is blocked by a tower, we still may want to store the direction
    // (though this might not be needed, IDK right now)
    pathfindingMap.deltaSrc[index].x = (char) deltaX;
    pathfindingMap.deltaSrc[index].y = (char) deltaY;

    // we skip nodes that are blocked by towers
    if (pathfindingMap.towerIndex[index] >= 0)
    {
      node->distance += 8.0f;
    }
    pathfindingMap.distances[index] = node->distance;
    pathfindingMap.maxDistance = fmaxf(pathfindingMap.maxDistance, node->distance);
    PathFindingNodePush(node->x, node->y + 1, node->x, node->y, node->distance + 1.0f);
    PathFindingNodePush(node->x, node->y - 1, node->x, node->y, node->distance + 1.0f);
    PathFindingNodePush(node->x + 1, node->y, node->x, node->y, node->distance + 1.0f);
    PathFindingNodePush(node->x - 1, node->y, node->x, node->y, node->distance + 1.0f);
  }
}

void PathFindingMapDraw()
{
  float cellSize = pathfindingMap.scale * 0.9f;
  float highlightDistance = fmodf(GetTime() * 4.0f, pathfindingMap.maxDistance);
  for (int x = 0; x < pathfindingMap.width; x++)
  {
    for (int y = 0; y < pathfindingMap.height; y++)
    {
      float distance = pathfindingMap.distances[y * pathfindingMap.width + x];
      float colorV = distance < 0 ? 0 : fminf(distance / pathfindingMap.maxDistance, 1.0f);
      Color color = distance < 0 ? BLUE : (Color){fminf(colorV, 1.0f) * 255, 0, 0, 255};
      Vector3 position = Vector3Transform((Vector3){x, -0.25f, y}, pathfindingMap.toWorldSpace);
      // animate the distance "wave" to show how the pathfinding algorithm expands
      // from the castle
      if (distance + 0.5f > highlightDistance && distance - 0.5f < highlightDistance)
      {
        color = BLACK;
      }
      DrawCube(position, cellSize, 0.1f, cellSize, color);
    }
  }
}

Vector2 PathFindingGetGradient(Vector3 world)
{
  int16_t mapX, mapY;
  if (PathFindingFromWorldToMapPosition(world, &mapX, &mapY))
  {
    DeltaSrc delta = pathfindingMap.deltaSrc[mapY * pathfindingMap.width + mapX];
    return (Vector2){(float)-delta.x, (float)-delta.y};
  }
  // fallback to a simple gradient calculation
  float n = PathFindingGetDistance(mapX, mapY - 1);
  float s = PathFindingGetDistance(mapX, mapY + 1);
  float w = PathFindingGetDistance(mapX - 1, mapY);
  float e = PathFindingGetDistance(mapX + 1, mapY);
  return (Vector2){w - e + 0.25f, n - s + 0.125f};
}