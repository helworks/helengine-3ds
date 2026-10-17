#include <array>
#include <cassert>
#include "platform/3ds/Nintendo3DsTextureRegion.hpp"

/// Exercises actual upload helpers across horizontal and vertical Morton tile boundaries and rejects invalid source storage.
int main() {
    using namespace helengine::nintendo3ds;
    std::array<uint8_t, 16 * 16 * 4> destination;
    destination.fill(99);
    const std::array<uint8_t, 24> source {1,2,3,73,4,5,6,74,0,0,0,0,7,8,9,75,10,11,12,76,0,0,0,0};
    ValidateTextureRegion(16,16,7,7,2,2,source.data(),source.size(),12);
    WriteTextureRegion(destination.data(),16,7,7,2,2,source.data(),12);
    const std::array<std::size_t,4> literalTexelAddresses {63,106,149,192};
    for (std::size_t index = 0; index < literalTexelAddresses.size(); index++) {
        const std::size_t input = (index / 2) * 12 + (index % 2) * 4;
        const std::size_t output = literalTexelAddresses[index] * 4;
        for (std::size_t channel = 0; channel < 4; channel++) {
            assert(destination[output + channel] == source[input + 3 - channel]);
        }
    }
    assert(destination[0] == 99 && destination[255 * 4] == 99);
    const auto snapshot = destination;
    for (int test = 0; test < 5; test++) {
        bool rejected = false;
        try {
            ValidateTextureRegion(16,16,test == 0 ? -1 : 7,test == 1 ? 15 : 7,2,2,
                test == 2 ? nullptr : source.data(),test == 3 ? 19 : 24,test == 4 ? 7 : 12);
        } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && destination == snapshot);
    }
}
