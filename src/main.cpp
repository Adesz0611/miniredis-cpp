#include "miniredis/store.h"

#include <iostream>

int main() {
    miniredis::Store store;
    store.set("hello", "world");
    std::cout << "hello=" << store.get("hello").value_or("<none>") << '\n';
    return 0;
}
