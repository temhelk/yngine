#include <yngine/board_state.hpp>
#include <yngine/tables.hpp>

#include <cassert>
#include <random>

namespace Yngine {

BoardState::BoardState()
    : next_action{NextAction::RingPlacement}
    , ring_and_row_removal_color{Color::Black}
    , last_ring_move_color{Color::Black}
    , last_ring_move{0, 0, Direction::SE}
    , white_rings{}
    , black_rings{}
    , white_markers{}
    , black_markers{} {
}

void BoardState::generate_moves(MoveList& move_list) const {
    switch (this->next_action) {
    case NextAction::RingPlacement: {
        this->generate_ring_placement_moves(move_list);
    } break;
    case NextAction::RingMovement: {
        this->generate_ring_moves(move_list);
    } break;
    case NextAction::RowRemoval: {
        this->generate_row_removal(move_list);
    } break;
    case NextAction::RingRemoval: {
        this->generate_ring_removal(move_list);
    } break;
    case NextAction::Done: {
        abort();
    } break;
    }

    assert(move_list.get_size() != 0);
}

void BoardState::apply_move(Move move) {
    std::visit(variant_overloaded{
        [this](PlaceRingMove move) {
            if (this->last_ring_move_color == Color::Black) {
                assert(this->white_rings.get_bit(move.index) == 0);
                this->white_rings.set_bit(move.index);
            } else {
                assert(this->black_rings.get_bit(move.index) == 0);
                this->black_rings.set_bit(move.index);
            }

            this->last_ring_move_color = opposite(this->last_ring_move_color);

            const auto black_ring_count = this->black_rings.popcount();

            if (black_ring_count == 5) {
                this->next_action = NextAction::RingMovement;
            }
        },
        [this](RingMove move) {
            const auto all_markers = this->white_markers | this->black_markers;
            assert(all_markers.get_bit(move.to) == 0);

            if (this->last_ring_move_color == Color::Black) {
                assert(this->black_rings.get_bit(move.to) == 0);

                this->white_rings.clear_bit(move.from);
                this->white_rings.set_bit(move.to);

                this->white_markers.set_bit(move.from);
            } else {
                assert(this->white_rings.get_bit(move.to) == 0);

                this->black_rings.clear_bit(move.from);
                this->black_rings.set_bit(move.to);

                this->black_markers.set_bit(move.from);
            }

            const auto direction_num = static_cast<uint8_t>(move.direction);
            const auto ray_from_from = TABLE_RAYS[move.from][direction_num];
            const auto ray_from_to   = TABLE_RAYS[move.to][direction_num];

            const auto need_to_flip_nodes = ray_from_from & ~ray_from_to;

            const auto black_markers_to_flip = this->black_markers & need_to_flip_nodes;
            const auto white_markers_to_flip = this->white_markers & need_to_flip_nodes;

            this->white_markers &= ~need_to_flip_nodes;
            this->black_markers &= ~need_to_flip_nodes;

            this->white_markers |= black_markers_to_flip;
            this->black_markers |= white_markers_to_flip;

            this->last_ring_move = move;
            this->last_ring_move_color = opposite(this->last_ring_move_color);

            // After moving we have to check whether we formed any rows
            // and set the state correspondingly
            const auto rows_color = this->check_rows(move);
            if (rows_color) {
                this->next_action = NextAction::RowRemoval;
                this->ring_and_row_removal_color = *rows_color;
            } else {
                // If we can't remove rows we check if we used all 51 markers
                if (this->white_markers.popcount() +
                    this->black_markers.popcount() == 51) {
                    this->next_action = NextAction::Done;
                }
            }
        },
        [this](RemoveRowMove move) {
            const auto remove_markers = BoardState::line_in_direction(move.from, move.direction, 5);
            assert(remove_markers.popcount() == 5);

            if (this->ring_and_row_removal_color == Color::White) {
                assert((white_markers & remove_markers).popcount() == 5);
                this->white_markers &= ~remove_markers;
            } else {
                assert((black_markers & remove_markers).popcount() == 5);
                this->black_markers &= ~remove_markers;
            }

            this->next_action = NextAction::RingRemoval;
        },
        [this](RemoveRingMove move) {
            if (this->ring_and_row_removal_color == Color::White) {
                this->white_rings.clear_bit(move.index);
            } else {
                this->black_rings.clear_bit(move.index);
            }

            // Check for win condition
            if (this->white_rings.popcount() == 2 ||
                this->black_rings.popcount() == 2) {
                this->next_action = NextAction::Done;
                return;
            }

            // Check if we still have rows left after the last move
            const auto rows_color = this->check_rows(this->last_ring_move);
            if (rows_color) {
                this->next_action = NextAction::RowRemoval;
                this->ring_and_row_removal_color = *rows_color;
            } else {
                this->next_action = NextAction::RingMovement;
            }
        },
        [this](PassMove move) {
            this->last_ring_move_color = opposite(this->last_ring_move_color);
        },
    }, move);
}

void BoardState::playout(XoshiroCpp::Xoshiro256StarStar& prng) {
    MoveList move_list;
    while (this->next_action != NextAction::Done) {
        this->generate_moves(move_list);

        std::uniform_int_distribution<size_t> dist{0, move_list.get_size() - 1};
        const auto move = move_list[dist(prng)];

        this->apply_move(move);

        move_list.reset();
    }
}

NextAction BoardState::get_next_action() const {
    return this->next_action;
}

GameResult BoardState::game_result() const {
    assert(this->next_action == NextAction::Done);

    const auto white_ring_count = this->white_rings.popcount();
    const auto black_ring_count = this->black_rings.popcount();

    if (white_ring_count == black_ring_count) {
        return GameResult::Draw;
    }

    if (white_ring_count < black_ring_count) {
        return GameResult::WhiteWon;
    } else {
        return GameResult::BlackWon;
    }
}

void BoardState::generate_ring_placement_moves(MoveList& move_list) const {
    Bitboard occupancy = this->white_rings | this->black_rings;
    Bitboard empty_nodes = ~occupancy & Bitboard::get_game_board();

    while (empty_nodes) {
        const auto index = empty_nodes.bit_scan_and_reset();
        move_list.append(PlaceRingMove{index});
    }
}

void BoardState::generate_ring_moves(MoveList& move_list) const {
    const auto all_rings = this->white_rings | this->black_rings;
    const auto all_markers = this->white_markers | this->black_markers;

    auto our_rings_iter =
        this->last_ring_move_color == Color::White
        ? this->black_rings : this->white_rings;

    while (our_rings_iter) {
        const auto ring_index = our_rings_iter.bit_scan_and_reset();

        for (int direction_num = 0; direction_num < 6; direction_num++) {
            const auto direction = static_cast<Direction>(direction_num);

            const auto ray_from_our_ring = TABLE_RAYS[ring_index][direction_num];
            const auto blocking_rings = all_rings & ray_from_our_ring;

            Bitboard possible_moves_without_rings;
            if (blocking_rings) {
                uint8_t closest_ring_index = blocking_rings.bit_scan_direction(direction);

                auto block_ray = TABLE_RAYS[closest_ring_index][direction_num];
                block_ray.set_bit(closest_ring_index);

                possible_moves_without_rings = ray_from_our_ring & ~block_ray;
            } else {
                possible_moves_without_rings = ray_from_our_ring;
            }

            const auto markers_on_the_way = all_markers & ray_from_our_ring;
            const auto empty_spots_on_the_way = ~markers_on_the_way & ray_from_our_ring;

            auto markers_on_the_way_shifted = markers_on_the_way;
            markers_on_the_way_shifted.shift_in_direction(direction);

            const auto empty_spots_after_markers =
                markers_on_the_way_shifted & empty_spots_on_the_way;

            Bitboard allowed_moves;
            if (empty_spots_after_markers) {
                uint8_t closest_last_move_spot_index =
                    empty_spots_after_markers.bit_scan_direction(direction);

                const auto forbidden_moves_after_markers =
                    TABLE_RAYS[closest_last_move_spot_index][direction_num];

                allowed_moves =
                    possible_moves_without_rings &
                    ~all_markers &
                    ~forbidden_moves_after_markers;
            } else {
                allowed_moves =
                    possible_moves_without_rings &
                    ~all_markers;
            }

            while (allowed_moves) {
                const auto move_index = allowed_moves.bit_scan_and_reset();

                move_list.append(RingMove{ring_index, move_index, direction});
            }
        }
    }

    if (move_list.get_size() == 0) {
        move_list.append(PassMove{});
    }
}

void BoardState::generate_row_removal(MoveList& move_list) const {
    const auto last_move = this->last_ring_move;
    const auto direction_num = static_cast<uint8_t>(last_move.direction);

    const auto ray_from_from = TABLE_RAYS[last_move.from][direction_num];
    const auto ray_from_to   = TABLE_RAYS[last_move.to  ][direction_num];

    auto affected_nodes = ray_from_from & ~ray_from_to;
    affected_nodes.set_bit(last_move.from);

    const auto markers =
        this->ring_and_row_removal_color == Color::White ?
        this->white_markers : this->black_markers;

    // Check for special case of a row in the axis of movement
    // but only if it's the same color as the move we made before
    if (this->last_ring_move_color == this->ring_and_row_removal_color &&
        markers.get_bit(last_move.from) == 1) {
        const auto length_in_move_direction =
            length_of_row(markers, last_move.from, last_move.direction);

        const auto length_opposite_move_direction =
            length_of_row(markers, last_move.from, opposite(last_move.direction));

        const auto total_length =
            length_in_move_direction +
            length_opposite_move_direction + 1;

        if (total_length >= 5) {
            const auto number_of_rows = total_length - 4;

            const auto last_opposite_dir_index = Bitboard::index_move_direction(
                last_move.from,
                opposite(last_move.direction),
                length_opposite_move_direction
            );

            for (int row_index = 0; row_index < number_of_rows; row_index++) {
                const auto from = Bitboard::index_move_direction(
                    last_opposite_dir_index,
                    last_move.direction,
                    row_index
                );

                move_list.append(RemoveRowMove{from, last_move.direction});
            }
        }
    }

    // Check for other 2 axis that are not the movement axis
    auto affected_markers_iter = markers & affected_nodes;
    while (affected_markers_iter) {
        const auto affected_marker_index =
            affected_markers_iter.bit_scan_and_reset();

        const Direction axes[3] = {
            Direction::SE, Direction::NE, Direction::N,
        };

        for (int axis_index = 0; axis_index < 3; axis_index++) {
            const auto axis = axes[axis_index];

            if (axis == last_move.direction || axis == opposite(last_move.direction))
                continue;

            const auto length_along_axis =
                length_of_row(markers, affected_marker_index, axis);

            const auto length_anti_axis =
                length_of_row(markers, affected_marker_index, opposite(axis));

            const auto total_length = length_along_axis + length_anti_axis + 1;

            if (total_length >= 5) {
                const auto number_of_rows = total_length - 4;

                const auto last_anti_axis_index = Bitboard::index_move_direction(
                    affected_marker_index,
                    opposite(axis),
                    length_anti_axis
                );

                for (int row_index = 0; row_index < number_of_rows; row_index++) {
                    const auto from = Bitboard::index_move_direction(
                        last_anti_axis_index,
                        axis,
                        row_index
                    );

                    move_list.append(RemoveRowMove{from, axis});
                }
            }
        }
    }
}

void BoardState::generate_ring_removal(MoveList& move_list) const {
    Bitboard ring_iter =
        this->ring_and_row_removal_color == Color::White ?
        this->white_rings : this->black_rings;

    while (ring_iter) {
        const auto ring_index = ring_iter.bit_scan_and_reset();

        move_list.append(RemoveRingMove{ring_index});
    }
}

std::optional<Color> BoardState::check_rows(RingMove last_move) const {
    const auto direction_num = static_cast<uint8_t>(last_move.direction);

    const auto ray_from_from = TABLE_RAYS[last_move.from][direction_num];
    const auto ray_from_to   = TABLE_RAYS[last_move.to  ][direction_num];

    auto affected_nodes = ray_from_from & ~ray_from_to;
    affected_nodes.set_bit(last_move.from);

    // Check for special case of a row in the axis of movement
    const auto last_mover_markers =
        this->last_ring_move_color == Color::White ?
        this->white_markers : this->black_markers;

    if (last_mover_markers.get_bit(last_move.from) == 1) {
        auto last_mover_affected_markers_iter =
            last_mover_markers & affected_nodes;

        const auto length_in_move_direction =
            length_of_row(last_mover_markers, last_move.from, last_move.direction);

        const auto length_opposite_move_direction =
            length_of_row(last_mover_markers, last_move.from, opposite(last_move.direction));

        if (length_in_move_direction + length_opposite_move_direction >= 4) {
            return this->last_ring_move_color;
        }
    }

    // Check for other 2 axis that are not the movement axis and for all colors
    // starting with the last mover's color
    const Color check_colors[2] = {
        this->last_ring_move_color,
        opposite(this->last_ring_move_color),
    };

    for (int check_color_index = 0; check_color_index < 2; check_color_index++) {
        const auto color = check_colors[check_color_index];

        const auto marker_bitboard_for_color =
            color == Color::White ? this->white_markers : this->black_markers;

        auto affected_markers_iter = marker_bitboard_for_color & affected_nodes;

        while (affected_markers_iter) {
            const auto affected_marker_index =
                affected_markers_iter.bit_scan_and_reset();

            const Direction axes[3] = {
                Direction::SE, Direction::NE, Direction::N,
            };

            for (int axis_index = 0; axis_index < 3; axis_index++) {
                const auto axis = axes[axis_index];

                if (axis == last_move.direction || axis == opposite(last_move.direction))
                    continue;

                const auto length_along_axis =
                    length_of_row(marker_bitboard_for_color, affected_marker_index, axis);

                const auto length_anti_axis =
                    length_of_row(marker_bitboard_for_color, affected_marker_index, opposite(axis));

                if (length_along_axis + length_anti_axis >= 4) {
                    return color;
                }
            }
        }
    }

    return std::nullopt;
}

uint8_t BoardState::length_of_row(Bitboard bitboard, uint8_t index, Direction direction) {
    const auto direction_num = static_cast<uint8_t>(direction);

    const auto ray_from_index = TABLE_RAYS[index][direction_num];
    const auto empty_spaces_on_ray = ~bitboard & ray_from_index;

    if (empty_spaces_on_ray) {
        const auto closest_empty_spot = empty_spaces_on_ray.bit_scan_direction(direction);

        auto ray_from_empty_spot = TABLE_RAYS[closest_empty_spot][direction_num];
        ray_from_empty_spot.set_bit(closest_empty_spot);

        const auto markers_in_row = ray_from_index & ~ray_from_empty_spot;

        if (markers_in_row) {
            return markers_in_row.popcount();
        } else {
            return 0;
        }
    } else {
        return ray_from_index.popcount();
    }
}

Bitboard BoardState::line_in_direction(uint8_t index, Direction direction, uint8_t length) {
    const auto direction_num = static_cast<uint8_t>(direction);

    auto ray_from_index = TABLE_RAYS[index][direction_num];
    ray_from_index.set_bit(index);

    const auto end_index = Bitboard::index_move_direction(
        index, direction, length - 1
    );

    const auto ray_from_end_index = TABLE_RAYS[end_index][direction_num];

    return ray_from_index & ~ray_from_end_index;
}

std::ostream& operator<<(std::ostream& out, BoardState board_state) {
    const auto game_board = Bitboard::get_game_board();

    for (int y = 10; y >= 0; y--) {
        for (int t = 0; t < y; t++)
            out << "    ";

        const auto diagonal_length = 11 - y;

        for (int n = 0; n < diagonal_length; n++) {
            if (game_board.get_bit(Bitboard::coords_to_index(n, y + n))) {
                const auto index = Bitboard::coords_to_index(n, y + n);
                if (board_state.white_rings.get_bit(index))
                    out << "A" << "       ";
                else if (board_state.white_markers.get_bit(index))
                    out << "a" << "       ";
                else if (board_state.black_rings.get_bit(index))
                    out << "B" << "       ";
                else if (board_state.black_markers.get_bit(index))
                    out << "b" << "       ";
                else
                    out << "." << "       ";;
            } else {
                out << " " << "       ";
            }
        }

        out << "\n";
    }

    for (int x = 1; x < 11; x++) {
        for (int t = 0; t < x; t++)
            out << "    ";

        const auto diagonal_length = 11 - x;

        for (int n = 0; n < diagonal_length; n++) {
            if (game_board.get_bit(Bitboard::coords_to_index(x + n, n))) {
                const auto index = Bitboard::coords_to_index(x + n, n);

                if (board_state.white_rings.get_bit(index))
                    out << "A" << "       ";
                else if (board_state.white_markers.get_bit(index))
                    out << "a" << "       ";
                else if (board_state.black_rings.get_bit(index))
                    out << "B" << "       ";
                else if (board_state.black_markers.get_bit(index))
                    out << "b" << "       ";
                else
                    out << "." << "       ";;
            } else {
                out << " " << "       ";
            }
        }

        out << "\n";
    }

    return out;
}

}
