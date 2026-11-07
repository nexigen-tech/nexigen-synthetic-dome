/* Wrapper executable for the can_source component plugin.
 *
 * Standalone process that loads the shared library and runs until SIGTERM.
 * This is a minimal stand-in for integration with nexigen-run's walking
 * skeleton, which spawns separate processes and drives them through one frame.
 */

#include <cstdio>
#include <dlfcn.h>
#include <cstdlib>
#include <unistd.h>

int main() {
    // The actual plugin logic is in the .so loaded by the host.
    // This executable serves as a managed process placeholder.
    std::printf("[can_source] pid=%d ready\n", static_cast<int>(getpid()));
    std::fflush(stdout);
    while (true) { pause(); }
    return 0;
}