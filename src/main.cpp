#include <iostream>

#include "version.h"

int main()
{
    std::cout << "Algol-GDR compiler, version " << version_string() << '\n';
    return 0;
}
