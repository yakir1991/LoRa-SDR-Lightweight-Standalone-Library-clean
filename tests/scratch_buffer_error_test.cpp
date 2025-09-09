#include <lora_phy/phy.hpp>
#include <cerrno>
#include <complex>
#include <iostream>
#include <vector>

int main() {
    using namespace lora_phy;
    const unsigned sf = 7;
    const size_t N = size_t(1) << sf;

    lora_demod_workspace ws{};
    lora_demod_init(&ws, sf, window_type::window_none, nullptr, 0);

    std::vector<std::complex<float>> samples(N, std::complex<float>(2.0f, 0.0f));
    std::vector<uint16_t> out(1);
    ssize_t rc = lora_demodulate(&ws, samples.data(), samples.size(), out.data(), 1);

    lora_demod_free(&ws);

    if (rc != -ERANGE) {
        std::cerr << "Expected -ERANGE, got " << rc << std::endl;
        return 1;
    }
    return 0;
}
