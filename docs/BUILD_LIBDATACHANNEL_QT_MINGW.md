# Build libdatachannel with Qt MinGW (no nanosleep64)

Qt’s MinGW (`C:\Qt\Tools\mingw1310_64`) must not see `C:\msys64\mingw64\include`
(that causes `wcstod` redefinition). Use OpenSSL **libs** from MSYS2, but **headers**
from a tiny copy that only contains `openssl/`.

## 1. Isolated OpenSSL headers (CMD)

```bat
mkdir C:\libs\openssl-headers\openssl 2>nul
xcopy /E /Y /I C:\msys64\mingw64\include\openssl C:\libs\openssl-headers\openssl
```

## 2. Configure + build (CMD)

```bat
set PATH=C:\Qt\Tools\mingw1310_64\bin;%PATH%
cd /d D:\libdatachannel

rmdir /s /q build-qtmingw
cmake -G "MinGW Makefiles" -B build-qtmingw ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX=C:\libs\libdatachannel-qtmingw ^
  -DCMAKE_C_COMPILER=C:/Qt/Tools/mingw1310_64/bin/gcc.exe ^
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe ^
  -DBUILD_SHARED_LIBS=ON ^
  -DUSE_GNUTLS=0 -DUSE_NICE=0 ^
  -DNO_TESTS=ON -DNO_EXAMPLES=ON -DNO_WEBSOCKET=ON ^
  -DOPENSSL_INCLUDE_DIR=C:/libs/openssl-headers ^
  -DOPENSSL_CRYPTO_LIBRARY=C:/msys64/mingw64/lib/libcrypto.dll.a ^
  -DOPENSSL_SSL_LIBRARY=C:/msys64/mingw64/lib/libssl.dll.a ^
  -DCMAKE_C_FLAGS=-Wno-error ^
  -DCMAKE_CXX_FLAGS=-Wno-error ^
  -DENABLE_WARNINGS_AS_ERRORS=OFF

cmake --build build-qtmingw -j%NUMBER_OF_PROCESSORS%
cmake --install build-qtmingw
```

## 3. Verify

```bat
dir C:\libs\libdatachannel-qtmingw\bin\libdatachannel.dll
C:\msys64\mingw64\bin\objdump.exe -p C:\libs\libdatachannel-qtmingw\bin\libdatachannel.dll | findstr /i nanosleep
```

No `nanosleep` lines = good.

## 4. QtMeet

`DATACHANNEL_DIR=C:\libs\libdatachannel-qtmingw` → Run qmake → Rebuild.

```bat
set BIN=D:\Work\Qt\qt-meet\build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug\bin
copy /y C:\libs\libdatachannel-qtmingw\bin\*.dll %BIN%\
copy /y C:\msys64\mingw64\bin\libssl-3-x64.dll %BIN%\
copy /y C:\msys64\mingw64\bin\libcrypto-3-x64.dll %BIN%\
```
