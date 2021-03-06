cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MSVC_Generate_Profiling OFF)
set(SIMD_ISA_MSVC "AVX" CACHE STRING "SIMD Optimization Architecture for MSVC")
set(SIMD_ISA_GCClang "avx" CACHE STRING "SIMD Optimization Architecture for GCC or Clang")

project(IV1Compressor)
	find_package(PNG REQUIRED)
	include_directories(${PNG_INCLUDE_DIR})

	add_executable(IV1Compressor 
		IV1enc.cpp
		Support/PNGLoader.h
		Support/PNGLoader.cpp
		VQLib/C++/VQDataTypes.h
		VQLib/C++/VQArithmetic.h
		VQLib/C++/VQAlgorithm.h)

	target_link_libraries(IV1Compressor ${PNG_LIBRARY})
	if (MSVC)
		target_compile_options(IV1Compressor PRIVATE /arch:${SIMD_ISA_MSVC})
		if (MSVC_Generate_Profiling)
			target_link_options(IV1Compressor PRIVATE /PROFILE)
		endif()
	else()
		target_compile_options(IV1Compressor PRIVATE -m${SIMD_ISA_GCClang} -ffast-math)
	endif()

project(IV1Decompressor)
	find_package(PNG REQUIRED)
	include_directories(${PNG_INCLUDE_DIR})

	add_executable(IV1Decompressor 
		IV1dec.cpp
		IV1BlockImage.h
		IV1File.h
		Support/PNGLoader.h
		Support/PNGLoader.cpp
		VQLib/C++/VQDataTypes.h
		VQLib/C++/VQArithmetic.h
		VQLib/C++/VQAlgorithm.h)

	target_link_libraries(IV1Decompressor ${PNG_LIBRARY})
	if (MSVC)
		target_compile_options(IV1Decompressor PRIVATE /arch:${SIMD_ISA_MSVC})
		if (MSVC_Generate_Profiling)
			target_link_options(IV1Decompressor PRIVATE /PROFILE)
		endif()
	else()
		target_compile_options(IV1Decompressor PRIVATE -m${SIMD_ISA_GCClang} -ffast-math)
	endif()

project(IV1Roundtrip)
	find_package(PNG REQUIRED)
	include_directories(${PNG_INCLUDE_DIR})

	add_executable(IV1Roundtrip 
		IV1round.cpp
		IV1BlockImage.h
		IV1File.h
		Support/PNGLoader.h
		Support/PNGLoader.cpp
		VQLib/C++/VQDataTypes.h
		VQLib/C++/VQArithmetic.h
		VQLib/C++/VQAlgorithm.h)

	target_link_libraries(IV1Roundtrip ${PNG_LIBRARY})
	if (MSVC)
		target_compile_options(IV1Roundtrip PRIVATE /arch:${SIMD_ISA_MSVC})
		if (MSVC_Generate_Profiling)
			target_link_options(IV1Roundtrip PRIVATE /PROFILE)
		endif()
	else()
		target_compile_options(IV1Roundtrip PRIVATE -m${SIMD_ISA_GCClang} -ffast-math)
	endif()

project(IV1DictViewer)
	find_package(PNG REQUIRED)
	include_directories(${PNG_INCLUDE_DIR})

	add_executable(IV1DictView 
		IV1dictview.cpp
		IV1BlockImage.h
		IV1File.h
		Support/PNGLoader.h
		Support/PNGLoader.cpp
		VQLib/C++/VQDataTypes.h
		VQLib/C++/VQArithmetic.h
		VQLib/C++/VQAlgorithm.h)

	target_link_libraries(IV1DictView ${PNG_LIBRARY})
	if (MSVC)
		target_compile_options(IV1DictView PRIVATE /arch:${SIMD_ISA_MSVC})
		if (MSVC_Generate_Profiling)
			target_link_options(IV1DictView PRIVATE /PROFILE)
		endif()
	else()
		target_compile_options(IV1DictView PRIVATE -m${SIMD_ISA_GCClang} -ffast-math)
	endif()