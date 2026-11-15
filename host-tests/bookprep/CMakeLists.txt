add_executable(BookPreparationTest test.cpp ${REPO_ROOT}/src/apps_local/bookprep/BookPreparation.cpp)
target_compile_features(BookPreparationTest PRIVATE cxx_std_20)
target_include_directories(BookPreparationTest PRIVATE stubs ${REPO_ROOT} ${REPO_ROOT}/lib/Memory ${REPO_ROOT}/lib/Epub)
add_test(NAME BookPreparationTest COMMAND BookPreparationTest)
