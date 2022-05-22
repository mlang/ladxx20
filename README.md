# An experiment in C++20 Linux Audio Development

## Components

- An audio processor (implemented as JACK client)
- A sample buffer class (with sndfile loading support)
- A simple interpreter to control programs

## Goals

- Keep user code as simple as possible
- Make use of C++20 features where appropriate
- Keep interface free from implementation details

## Dependencies

- CMake
- C++20 compiler (GCC 10 is fine)
- JACK
- libsndfile
