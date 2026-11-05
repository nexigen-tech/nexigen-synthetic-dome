/* Minimal stand-in component for the nxd-ws01 walking skeleton.
 *
 * The process simply waits for SIGTERM.  It prints its PID to stdout so that
 * the integration test can verify the process was actually launched.
 */

#include <cstdio>
#include <unistd.h>

int main() {
    std::printf("nxd-ws01-echo pid=%d ready\n", static_cast<int>(getpid()));
    std::fflush(stdout);

    // Suspend until SIGTERM arrives.  The default disposition terminates the
    // process, so pause() never returns in the success path.
    while (true) {
        pause();
    }

    return 0;
}
