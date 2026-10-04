#pragma once

#include <string>

namespace zenith {

class ZenithCtl {
public:
    // Determine whether this process invocation is for CLI client mode
    static bool should_handle(int argc, char** argv);

    // Execute the requested CLI command and return process exit code
    static int run(int argc, char** argv);
};

} // namespace zenith
