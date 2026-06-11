# CMU 15-445 BusTub Practice

This is a private practice repository based on the
[CMU-DB BusTub](https://github.com/cmu-db/bustub) educational database system.

It is used for learning C++, data structures, concurrency, and database system
implementation. This repository is not intended for course submission or
grading and should remain private.

## Upstream

The Git remotes are organized as follows:

```text
origin  -> private practice repository
public  -> official CMU-DB BusTub repository
```

Fetch upstream changes with:

```bash
git fetch public
```

Review the changes before merging them into a practice branch.

## Build

Install the required build tools, then configure an out-of-source build:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

For WSL environments where AddressSanitizer cannot start correctly, use a
separate release build:

```bash
mkdir -p build-release
cd build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Project 0

Build and run the Count-Min Sketch tests:

```bash
cd build-release
make -j$(nproc) count_min_sketch_test
./test/count_min_sketch_test
```

The implementation is located in:

```text
src/include/primer/count_min_sketch.h
src/primer/count_min_sketch.cpp
```

## Attribution

BusTub is developed by the Carnegie Mellon University Database Group for
educational use. See `LICENSE` for the upstream license.
