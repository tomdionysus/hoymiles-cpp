# hoymiles-cpp

A protobuf based library to communicate with Hoymiles microinverter devices

## Build

```sh
mkdir build
cd build
cmake ..
make
```

## Demo

```sh
hoymiles_cpp --ip 192.168.1.193 info
```

## Format / Lint

```sh
cmake --build build --target format
cmake --build build --target lint
```

## Credits

* https://github.com/henkwiedig/Hoymiles-DTU-Proto