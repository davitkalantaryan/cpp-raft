# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

# Default target executed when no arguments are given to make.
default_target: all
.PHONY : default_target

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Produce verbose output by default.
VERBOSE = 1

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/osada/progs/cpp-raft

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/osada/progs/cpp-raft

#=============================================================================
# Targets provided globally by CMake.

# Special rule for the target edit_cache
edit_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running interactive CMake command-line interface..."
	/usr/bin/cmake -i .
.PHONY : edit_cache

# Special rule for the target edit_cache
edit_cache/fast: edit_cache
.PHONY : edit_cache/fast

# Special rule for the target rebuild_cache
rebuild_cache:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --cyan "Running CMake to regenerate build system..."
	/usr/bin/cmake -H$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
.PHONY : rebuild_cache

# Special rule for the target rebuild_cache
rebuild_cache/fast: rebuild_cache
.PHONY : rebuild_cache/fast

# The main all target
all: cmake_check_build_system
	$(CMAKE_COMMAND) -E cmake_progress_start /home/osada/progs/cpp-raft/CMakeFiles /home/osada/progs/cpp-raft/CMakeFiles/progress.marks
	$(MAKE) -f CMakeFiles/Makefile2 all
	$(CMAKE_COMMAND) -E cmake_progress_start /home/osada/progs/cpp-raft/CMakeFiles 0
.PHONY : all

# The main clean target
clean:
	$(MAKE) -f CMakeFiles/Makefile2 clean
.PHONY : clean

# The main clean target
clean/fast: clean
.PHONY : clean/fast

# Prepare targets for installation.
preinstall: all
	$(MAKE) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall

# Prepare targets for installation.
preinstall/fast:
	$(MAKE) -f CMakeFiles/Makefile2 preinstall
.PHONY : preinstall/fast

# clear depends
depend:
	$(CMAKE_COMMAND) -H$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 1
.PHONY : depend

#=============================================================================
# Target rules for targets named cpp_raft

# Build rule for target.
cpp_raft: cmake_check_build_system
	$(MAKE) -f CMakeFiles/Makefile2 cpp_raft
.PHONY : cpp_raft

# fast build rule for target.
cpp_raft/fast:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/build
.PHONY : cpp_raft/fast

raft_logger.o: raft_logger.cpp.o
.PHONY : raft_logger.o

# target to build an object file
raft_logger.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_logger.cpp.o
.PHONY : raft_logger.cpp.o

raft_logger.i: raft_logger.cpp.i
.PHONY : raft_logger.i

# target to preprocess a source file
raft_logger.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_logger.cpp.i
.PHONY : raft_logger.cpp.i

raft_logger.s: raft_logger.cpp.s
.PHONY : raft_logger.s

# target to generate assembly for a file
raft_logger.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_logger.cpp.s
.PHONY : raft_logger.cpp.s

raft_node.o: raft_node.cpp.o
.PHONY : raft_node.o

# target to build an object file
raft_node.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_node.cpp.o
.PHONY : raft_node.cpp.o

raft_node.i: raft_node.cpp.i
.PHONY : raft_node.i

# target to preprocess a source file
raft_node.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_node.cpp.i
.PHONY : raft_node.cpp.i

raft_node.s: raft_node.cpp.s
.PHONY : raft_node.s

# target to generate assembly for a file
raft_node.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_node.cpp.s
.PHONY : raft_node.cpp.s

raft_server.o: raft_server.cpp.o
.PHONY : raft_server.o

# target to build an object file
raft_server.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_server.cpp.o
.PHONY : raft_server.cpp.o

raft_server.i: raft_server.cpp.i
.PHONY : raft_server.i

# target to preprocess a source file
raft_server.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_server.cpp.i
.PHONY : raft_server.cpp.i

raft_server.s: raft_server.cpp.s
.PHONY : raft_server.s

# target to generate assembly for a file
raft_server.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_server.cpp.s
.PHONY : raft_server.cpp.s

raft_server_properties.o: raft_server_properties.cpp.o
.PHONY : raft_server_properties.o

# target to build an object file
raft_server_properties.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_server_properties.cpp.o
.PHONY : raft_server_properties.cpp.o

raft_server_properties.i: raft_server_properties.cpp.i
.PHONY : raft_server_properties.i

# target to preprocess a source file
raft_server_properties.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_server_properties.cpp.i
.PHONY : raft_server_properties.cpp.i

raft_server_properties.s: raft_server_properties.cpp.s
.PHONY : raft_server_properties.s

# target to generate assembly for a file
raft_server_properties.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/raft_server_properties.cpp.s
.PHONY : raft_server_properties.cpp.s

state_mach.o: state_mach.cpp.o
.PHONY : state_mach.o

# target to build an object file
state_mach.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/state_mach.cpp.o
.PHONY : state_mach.cpp.o

state_mach.i: state_mach.cpp.i
.PHONY : state_mach.i

# target to preprocess a source file
state_mach.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/state_mach.cpp.i
.PHONY : state_mach.cpp.i

state_mach.s: state_mach.cpp.s
.PHONY : state_mach.s

# target to generate assembly for a file
state_mach.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/state_mach.cpp.s
.PHONY : state_mach.cpp.s

tests/main_test.o: tests/main_test.cpp.o
.PHONY : tests/main_test.o

# target to build an object file
tests/main_test.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/main_test.cpp.o
.PHONY : tests/main_test.cpp.o

tests/main_test.i: tests/main_test.cpp.i
.PHONY : tests/main_test.i

# target to preprocess a source file
tests/main_test.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/main_test.cpp.i
.PHONY : tests/main_test.cpp.i

