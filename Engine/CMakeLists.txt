# Collect all engine source files recursively
file(GLOB_RECURSE ENGINE_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

# Create the executable
add_executable(${PROJECT_NAME} WIN32
    ${ENGINE_SOURCES}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/ThirdParty
)

add_definitions(-DUNICODE -D_UNICODE)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_CharacterSet "Unicode"
)

# Link DirectX 12 libraries and dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    fastgltf::fastgltf
    glm::glm
    imgui
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
)

# Compiler options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4                 # Warning level 4
        /WX-                # Warnings are not errors
        /permissive-        # Standards conformance
        /Zc:__cplusplus     # Enable correct __cplusplus macro
    )
    
    # Enable multiprocessor compilation
    target_compile_options(${PROJECT_NAME} PRIVATE /MP)
endif()

# Organize source files in Visual Studio solution explorer
foreach(SOURCE IN ITEMS ${ENGINE_SOURCES})
    get_filename_component(SOURCE_PATH "${SOURCE}" PATH)
    file(RELATIVE_PATH SOURCE_PATH_REL "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${SOURCE_PATH}")
    string(REPLACE "/" "\\" GROUP_PATH "${SOURCE_PATH_REL}")
    source_group("${GROUP_PATH}" FILES "${SOURCE}")
endforeach()

# Copy shaders to output directory after build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Shaders
    COMMENT "Copying shaders to output directory"
)

# Set working directory for debugging in Visual Studio
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)