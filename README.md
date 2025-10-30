[![CI](https://github.com/microsoft/arcana.cpp/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/microsoft/arcana.cpp/actions/workflows/ci.yml)

# Arcana.cpp

Arcana is a collection of general purpose C++ utilities with no code that is specific to a particular project or specialized technology area, sort of like an extension to the STL.  At present, the most notable of these utilities is the Arcana task library.

You can learn more about API usage in the [arcana.cpp documentation](Source/Arcana.md).

## Getting Started

1. Clone the repo and checkout the master branch.

### Prerequisites

- CMake 3.15 or higher
- A C++17 compatible compiler (Visual Studio 2019+, GCC 8+, or Clang 7+)

### Building with CMake

#### Configure and Build

From the root directory of the repository:

```cmd
# Configure the project
cmake -B Build
```

#### Build Options

- `ARCANA_TESTS`: Enable/disable building tests (default: ON if this is the top-level project)

#### Platform-Specific Examples

**Windows (Visual Studio)**
```cmd
cmake -B Build
start Build\arcana.cpp.sln
```

**macOS (Xcode)**
```zsh
cmake -B Build -G Xcode
open Build/arcana.cpp.xcodeproj
```

## Deployment

There is no official deployment mechanism available at this time.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct, and the process for submitting pull requests to us.

## Versioning

arcana.cpp does not use [SemVer](http://semver.org/). Instead, it uses a version derived from the current date. Therefore, the version contains no semantic information.

## Maintainers

With questions, please contact one of the maintainers:

- [Justin Murray](https://twitter.com/syntheticmagus)
- [Gary Hsu](https://twitter.com/bghgary)

## Credits

Arcana owes especial thanks to:

- [Julien Monat Rodier](https://github.com/jumonatr): project creator and primary developer/architect.
- [Ryan Tremblay](https://github.com/ryantrem): task system co-architect and creator of the coroutine system.

## Reporting Security Issues

Security issues and bugs should be reported privately, via email, to the Microsoft Security
Response Center (MSRC) at [secure@microsoft.com](mailto:secure@microsoft.com). You should
receive a response within 24 hours. If for some reason you do not, please follow up via
email to ensure we received your original message. Further information, including the
[MSRC PGP](https://technet.microsoft.com/en-us/security/dn606155) key, can be found in
the [Security TechCenter](https://technet.microsoft.com/en-us/security/default).

