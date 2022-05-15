mkdir tmp_build_unix32d
cd tmp_build_unix32d
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
cp skizo ../bin/skizo
cp src/third-party/tcc/libtcc.so ../bin/libtcc.so