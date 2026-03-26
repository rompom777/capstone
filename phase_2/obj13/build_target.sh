# go into xpdf directory
# ./build_target.sh <yourusername>


INSTR_PATH=/home/grads/$1/capstone/phase_2/obj13/instrumentation.c
TARGET_OUT_PATH=/home/grads/$1/capstone/phase_2/obj13/

XPDF_URL=https://dl.xpdfreader.com
XPDF_DIR=xpdf-4.06

curl -O ${XPDF_URL}/${XPDF_DIR}.tar.gz

tar -xzf ${XPDF_DIR}.tar.gz

clang -c $INSTR_PATH -o instrumentation.o
INSTR_O_PATH=$(realpath instrumentation.o)

cd ${XPDF_DIR}
mkdir build
cd build
pwd
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS="-fsanitize-coverage=inline-8bit-counters" \
    -DCMAKE_CXX_FLAGS="-fsanitize-coverage=inline-8bit-counters" \
    -DCMAKE_EXE_LINKER_FLAGS="$INSTR_O_PATH -Wl,--wrap=main" \
    ../

make

mv xpdf/pdftotext $TARGET_OUT_PATH/pdftotext

cd ../../

rm -rf $XPDF_DIR instrumentation.o $XPDF_DIR.tar.gz