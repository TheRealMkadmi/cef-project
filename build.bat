cmake  -G "Visual Studio 16 2019" -A x64 -S . -B build ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_SYSTEM_VERSION=6.0 ^
  -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -DNTDDI_VERSION=0x06000000 ^
  -DUSE_SANDBOX=OFF -DUSE_ATL=OFF ^
  -DCEF_BUILD_TESTS=OFF ^
  && cmake --build build --config Release
