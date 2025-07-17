# Install
* Download LLVM pre-build package from https://github.com/SakuraEngine/llvm-build/releases and unzip to #LLVM_PATH#
* Use command
> xmake l setup.lua #LLVM_PATH#
> xmake f -m release -c
> xmake

# Run test
* Command like:
> xmake run EXAMPLE_NAME BACKEND_NAME
* for instance:
> xmake run test_path_tracing vk