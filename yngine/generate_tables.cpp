#include <yngine/bitboard.hpp>

#include <iostream>
#include <fstream>

using namespace Yngine;

void generate_rays_tables(std::ofstream& output) {
    output << "const Bitboard TABLE_RAYS" << "[121][6] = {\n";

    for (int index = 0; index < 11*11; index++) {
        output << "    {\n";

        for (int dir_index = 0; dir_index < 6; dir_index++) {
            output << "        ";

            const auto dir = direction_to_vec2[dir_index];

            if (Bitboard::is_index_in_game(index)) {
                const auto position = Bitboard::index_to_coords(index);

                Bitboard ray_builder{};

                auto x = position.first + dir.first;
                auto y = position.second + dir.second;
                while (Bitboard::are_coords_in_game(x, y)) {
                    ray_builder.set_bit(Bitboard::coords_to_index(x, y));

                    x += dir.first;
                    y += dir.second;
                }

                const auto result_bits = ray_builder.get_bits();
                const uint64_t low  = result_bits;
                const uint64_t high = result_bits >> 64;

                const auto save_flags{output.flags()};

                output <<
                    "Bitboard{((__uint128_t)0x" <<
                    std::hex << std::uppercase <<
                    high << " << 64) | 0x" << low << "},\n";

                output.flags(save_flags);
            } else {
                output << "Bitboard{0},\n";
            }
        }

        output << "    },\n";
    }

    output << "};\n\n";
}

int main(int argc, const char** argv) {
    if (argc != 2) {
        std::cerr <<
            "Wrong number of arguments, expected exactly 1 argument "
            "which is a file path to a header to generate" << std::endl;
        return 1;
    }

    const auto file_path = argv[1];
    std::ofstream header{file_path};

    header << "#ifndef YNGINE_TABLES_HPP\n";
    header << "#define YNGINE_TABLES_HPP\n\n";

    header << "#include <yngine/common.hpp>\n";
    header << "#include <yngine/bitboard.hpp>\n\n";
    header << "namespace Yngine {\n\n";

    generate_rays_tables(header);

    header << "}\n\n"; // namespace Yngine
    header << "#endif // YNGINE_TABLES_HPP\n";

    return 0;
}
