# KDE authentication integration

Developer: MCC45TR <mcc45tr@gmail.com>

Research in progress. The existing `slayer-face-auth` tool is an explicit IR
Howdy enrollment/test frontend. Installation does not modify login policies.

Primary references:
- https://github.com/KDE/kscreenlocker/blob/master/README.pam
- https://github.com/sddm/sddm
- https://github.com/boltgolt/howdy

KScreenLocker uses PAM (normally the `kde` service). SDDM uses its own PAM
configuration. A face engine can participate through PAM; login, screen unlock,
privilege prompts and wallet unlock need separate verification.
