#include "../include/QuickMedia.hpp"
#include <X11/Xlib.h>
#include <libgen.h>
#include <unistd.h>

int main(int argc, char **argv) {
    chdir(dirname(argv[0]));
    XInitThreads();
    QuickMedia::Program program;
    return program.run(argc, argv);
}
