#include "../include/QuickMedia.hpp"
#include <X11/Xlib.h>

int main(int argc, char **argv) {
    XInitThreads();
    QuickMedia::Program program;
    return program.run(argc, argv);
}
