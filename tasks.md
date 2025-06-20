1) How to fix this error :
The CXX compiler identification is AppleClang 17.0.0.17000013
Detecting CXX compiler ABI info
Detecting CXX compiler ABI info - done
Check for working CXX compiler: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++ - skipped
Detecting CXX compile features
Detecting CXX compile features - done
CMake Deprecation Warning at build/_deps/cli11-src/CMakeLists.txt:1 (cmake_minimum_required):
  Compatibility with CMake < 3.10 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.


The C compiler identification is AppleClang 17.0.0.17000013
Detecting C compiler ABI info
Detecting C compiler ABI info - done
Check for working C compiler: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang - skipped
Detecting C compile features
Detecting C compile features - done
Found Git: /usr/bin/git (found version "2.39.5 (Apple Git-154)")
Performing Test CMAKE_HAVE_LIBC_PTHREAD
Performing Test CMAKE_HAVE_LIBC_PTHREAD - Success
Found Threads: TRUE
Warning: ccache not found - consider installing it for faster compilation or disable this warning with GGML_CCACHE=OFF
CMAKE_SYSTEM_PROCESSOR: arm64
GGML_SYSTEM_ARCH: ARM
Including CPU backend
Accelerate framework found
Could NOT find OpenMP_C (missing: OpenMP_C_FLAGS OpenMP_C_LIB_NAMES) 
Could NOT find OpenMP_CXX (missing: OpenMP_CXX_FLAGS OpenMP_CXX_LIB_NAMES) 
Could NOT find OpenMP (missing: OpenMP_C_FOUND OpenMP_CXX_FOUND) 
CMake Warning at build/_deps/llama_cpp-src/ggml/src/ggml-cpu/CMakeLists.txt:77 (message):
  OpenMP not found
Call Stack (most recent call first):
  build/_deps/llama_cpp-src/ggml/src/CMakeLists.txt:344 (ggml_add_cpu_backend_variant_impl)


ARM detected
Performing Test GGML_COMPILER_SUPPORTS_FP16_FORMAT_I3E
Performing Test GGML_COMPILER_SUPPORTS_FP16_FORMAT_I3E - Failed
ARM -mcpu not found, -mcpu=native will be used
Performing Test GGML_MACHINE_SUPPORTS_dotprod
Performing Test GGML_MACHINE_SUPPORTS_dotprod - Success
Performing Test GGML_MACHINE_SUPPORTS_i8mm
Performing Test GGML_MACHINE_SUPPORTS_i8mm - Success
Performing Test GGML_MACHINE_SUPPORTS_sve
Performing Test GGML_MACHINE_SUPPORTS_sve - Failed
Performing Test GGML_MACHINE_SUPPORTS_nosve
Performing Test GGML_MACHINE_SUPPORTS_nosve - Success
Performing Test GGML_MACHINE_SUPPORTS_sme
Performing Test GGML_MACHINE_SUPPORTS_sme - Failed
Performing Test GGML_MACHINE_SUPPORTS_nosme
Performing Test GGML_MACHINE_SUPPORTS_nosme - Success
ARM feature DOTPROD enabled
ARM feature MATMUL_INT8 enabled
ARM feature FMA enabled
ARM feature FP16_VECTOR_ARITHMETIC enabled
Adding CPU backend variant ggml-cpu: -mcpu=native+dotprod+i8mm+nosve+nosme 
Looking for dgemm_
Looking for dgemm_ - found
Found BLAS: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/Accelerate.framework
BLAS found, Libraries: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/Accelerate.framework
BLAS found, Includes: 
Including BLAS backend
Metal framework found
The ASM compiler identification is AppleClang
Found assembler: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
Including METAL backend
LLAMA_BUILD_COMMON is OFF, disabling LLAMA_CURL
Configuring done (44.1s)
CMake Error at CMakeLists.txt:37 (add_executable):
  Cannot find source file:

    src/Camus/Core.cpp

  Tried extensions .c .C .c++ .cc .cpp .cxx .cu .mpp .m .M .mm .ixx .cppm
  .ccm .cxxm .c++m .h .hh .h++ .hm .hpp .hxx .in .txx .f .F .for .f77 .f90
  .f95 .f03 .hip .ispc


CMake Error at CMakeLists.txt:37 (add_executable):
  No SOURCES given to target: camus