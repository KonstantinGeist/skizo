mkdir tmp_build_win32d
cd tmp_build_win32d
cmake -DCMAKE_BUILD_TYPE=Debug -G "CodeBlocks - MinGW Makefiles" ..
make -j4
cp skizo.exe ../bin/skizo.exe
cp src/third-party/tcc/libtcc.dll ../bin/libtcc.dll