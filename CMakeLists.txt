# CMakeList.txt : CMake project for watcher, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Set the C++ standard to C++20
set (CMAKE_CXX_STANDARD 23)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("watcher" LANGUAGES CXX)

# Add source to this project's executable.
add_executable (watcher "src/watcher.cpp" "src/main.cpp")

# Add the include directory
target_include_directories (watcher PRIVATE "include")

# TODO: Add tests and install targets if needed.
