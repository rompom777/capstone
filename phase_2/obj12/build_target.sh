# go into xpdf directory
# ./build_target.sh <yourusername>


INSTR_PATH=/home/ugrads/majors/$1/capstone/capstone/phase_2/obj12/instrumentation.c
TARGET_OUT_PATH=/home/ugrads/majors/$1/capstone/capstone/phase_2/obj12/
OBJ12_PATH=/home/ugrads/majors/$1/capstone/capstone/phase_2/obj12/obj12.c

XPDF_URL=https://dl.xpdfreader.com
XPDF_DIR=xpdf-4.06

curl -O ${XPDF_URL}/${XPDF_DIR}.tar.gz

tar -xzf ${XPDF_DIR}.tar.gz

clang -c $INSTR_PATH -o instrumentation.o
clang -c $OBJ12_PATH -o obj12.o

ar rcs libfuzzer.a instrumentation.o obj12.o
LIB_PATH=$(realpath libfuzzer.a)

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
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,--wrap=main $LIB_PATH" \
    ../

make

mv xpdf/pdftotext $TARGET_OUT_PATH/pdftotext

cd ../../

rm -rf $XPDF_DIR instrumentation.o obj12.o libfuzzer.a $XPDF_DIR.tar.gz