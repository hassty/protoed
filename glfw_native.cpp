// separate file to avoid namespace collisions between absl and x11 libs
#ifdef OS_IMPLEMENTATION_LINUX
#define GLFW_EXPOSE_NATIVE_X11
#elif OS_IMPLEMENTATION_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <nfd.h>
#include <nfd_glfw3.h>

#include "base/log.h"

void set_native_window(GLFWwindow *glfw_window, nfdwindowhandle_t *native_window) {
    if (!NFD_GetNativeWindowFromGLFWWindow(glfw_window, native_window)) {
        LOG_WRN("NFD_GetNativeWindowFromGLFWWindow failed");
    }
}
