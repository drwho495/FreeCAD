# Stub for the Arch_rc Qt resource module (wasm build).
#
# The real Arch_rc is generated from Arch.qrc and bundles ~45MB of icons plus the
# ":/ui/preferences-*.ui" preference pages. It is intentionally omitted from the
# WebAssembly package to keep the download small. Many BIM command GetResources()
# methods (and a couple of modules) do a bare `import Arch_rc` purely for the side
# effect of registering the ":/icons" Qt resources. Without the module those
# imports raise ModuleNotFoundError, which aborts `import bimcommands` and therefore
# BIM workbench Initialize()/activation.
#
# This stub lets those imports succeed. Consequences on wasm:
#   * ":/icons/..." command pixmaps are unavailable (cosmetic; missing icons).
#   * Workbench-selector icons still resolve — they are loaded from disk via the
#     /freecad/share/Mod/<M> resource symlink bridge (see index.html preRun).
#   * The ":/ui" preference pages are loaded from disk by
#     draftutils.params._read_ui_from_disk(), so all Arch param defaults are intact.
#
# No symbols are exported: every real reference is `import Arch_rc` followed by the
# use of ":/..." resource path strings, never an Arch_rc attribute.
