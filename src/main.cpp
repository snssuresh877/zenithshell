#include "app/app.hpp"
#include "cli/zenithctl.hpp"

int main(int argc, char** argv) {
    if (zenith::ZenithCtl::should_handle(argc, argv)) {
        return zenith::ZenithCtl::run(argc, argv);
    }

    zenith::App app(argc, argv);
    return app.run();
}
