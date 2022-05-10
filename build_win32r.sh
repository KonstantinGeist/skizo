mkdir tmp_build_win32r
cd tmp_build_win32r
cmake -DCMAKE_BUILD_TYPE=Release -G "CodeBlocks - MinGW Makefiles" ..
make -j4
cp skizo.exe ../bin/skizo.exe
cp src/third-party/tcc/libtcc.dll ../bin/libtcc.dll