tests/main_test.s: tests/main_test.cpp.s
.PHONY : tests/main_test.s

# target to generate assembly for a file
tests/main_test.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/main_test.cpp.s
.PHONY : tests/main_test.cpp.s

tests/mock_send_functions.o: tests/mock_send_functions.cpp.o
.PHONY : tests/mock_send_functions.o

# target to build an object file
tests/mock_send_functions.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/mock_send_functions.cpp.o
.PHONY : tests/mock_send_functions.cpp.o

tests/mock_send_functions.i: tests/mock_send_functions.cpp.i
.PHONY : tests/mock_send_functions.i

# target to preprocess a source file
tests/mock_send_functions.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/mock_send_functions.cpp.i
.PHONY : tests/mock_send_functions.cpp.i

tests/mock_send_functions.s: tests/mock_send_functions.cpp.s
.PHONY : tests/mock_send_functions.s

# target to generate assembly for a file
tests/mock_send_functions.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/mock_send_functions.cpp.s
.PHONY : tests/mock_send_functions.cpp.s

tests/test_log.o: tests/test_log.cpp.o
.PHONY : tests/test_log.o

# target to build an object file
tests/test_log.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_log.cpp.o
.PHONY : tests/test_log.cpp.o

tests/test_log.i: tests/test_log.cpp.i
.PHONY : tests/test_log.i

# target to preprocess a source file
tests/test_log.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_log.cpp.i
.PHONY : tests/test_log.cpp.i

tests/test_log.s: tests/test_log.cpp.s
.PHONY : tests/test_log.s

# target to generate assembly for a file
tests/test_log.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_log.cpp.s
.PHONY : tests/test_log.cpp.s

tests/test_node.o: tests/test_node.cpp.o
.PHONY : tests/test_node.o

# target to build an object file
tests/test_node.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_node.cpp.o
.PHONY : tests/test_node.cpp.o

tests/test_node.i: tests/test_node.cpp.i
.PHONY : tests/test_node.i

# target to preprocess a source file
tests/test_node.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_node.cpp.i
.PHONY : tests/test_node.cpp.i

tests/test_node.s: tests/test_node.cpp.s
.PHONY : tests/test_node.s

# target to generate assembly for a file
tests/test_node.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_node.cpp.s
.PHONY : tests/test_node.cpp.s

tests/test_scenario.o: tests/test_scenario.cpp.o
.PHONY : tests/test_scenario.o

# target to build an object file
tests/test_scenario.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_scenario.cpp.o
.PHONY : tests/test_scenario.cpp.o

tests/test_scenario.i: tests/test_scenario.cpp.i
.PHONY : tests/test_scenario.i

# target to preprocess a source file
tests/test_scenario.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_scenario.cpp.i
.PHONY : tests/test_scenario.cpp.i

tests/test_scenario.s: tests/test_scenario.cpp.s
.PHONY : tests/test_scenario.s

# target to generate assembly for a file
tests/test_scenario.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_scenario.cpp.s
.PHONY : tests/test_scenario.cpp.s

tests/test_server.o: tests/test_server.cpp.o
.PHONY : tests/test_server.o

# target to build an object file
tests/test_server.cpp.o:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_server.cpp.o
.PHONY : tests/test_server.cpp.o

tests/test_server.i: tests/test_server.cpp.i
.PHONY : tests/test_server.i

# target to preprocess a source file
tests/test_server.cpp.i:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_server.cpp.i
.PHONY : tests/test_server.cpp.i

tests/test_server.s: tests/test_server.cpp.s
.PHONY : tests/test_server.s

# target to generate assembly for a file
tests/test_server.cpp.s:
	$(MAKE) -f CMakeFiles/cpp_raft.dir/build.make CMakeFiles/cpp_raft.dir/tests/test_server.cpp.s
.PHONY : tests/test_server.cpp.s

# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (the default if no target is provided)"
	@echo "... clean"
	@echo "... depend"
	@echo "... cpp_raft"
	@echo "... edit_cache"
	@echo "... rebuild_cache"
	@echo "... raft_logger.o"
	@echo "... raft_logger.i"
	@echo "... raft_logger.s"
	@echo "... raft_node.o"
	@echo "... raft_node.i"
	@echo "... raft_node.s"
	@echo "... raft_server.o"
	@echo "... raft_server.i"
	@echo "... raft_server.s"
	@echo "... raft_server_properties.o"
	@echo "... raft_server_properties.i"
	@echo "... raft_server_properties.s"
	@echo "... state_mach.o"
	@echo "... state_mach.i"
	@echo "... state_mach.s"
	@echo "... tests/main_test.o"
	@echo "... tests/main_test.i"
	@echo "... tests/main_test.s"
	@echo "... tests/mock_send_functions.o"
	@echo "... tests/mock_send_functions.i"
	@echo "... tests/mock_send_functions.s"
	@echo "... tests/test_log.o"
	@echo "... tests/test_log.i"
	@echo "... tests/test_log.s"
	@echo "... tests/test_node.o"
	@echo "... tests/test_node.i"
	@echo "... tests/test_node.s"
	@echo "... tests/test_scenario.o"
	@echo "... tests/test_scenario.i"
	@echo "... tests/test_scenario.s"
	@echo "... tests/test_server.o"
	@echo "... tests/test_server.i"
	@echo "... tests/test_server.s"
.PHONY : help



#=============================================================================
# Special targets to cleanup operation of make.

# Special rule to run CMake to check the build system integrity.
# No rule that depends on this can have commands that come from listfiles
# because they might be regenerated.
cmake_check_build_system:
	$(CMAKE_COMMAND) -H$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 0
.PHONY : cmake_check_build_system

