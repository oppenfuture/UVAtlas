# How to build

## Ubuntu 18.04 LTS
1. `sudo apt install clang`
2. Clone DirectXHeader
3. `mkdir DirectXHeader/build && cd DirectXHeader/build`
4. `cmake .. && make install`
5. Clone DirectXMath
6. `mkdir DirectXMath/Inc/build && cd DirectXMath/Inc/build`
7. `cmake .. && make install`
8. Clone DirectXMesh
9. `mkdir DirectXMesh/DirectXMesh/build && cd DirectXMesh/DirectXMesh/build`
10. `cmake .. && make install`
11. Clone UVAtlas
12. `mkdir UVAtlas/build && cd UVAtlas/build`
13. `cmake .. && make UVAtlasTool`

The executable should be in UVAtlas/build/bin.
