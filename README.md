# libkinguard

libkinguard provides the high level system library used by the Kinguard Project platform. It offers C++ manager classes that simplify configuring and maintaining a unit.

## Features

- **Identity management** – set host names and domains, manage DNS registration and certificates.
- **User management** – create and update users, groups and passwords stored in the Secop authentication database.
- **Mail management** – configure domains, local and remote mail addresses and aliases.
- **Network management** – query and apply interface settings through libopi network helpers.
- **Storage management** – discover disks, configure LUKS/LVM layouts and mount devices.
- **Backup management** – configure backup parameters and trigger or restore backups.
- **System management** – check for upgrades, enable shell access and other system level tasks.

All classes live in the `KGP` namespace and derive from `BaseManager` to provide a unified error reporting interface.

## Building

This project uses CMake and depends on:

- [libopi](https://github.com/openproducts/libopi) ≥ 1.6.60
- [libutils](https://github.com/openproducts/libutils) ≥ 1.5.19
- [nlohmann/json](https://github.com/nlohmann/json) (header-only)
- [CppUnit](https://sourceforge.net/projects/cppunit/) for tests

Example build:

```sh
cmake -S . -B build
cmake --build build
```

## Running tests

After building the project, run the test suite with:

```sh
ctest --test-dir build
```

## License

libkinguard is licensed under the GNU Affero General Public License v3.0. See [COPYING](COPYING) for the full license text.

