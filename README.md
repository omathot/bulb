# Bulb

A project to checkout SDL3 GPU


## Build

Only tested on Fedora 44 beta. There seems to be some weird package conflict atm between SDL vendored and system, needs a couple extra tweaks to compile

For fedora 44 beta:
````
cmake -G Ninja -B build \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSDLIMAGE_VENDORED=ON \
    -DSDLTTF_VENDORED=ON \
    -DSDLIMAGE_TIF=OFF \
    -DSDLIMAGE_WEBP=OFF
````
I'm assuming stable fedora should compile without any of the 'DSDL...' options, but haven't tested there.

