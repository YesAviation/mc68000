# tests/CMakeLists.txt — Test executables

enable_testing()

# CPU + bus tests
add_executable(test_cpu test_cpu.c)
target_include_directories(test_cpu PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_cpu m68k_core m68k_common)

# Assembler tests
add_executable(test_assembler test_assembler.c)
target_include_directories(test_assembler PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_assembler m68k_asm_lib m68k_common)

# Disassembler tests
add_executable(test_disassembler test_disassembler.c)
target_include_directories(test_disassembler PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_disassembler m68k_dis_lib m68k_common)

# Register with CTest
add_test(NAME CpuTests COMMAND test_cpu)
add_test(NAME AssemblerTests COMMAND test_assembler)
add_test(NAME DisassemblerTests COMMAND test_disassembler)
