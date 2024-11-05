# Contributing

Pull requests and issues are welcome. Before submitting a pull request, please
read the following sections entirely.

## Overview

One of the main features of this fork is to separate the upstream gui
application into two parts. The part that does the hardware emulation is the
backend, and the part that presents the emulator's output to the user is the
frontend.

### Backend

The backend is the emulator itself. Roughly speaking, these modules are
considered to be part of the backend:

- mcu and mcu_*
- submcu
- lcd
- pcm
- emu

Modifications to the backend should not add, change, or remove functionality.
These are enhancements to the emulator itself and should be first submitted
[upstream](https://github.com/nukeykt/Nuked-SC55). When new commits are merged
upstream, they will be cherry-picked and updated for this fork as necessary.

There are a couple reasons for this:

1. Most people are not using this fork, and it's best if everyone can benefit
   from your feature.
2. As new features are implemented upstream, they need to be integrated into
   this fork. If there are two implementations, we need to delete ours and
   replace it with upstream. If we add a feature and upstream implements it
   later, it creates extra work and more complex merges.

Optimizations to the backend are accepted as long as they can be shown to be
equivalent to the current behavior. The integration tests are designed to help
with this. See the [Development section of BUILDING](BUILDING.md#development)
for more information.

The only exceptions to this are cases where upstream does something
demonstrably incorrect like accessing an array out of bounds or invoking
undefined behavior. Fixing the bug and opening an issue or PR upstream is
appropriate in this case.

### Frontend

There are currently two frontends:

1. The standard frontend, a more feature-rich version of upstream's gui
   application
2. The renderer frontend, a command line tool for rendering MIDI files to WAVE
   files

It is permissible to add quality-of-life features to both of these, however:

- Changes to the standard frontend should preserve the same featureset as
  upstream.
- Changes to the renderer frontend must not change output in a way that causes
  the integration tests to fail. The renderer frontend is crucially important
  because it is used to test the backend deterministically.

## General advice

When submitting a PR please make the diff as clean as possible. Do not add
extra lines or whitespace. Small "fix minor thing in the previous commit" type
commits should be squashed. Follow the convention (naming, brace styles,
indentation) of surrounding code.

As a goal, all tests should pass for every commit on master. This is not a hard
rule, but having only functional commits makes finding bugs easier in some
cases.
