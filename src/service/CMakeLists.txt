add_executable(service service.c)
target_link_libraries(service ws2_32)
add_executable(agent agent.c)
target_link_libraries(agent ws2_32)

