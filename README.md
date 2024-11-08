# Nuked SC-55

Fork of [nukeykt/Nuked-SC55](https://github.com/nukeykt/Nuked-SC55) with
the optimization work of jcmoyer [jcmoyer/Nuked-SC55](https://github.com/jcmoyer/Nuked-SC55)
and GUI code work of mckuhei [mckuhei/Nuked-SC55](https://github.com/mckuhei/Nuked-SC55).

Differences from upstream:

- Produces a library for the emulator.
- Standard frontend supports routing to multiple emulators to raise polyphony
  limits.
- Includes a MIDI-to-WAVE rendererer.
- Adds tests so that the backend can be modified without worrying about
  breaking things.
- Command line is handled slightly differently. Pass `--help` to a binary to
  see what arguments it accepts.
- Improved performance without sacrificing accuracy.
- Added GUI support and LCD Contrast.
- Added volume adjustment.

## Building

See [BUILDING.md](BUILDING.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Nuked SC-55 can be distributed and used under the original MAME license (see
LICENSE file). Non-commercial license was chosen to prevent making and selling
SC-55 emulation boxes using (or around) this code, as well as preventing from
using it in the commercial music production.

