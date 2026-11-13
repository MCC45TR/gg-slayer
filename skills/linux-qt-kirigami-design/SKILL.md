---
name: linux-qt-kirigami-design
description: Design, implement, or review native Linux Qt Widgets and KDE Kirigami interfaces, including settings pages, navigation, themes, accessibility, and desktop UX. Use for Slayer Control UI work; backend-only changes do not need this skill.
metadata:
  version: "1.0"
---

# Native Linux Qt and Kirigami design

Create coherent desktop interfaces using KDE Human Interface Guidelines and the
application's existing architecture. Read the affected screen and its shared
components before changing its design; use existing screenshots as supporting
evidence rather than assuming they represent the current build.

## Project context

- `gui/main.cpp` is the standalone Qt Widgets application. Shared native widgets
  and layouts are in `gui/widgets.hpp`; `gui/backend.*` connects the UI to the
  existing application services.
- `kcm/ui/main.qml` is a Kirigami/QML module hosted by KDE System Settings, rooted
  in `KCM.SimpleKCM`. Preserve the host's navigation and window ownership.
- Inspect `CMakeLists.txt`, `kcm/CMakeLists.txt`, `README.md`, and the relevant UI
  notes in `docs/` for current dependencies and behavior. Use the installed
  Qt/KF/Kirigami versions when selecting APIs.
- A visual redesign does not itself require migrating Widgets to QML. Preserve
  the existing stack unless the requested scope includes an architectural change.

## Native visual language

- Use the desktop's font, icon theme, accent, and light/dark palette. In Widgets,
  reuse `QPalette`, `QIcon::fromTheme`, native controls, and existing `SlayerUi`
  components before adding custom painting or application-wide style rules.
- In Kirigami, use `Kirigami.Theme` color roles and `Kirigami.Units` spacing and
  sizing. Use `Kirigami.Heading` for headings and Qt Quick Controls labels for
  normal text. Derive hierarchy from the system font rather than fixing a new
  typeface or a pixel-only typography system.
- Preserve KDE's runtime control style. Generic QML advice about forcing Basic,
  Material, or another style is not a reason to replace native KDE controls.
- Prefer `Kirigami.FormLayout` for related settings. Use Kirigami Addons FormCard
  components when they fit the screen and the required dependency is present or
  justified; they are not a mandatory replacement for every form or KCM root.
- Group controls by the user's task. Keep page titles, group labels, and primary
  actions consistent across the Widgets application and KCM. Use cards or
  separators where they clarify grouping, without framing every individual row.
- Let layouts reflow at narrow widths and with larger system fonts. Give long
  Turkish labels room to wrap. In QML, let `Layout.*` size layout-managed items;
  reserve explicit geometry for justified visuals such as the keyboard drawing.

## Settings and hardware interaction

- Distinguish the current device state, edits awaiting application, saved
  profiles, and settings that take effect after restart. Label actions according
  to their actual effect; saving a profile and applying it are separate actions.
- Preserve the existing explicit Apply behavior for hardware changes. Previewing
  a color, moving a slider, navigating, or opening a screen must not accidentally
  issue hardware writes. Use the existing backend and capability checks.
- Present busy, successful, failed, unsupported, and unavailable states clearly.
  Pair state colors with text or icons and provide a useful reason when a
  capability is blocked. Keep missing sensor values distinct from a real zero.
- Keep the UI unprivileged and preserve existing helper/polkit boundaries. Use
  fixture data for interaction testing that could otherwise affect real devices.
- Preserve the lightweight runtime: add refreshes, polling, animation, or new
  services only when the requested behavior needs them.

## Accessibility and verification

Keep keyboard focus visible and ordered, label icon-only and custom controls,
and make hover help available from the keyboard. Preserve existing shortcuts.
Treat the custom keyboard as an interactive control with accessible selection
and navigation, not solely as a decorative drawing.

For substantial visual changes, inspect the affected screens in light and dark
themes, at a normal and a narrow supported window size, with increased font
scaling. Check clipping, scrolling, focus order, and operation feedback. Use the
project's relevant UI fixtures and build checks; inspect its current CMake/Make
targets rather than inventing commands. Small text or spacing edits need checks
proportionate to their scope. Report what was actually verified and any runtime
or hardware behavior that remains untested.

## Authoritative references

Consult the relevant official page when a component or convention is uncertain:

- [KDE HIG: layout and navigation](https://develop.kde.org/hig/layout_and_nav/)
- [KDE HIG: text and labels](https://develop.kde.org/hig/text_and_labels/)
- [KDE HIG: input controls](https://develop.kde.org/hig/getting_input/)
- [Kirigami form layouts](https://develop.kde.org/docs/getting-started/kirigami/components-formlayouts/)
- [Kirigami colors](https://develop.kde.org/docs/getting-started/kirigami/style-colors/)
- [Kirigami Addons form delegates](https://develop.kde.org/docs/getting-started/kirigami/addons-delegates/)
