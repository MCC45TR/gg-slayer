# ITE 8291 native LED integration

GPL-2.0-or-later TUXEDO `ite_8291.c`, imported from the locally installed
`ite8291r3-kmod` source (upstream revision 76b226bb). The upstream copyright
and license header are preserved. Original source SHA-256 is recorded below.

The Slayer changes use the unnumbered feature report declared by the USB HID
descriptor, combine per-key writes after a 16 ms quiet interval, preserve effects
when changing brightness, and expose a validated RAM-only hardware effect
endpoint so UPower sees the same brightness. R9T rev0003 transmits RGB unchanged;
other models retain their upstream white-balance quirks.

`controls/transfer_stats` reports successful complete frames, brightness commands
and the last transport error. No periodic polling or userspace RGB daemon is
required. USB still transports six rows; the controller has no verified atomic
frame-commit command. The update avoids repeatedly showing 126 partial frames.

Sources:
- https://github.com/tuxedocomputers/tuxedo-drivers/blob/master/src/ite_8291/ite_8291.c
- https://github.com/torvalds/linux/blob/master/drivers/hid/usbhid/hid-core.c

Original SHA-256: `590862a9816f2671e7ab17cd9810d120762d60e36e19df5a17c478cb974fb5ed`.
