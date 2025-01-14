#include "raylib.h"
#include "preferred_size.h"

// Since the canvas size is not known at compile time, we need to query it at runtime;
// the following platform specific code obtains the canvas size and we will use this
// size as the preferred size for the window at init time. We're ignoring here the
// possibility of the canvas size changing during runtime - this would require to
// poll the canvas size in the game loop or establishing a callback to be notified

#ifdef PLATFORM_WEB
#include <emscripten.h>
EMSCRIPTEN_RESULT emscripten_get_element_css_size(const char *target, double *width, double *height);

void GetPreferredSize(int *screenWidth, int *screenHeight)
{
  double canvasWidth, canvasHeight;
  emscripten_get_element_css_size("#" CANVAS_NAME, &canvasWidth, &canvasHeight);
  *screenWidth = (int)canvasWidth;
  *screenHeight = (int)canvasHeight;
  TraceLog(LOG_INFO, "preferred size for %s: %d %d", CANVAS_NAME, *screenWidth, *screenHeight);
}

int IsPaused()
{
  const char *js = "(function(){\n"
  "  var canvas = document.getElementById(\"" CANVAS_NAME "\");\n"
  "  var rect = canvas.getBoundingClientRect();\n"
  "  var isVisible = (\n"
  "    rect.top >= 0 &&\n"
  "    rect.left >= 0 &&\n"
  "    rect.bottom <= (window.innerHeight || document.documentElement.clientHeight) &&\n"
  "    rect.right <= (window.innerWidth || document.documentElement.clientWidth)\n"
  "  );\n"
  "  return isVisible ? 0 : 1;\n"
  "})()";
  return emscripten_run_script_int(js);
}

#else
void GetPreferredSize(int *screenWidth, int *screenHeight)
{
  *screenWidth = 600;
  *screenHeight = 240;
}
int IsPaused()
{
  return 0;
}
#endif