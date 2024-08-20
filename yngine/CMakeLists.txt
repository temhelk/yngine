# Generate tables
add_executable(
    generate_tables
    generate_tables.cpp
    bitboard.cpp bitboard.hpp
)

target_include_directories(generate_tables PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_features(generate_tables PRIVATE cxx_std_20)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tables.hpp
    COMMAND generate_tables ${CMAKE_CURRENT_SOURCE_DIR}/tables.hpp
    DEPENDS generate_tables
)

# Set up the library
add_library(
    Yngine
    bitboard.cpp bitboard.hpp
    moves.cpp moves.hpp
    board_state.cpp board_state.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tables.hpp
)

add_library(
    Yngine::Yngine ALIAS Yngine
)

set_target_properties(
    Yngine PROPERTIES
    CXX_EXTENSIONS OFF
    INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Debug>:FALSE:TRUE>
)

target_compile_definitions(
    Yngine
    PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(Yngine PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(Yngine PUBLIC cxx_std_20)
