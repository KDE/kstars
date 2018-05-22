
ARGS1="-checks=*,-google-readability-casting,-google-readability-braces-around-statements,-readability-braces-around-statements"
ARGS2="-cppcoreguidelines-pro-type-vararg,-clang-diagnostic-unused-command-line-argument,-cppcoreguidelines-no-malloc"
ARGS3="-cert-msc30-c,-google-readability-todo,-cppcoreguidelines-pro-bounds-array-to-pointer-decay,-google-build-using-namespace"
ARGS4="-misc-macro-parentheses,-modernize-use-using,-llvm-include-order,-readability-static-definition-in-anonymous-namespace"
ARGS5="-cert-err58-cpp,-readability-else-after-return,-cppcoreguidelines-pro-bounds-pointer-arithmetic"
ARGS6="-cppcoreguidelines-pro-type-reinterpret-cast,-readability-redundant-member-init,-fuchsia-default-arguments"
ARGS7="-cppcoreguidelines-owning-memory,-hicpp-no-malloc,-hicpp-braces-around-statements,-hicpp-no-array-decay,-google-runtime-int"
# modernize-make-unique is skipped because we compile only with C++11 support
ARGS8="-readability-misleading-indentation,-hicpp-vararg,-modernize-make-unique,-hicpp-vararg,-cert-flp30-c"
ARGS9="-cppcoreguidelines-pro-bounds-constant-array-index,-cppcoreguidelines-pro-type-member-init,-hicpp-member-init"

# Configure CMake
cmake -Bclang_tidy_build -H.. -DCMAKE_CXX_CLANG_TIDY:STRING="clang-tidy-6.0;$ARGS1,$ARGS2,$ARGS3,$ARGS4,$ARGS5,$ARGS6,$ARGS7,$ARGS8,$ARGS9" \
      -DUNITY_BUILD=OFF -DCMAKE_C_COMPILER=clang-6.0 -DCMAKE_CXX_COMPILER=clang++-6.0 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $@

make -C clang_tidy_build -j3
