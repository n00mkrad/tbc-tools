# ==========================================
# 1. 启用 CUDA 支持与架构配置
# ==========================================
# 在子模块中动态启用 CUDA 语言
enable_language(CUDA)

# 设定现代 GPU 架构 (75=20系, 86=30系, 89=40系)，防止 atomicAdd 报错
set(CMAKE_CUDA_ARCHITECTURES 75 86 89)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# ==========================================
# 2. 查找外部深度学习与 GPU 依赖
# ==========================================
find_package(CUDAToolkit REQUIRED)

find_path(ONNXRUNTIME_INCLUDE_DIR NAMES onnxruntime_cxx_api.h PATHS ${ONNXRUNTIME_ROOT}/include $ENV{ONNXRUNTIME_ROOT}/include)
find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime PATHS ${ONNXRUNTIME_ROOT}/lib $ENV{ONNXRUNTIME_ROOT}/lib)

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "Could not find ONNX Runtime. Please set ONNXRUNTIME_ROOT.")
endif()

# ==========================================
# 3. 原始构建逻辑 (融合 GPU 滤镜)
# ==========================================
# For M_PI constant
add_compile_definitions(_USE_MATH_DEFINES)

# (可选) 给程序加上你的自定义版本标记
add_compile_definitions(APP_BRANCH="nnTransform3DCUDA" APP_COMMIT="Independent")

add_library(lddecode-chroma STATIC
    decoder.cpp
    decoderpool.cpp
    comb.cpp
    componentframe.cpp
    framecanvas.cpp
    outputwriter.cpp
    palcolour.cpp
    sourcefield.cpp
    transformpal.cpp
    transformpal2d.cpp
    transformpal3d.cpp
    monodecoder.cpp
    
    # --- 新增的 GPU 核函数文件 ---
    nnTransform3D_kernel.cu
)

# 头文件包含路径 (新增 ONNX)
target_include_directories(lddecode-chroma PUBLIC 
    . 
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# 静态库链接 (新增 CUDA 和 ONNX)
target_link_libraries(lddecode-chroma PRIVATE 
    Qt::Core 
    ${FFTW_LIBRARY} 
    lddecode-library
    CUDA::cufft
    CUDA::cudart
    ${ONNXRUNTIME_LIBRARY}
)

target_include_directories(lddecode-chroma PRIVATE ${FFTW_INCLUDE_DIR})

# ==========================================
# 4. 可执行文件构建 (ld-chroma-decoder)
# ==========================================
add_executable(ld-chroma-decoder
    decoder.cpp
    decoderpool.cpp
    main.cpp
    monodecoder.cpp
    ntscdecoder.cpp
    paldecoder.cpp
)

set_target_properties(lddecode-chroma PROPERTIES AUTOMOC ON)

target_include_directories(ld-chroma-decoder PRIVATE ${FFTW_INCLUDE_DIR})

# 链接给可执行文件
target_link_libraries(ld-chroma-decoder PRIVATE Qt::Core lddecode-library lddecode-chroma)

install(TARGETS ld-chroma-decoder)