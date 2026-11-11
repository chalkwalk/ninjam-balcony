# Third-party code

The balcony itself is MIT ([LICENSE](LICENSE)). What it builds in, all as git
submodules, each with its own licence file in its own tree:

| Code | Where | Version | Licence |
|---|---|---|---|
| [Opus](https://opus-codec.org/) (xiph/opus) | `modules/opus` | v1.6.1 | BSD 3-clause (`modules/opus/COPYING`) |
| [libogg](https://xiph.org/ogg/) | `libs/ninjam/modules/ogg` | 1.3.6+ | BSD 3-clause (`COPYING` there) |
| [libvorbis](https://xiph.org/vorbis/) | `libs/ninjam/modules/vorbis` | 1.3.7+ | BSD 3-clause (`COPYING` there) |
| [chalkwalk-ninjam](https://github.com/chalkwalk/chalkwalk-ninjam) | `libs/ninjam` | pinned | MIT |
| [chalkwalk-dsp](https://github.com/chalkwalk/chalkwalk-dsp) | `libs/dsp` | pinned | MIT |
| [chalkwalk-jambot](https://github.com/chalkwalk/chalkwalk-jambot) (with chalkwalk-music) | `libs/jambot` | pinned | MIT |

The Xiph libraries' BSD licences ask that a binary distribution reproduce their
copyright notice and conditions: anyone shipping a built `balcony` ships those
three `COPYING` files with it. The web page (`web/`) uses no third-party code; it
decodes Opus with the browser's own WebCodecs.

No code comes from the reference NINJAM sources (justinfrankel/ninjam, WDL,
ReaNINJAM); see `chalkwalk-ninjam`'s README, *Provenance*.

Tests, not built into `balcony`: [Catch2](https://github.com/catchorg/Catch2) (BSL-1.0),
fetched by the libraries' test builds.
