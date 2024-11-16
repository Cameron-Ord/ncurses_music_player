WORKING_DIR=$(cd "$(dirname "$0")" && pwd)
cd $WORKING_DIR

if [ -d "build" ]; then
  rm -r build
fi

mkdir build
cd build

cmake ..
make -j$(nproc)
