# vcg_simplifier
VCG-Simplifier is a lightweight, texture-aware mesh simplification tool written in C++. It utilizes Quadric Error Metrics (QEM) to reduce polygon count for OBJ and GLB models while effectively preserving UV coordinates and visual fidelity.

## 🔨 Build / 编译

### Prerequisites (环境要求)
* **CMake**
* **C++ Compiler**
* **Git**

### Build Steps (构建步骤)

1. **Clone the repository** (Clone 仓库)
   ```bash
   git clone [https://github.com/HellAngel18/vcg_simplifier.git](https://github.com/HellAngel18/vcg_simplifier.git)
   cd vcg_simplifier
    ```
2. **Configure and Build** (配置并编译)
    ```bash
    cmake -B build

    cmake --build build --config Release
    ```
3. **Run the Simplifier** (运行简化器)
    ```bash
    cd build
    ./vcg-simplifier -i input.glb/obj -o output.glb/obj -r 0.5
    ```