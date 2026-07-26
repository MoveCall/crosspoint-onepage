add_executable(test_runtime test_runtime.cpp)
target_link_libraries(test_runtime PRIVATE arduino_host)
add_test(NAME arduino_host.runtime COMMAND test_runtime)
