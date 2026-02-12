add_executable(gateflow_tests
    test_gate.cpp
    test_circuit.cpp
    test_circuit_builder.cpp
    test_nand_decompose.cpp
    test_propagation.cpp
    test_scheduler.cpp
    test_layout_engine.cpp
)

target_link_libraries(gateflow_tests PRIVATE gateflow_simulation gateflow_timing gateflow_rendering Catch2::Catch2WithMain)

if(GATEFLOW_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(gateflow_tests PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    target_link_options(gateflow_tests PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()

# Enable CTest integration
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(gateflow_tests)
