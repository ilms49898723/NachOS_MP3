#include "syscall.h"

int main() {
    int i;
    int n = 0;

    for (i = 0; i < 100; ++i) {
        n = n + 1;
    }

    PrintInt(1);

    for (i = 0; i < 10000; ++i) {
        n = n + 1;
    }
}
