#!/bin/bash

pwd=$(pwd)

echo "==> CONAN BUILDING <=="
echo ""

mkdir -p build
cd ./build/
conan build ..

echo ""
echo "==> CONAN BUILT <=="

cd $pwd
