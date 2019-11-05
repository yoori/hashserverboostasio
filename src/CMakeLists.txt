project(BAHashServer)

add_executable(BAHashServer
  CRC.cpp
  Hash.cpp
  StringManip.cpp
  main.cpp
  )

target_link_libraries(BAHashServer
  boost_system
  boost_program_options
  ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS BAHashServer DESTINATION bin)
