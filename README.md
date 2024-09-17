# Packed-ADFA

This repository contains the implementation  for Packed ADFA.

## Datasets
The datasets used in the paper are described in [dataset.md](https://github.com/shibh308/Packed_ADFA/blob/main/data/dataset.md).

## Build

You can use [cmake](https://cmake.org/) to build the code.
This project requires [sdsl](https://github.com/simongog/sdsl-lite/tree/master).
You should install sdsl and set `SDSL_INCLUDE_DIR` and `SDSL_LIBRARY_DIR` in `CMakeLists.txt`
After completing the above steps, you can build the project using the following commands:
```
cmake . -DCMAKE_BUILD_TYPE=Release
make
```

## Run Benchmarks
To run benchmark, use the following command:
```
./Packed_ADFA {dataset_name} {dataset_size (optional)}
```
The command reads a dictionary from `../data/{dataset_name}` and executes benchmark for each trie/ADFA.
If `{dataset_size}` is specified, the program extracts first `{dataset_size}` bytes (default=`1e9`).
