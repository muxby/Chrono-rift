#include <iostream>
#include <cstdlib>

int main() {
    int seeds[] = {248, 753};
    for (int seed : seeds) {
        srand(seed);
        int num_npcs = (rand() % 8) + 2;
        std::cout << "Seed: " << seed << " -> num_npcs: " << num_npcs << std::endl;
    }
    return 0;
}
