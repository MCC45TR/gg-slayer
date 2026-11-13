`sign-file.c` and `ssl-common.h` are unmodified Linux upstream scripts under
LGPL-2.1-or-later, retrieved from https://github.com/torvalds/linux/tree/master/scripts
on 2026-11-14. They are compiled on COPR as `slayerctl-module-sign`.

The RPM post-transaction helper signs only its two fixed module paths with an
existing root-only akmods key. It never creates or enrolls a key, reads the key
into a frontend, compiles a module, or changes Secure Boot configuration.
