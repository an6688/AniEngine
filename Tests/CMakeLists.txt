file(GLOB_RECURSE SMOKE_ENGINE_SOURCES "${CMAKE_SOURCE_DIR}/Engine/Source/*.cpp")
list(FILTER SMOKE_ENGINE_SOURCES EXCLUDE REGEX "/Main\\.cpp$")
add_executable(ShadowSmoke ShadowSmoke.cpp ${SMOKE_ENGINE_SOURCES})
get_target_property(ENGINE_INCLUDES AniEngine INCLUDE_DIRECTORIES)
get_target_property(ENGINE_LIBRARIES AniEngine LINK_LIBRARIES)
get_target_property(ENGINE_DEFINITIONS AniEngine COMPILE_DEFINITIONS)
target_include_directories(ShadowSmoke PRIVATE ${ENGINE_INCLUDES})
target_link_libraries(ShadowSmoke PRIVATE ${ENGINE_LIBRARIES})
target_compile_definitions(ShadowSmoke PRIVATE ${ENGINE_DEFINITIONS})
add_custom_command(TARGET ShadowSmoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Engine/Shaders" "$<TARGET_FILE_DIR:ShadowSmoke>/Shaders")
add_test(NAME directional_shadow_smoke COMMAND ShadowSmoke)
set_tests_properties(directional_shadow_smoke PROPERTIES WORKING_DIRECTORY "$<TARGET_FILE_DIR:ShadowSmoke>")

add_executable(ScenePersistence ScenePersistence.cpp ${SMOKE_ENGINE_SOURCES})
target_include_directories(ScenePersistence PRIVATE ${ENGINE_INCLUDES})
target_link_libraries(ScenePersistence PRIVATE ${ENGINE_LIBRARIES})
target_compile_definitions(ScenePersistence PRIVATE ${ENGINE_DEFINITIONS})
add_test(NAME scene_persistence COMMAND ScenePersistence)

add_executable(ImageProcessingSmoke ImageProcessingSmoke.cpp ${SMOKE_ENGINE_SOURCES})
target_include_directories(ImageProcessingSmoke PRIVATE ${ENGINE_INCLUDES})
target_link_libraries(ImageProcessingSmoke PRIVATE ${ENGINE_LIBRARIES})
target_compile_definitions(ImageProcessingSmoke PRIVATE ${ENGINE_DEFINITIONS})
add_custom_command(TARGET ImageProcessingSmoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Engine/Shaders" "$<TARGET_FILE_DIR:ImageProcessingSmoke>/Shaders")
add_test(NAME image_processing_smoke COMMAND ImageProcessingSmoke)
set_tests_properties(image_processing_smoke PROPERTIES WORKING_DIRECTORY "$<TARGET_FILE_DIR:ImageProcessingSmoke>")
