#include <fstream>
#include <random>
#include <iostream>

void generate_matrix(const std::string& filename, int n, std::mt19937& gen) {
    std::ofstream file(filename, std::ios::binary);
    std::uniform_real_distribution<> dis(1.0, 5.0);
    for (int i = 0; i < n * n; ++i) {
        double val = dis(gen);
        file.write(reinterpret_cast<const char*>(&val), sizeof(double));
    }
}

int main() {
    const int n = 100;
    std::mt19937 gen1(42); // Seed for matrix1
    std::mt19937 gen2(84); // Different seed for matrix2

    generate_matrix("../matrix1.bin", n, gen1);
    generate_matrix("../matrix2.bin", n, gen2);

    std::cout << "Generated matrix1.bin and matrix2.bin with size " << n << "x" << n << "\n";
    return 0;
}