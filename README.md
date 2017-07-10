# HiFive1-Korobeiniki

Quick demonstration of the PWM module for [SiFive's FE310 chip](https://www.sifive.com/products/freedom-e310/), as used on the [HiFive1 dev board](https://www.sifive.com/products/hifive1/).

[Demo video.](https://twitter.com/DavidGrubb18/status/829711362016342017)

## Usage

Checkout the [freedom-e-sdk](https://github.com/sifive/freedom-e-sdk "Freedom Everywhere SDK") and move
_korobeiniki_ to freedom-e-sdk/software (or clone directly in place). _Make_ as usual with the SDK to build and upload
to target development board:

```bash
dave@Prospero:~/freedom-e-sdk$ make software PROGRAM=korobeiniki BOARD=freedom-e300-hifive1
dave@Prospero:~/freedom-e-sdk$ make upload PROGRAM=korobeiniki BOARD=freedom-e300-hifive1
```

## Files

+ korobeiniki.c: Quick and dirty demonstration. Sets up PWM and adjusts frequency and duty cycle to generate musical tones.
+ Makefile: Adds functionality specific to this demo. Note that it's not standalone and relies on including other Makefiles from the SDK.
+ korobeiniki-demo-video.mp4: Shows the program in action with a corresponding oscilloscope trace.
