# Project skills

This repository keeps its skills in `skills/`. They are project-local guidance,
not entries in the user's global skill catalog. When a request matches a skill
below, read its `SKILL.md` and follow the relevant workflow. Resolve its
relative reference and script paths from that skill's directory.

| Skill | When to use | Entry point |
| --- | --- | --- |
| `linux-qt-kirigami-design` | Design, implement, or assess the native Linux UI and its KDE integration | `skills/linux-qt-kirigami-design/SKILL.md` |
| `qt-ui-design` | Screen layouts, navigation, visual hierarchy, accessibility, or UX audits | `skills/qt-ui-design/SKILL.md` |
| `qt-qml` | Write, fix, or refactor QML components, layouts, or bindings | `skills/qt-qml/SKILL.md` |
| `qt-qml-review` | Requested code review of Qt/QML changes or components | `skills/qt-qml-review/SKILL.md` |
| `qt-cpp-review` | Requested code review of Qt/C++ changes or components | `skills/qt-cpp-review/SKILL.md` |

For native UI work, use the local Kirigami skill for project and KDE conventions,
then load the Qt skill relevant to the task. Preserve the platform's existing
style and design tokens when generic guidance offers different defaults.
Read-only review skills do not authorize fixes or unrelated changes.

The four upstream Qt skill directories are unmodified imports from
`TheQtCompanyRnD/agent-skills`. Their source revision and file hashes are recorded
in `skills/qt-upstream.json`; each directory includes its upstream license.
