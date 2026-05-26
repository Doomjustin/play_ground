# 2. 包管理工具: vcpkg

vcpkg 是一个面向 C/C++ 的跨平台包管理工具，和 CMake 配合很好。

这一章目标：

1. 下载并初始化 vcpkg
2. 在项目中使用 Manifest 模式（`vcpkg.json`）
3. 让 CMake 自动安装并发现依赖

## 2.1 下载 vcpkg

### Linux / macOS

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

### Windows (PowerShell)

```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

初始化完成后，会在 vcpkg 根目录生成可执行文件：

- Linux/macOS: `./vcpkg`
- Windows: `vcpkg.exe`

## 2.2 建议设置环境变量

为了避免每次都写完整路径，建议设置 `VCPKG_ROOT`。

### Linux / macOS

```bash
export VCPKG_ROOT=/path/to/vcpkg
export PATH="$VCPKG_ROOT:$PATH"
```

可写入 `~/.bashrc` 或 `~/.zshrc` 持久化。

### Windows (PowerShell)

```powershell
setx VCPKG_ROOT "D:\tools\vcpkg"
```

## 2.3 使用 Manifest 模式

这个项目已经在仓库根目录使用了 `vcpkg.json`。

Manifest 模式的特点：

- 依赖与项目绑定，不依赖“全局已安装状态”
- 克隆仓库后即可复现依赖
- 版本可控（通过 `builtin-baseline`）

当前项目示例（节选）：

```json
{
	"name": "playground",
	"version": "0.0.1",
	"dependencies": [
		"fmt",
		"spdlog",
		"ms-gsl",
		"catch2",
		"magic-enum"
	]
}
```

新增依赖时，只需编辑 `vcpkg.json` 的 `dependencies`。

## 2.4 在 CMake 中引入 vcpkg

关键点是配置阶段传入 toolchain 文件。

### 推荐命令

```bash
cmake -S . -B build \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

然后正常编译：

```bash
cmake --build build
```

当 CMake 读取到 `vcpkg.json` 后，会自动触发依赖安装和解析。

## 2.5 CMakeLists.txt 中如何消费依赖

引入 vcpkg 后，依赖仍按标准 CMake 写法使用：

```cmake
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

target_link_libraries(your_target
		PRIVATE
				fmt::fmt
				spdlog::spdlog
				Microsoft.GSL::GSL
				magic_enum::magic_enum
)
```

本项目顶层 CMake 已经使用该模式，例如：

```cmake
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(glaze CONFIG REQUIRED)
find_package(ctre CONFIG REQUIRED)
```

## 2.6 本项目一键流程（Linux 示例）

```bash
git clone <your_repo_url>
cd play_ground

# 假设你已在系统中安装并设置好 VCPKG_ROOT
cmake -S . -B build \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build build
ctest --test-dir build --output-on-failure
```

## 2.7 常见问题

### 1) CMake 报找不到某个包

先确认：

- 配置命令里是否带了 `-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake`
- 该依赖是否在 `vcpkg.json` 的 `dependencies` 里
- 是否清理过旧缓存（删除 `build` 后重新配置）

### 2) Linux 下 `pkg-config` 相关报错

本项目还使用了：

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(liburing REQUIRED IMPORTED_TARGET liburing)
pkg_check_modules(jemalloc REQUIRED IMPORTED_TARGET jemalloc)
```

这两项依赖通常通过系统包管理器提供，请先安装对应开发包。

## 2.8 小结

接入 vcpkg 的核心只有一句话：

> 用 `vcpkg.json` 描述依赖，用 `CMAKE_TOOLCHAIN_FILE` 告诉 CMake 去哪里找 vcpkg。

之后项目就可以保持“依赖可复现、配置可迁移、构建流程统一”。
