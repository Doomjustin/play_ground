file(GLOB BG_UTILITY_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.h")
file(GLOB BG_UTILITY_IMPLEMENT_SOURCE CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
file(GLOB BG_UTILITY_TEST_SOURCE CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.test.cpp")

list(FILTER BG_UTILITY_IMPLEMENT_SOURCE EXCLUDE REGEX "\\.test\\.cpp$")

add_library(playground_utility ${BG_UTILITY_IMPLEMENT_SOURCE})
add_library(pg::utility ALIAS playground_utility)

target_compile_features(playground_utility PUBLIC cxx_std_23)
target_include_directories(playground_utility PUBLIC ${PG_INCLUDE_DIRS})
target_link_libraries(playground_utility
    PUBLIC
        spdlog::spdlog
        magic_enum::magic_enum
)

if (${PROJECT_NAME}_BUILD_TESTS AND BG_UTILITY_TEST_SOURCE)
    include(Catch)
    
    add_executable(playground_utility.test ${BG_UTILITY_TEST_SOURCE})
    
    target_link_libraries(playground_utility.test
        PRIVATE
            pg::utility
            Catch2::Catch2WithMain
    )

    catch_discover_tests(playground_utility.test)
endif()