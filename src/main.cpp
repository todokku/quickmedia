#include "../include/QuickMedia.hpp"
#include <X11/Xlib.h>

int main() {
    XInitThreads();
    QuickMedia::Program program;
    program.run();
    return 0;
}
