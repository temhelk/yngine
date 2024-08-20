add_executable(playouts_test playouts.cpp)
target_link_libraries(playouts_test PRIVATE Yngine)

add_test(NAME Playouts COMMAND playouts_test)
