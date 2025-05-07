# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\EventMonitor_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\EventMonitor_autogen.dir\\ParseCache.txt"
  "EventMonitor_autogen"
  )
endif()
