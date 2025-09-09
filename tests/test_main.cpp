#include <cstdio>

int bit_exact_test_main();
int e2e_chain_test_main();
int no_alloc_test_main();
int performance_test_main();
int roundtrip_test_main();
int whitening_test_main();
int equal_power_bin_test_main();
int sync_word_test_main();
int error_code_test_main();
int odd_symbol_count_test_main();
int scratch_buffer_error_test_main();
int lorawan_mic_test_main();
int base64_utils_test_main();

int main() {
    int result = 0;
    result |= bit_exact_test_main();
    result |= e2e_chain_test_main();
    result |= no_alloc_test_main();
    result |= performance_test_main();
    result |= roundtrip_test_main();
    result |= whitening_test_main();
    result |= equal_power_bin_test_main();
    result |= sync_word_test_main();
    result |= error_code_test_main();
    result |= odd_symbol_count_test_main();
    result |= scratch_buffer_error_test_main();
    result |= lorawan_mic_test_main();
    result |= base64_utils_test_main();
    if (result != 0) {
        std::printf("Some tests failed\n");
    }
    return result;
}
