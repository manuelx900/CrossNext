"""Exercise the production project-confirmation branch with guarded-save adapters."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(sys.argv[1]).resolve()
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / "src/apps_local/pet/PetActivity.cpp").read_text("utf-8")
start = source.index("    case petui::SelectProject:")
end = source.index("    case petui::Page:", start)
branch = source[start:end]
program = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include "src/apps_local/pet/PetCatalogView.h"
struct Selection {
  pet::Profile profile, draft;
  pet::View view = pet::View::ProjectPreview;
  petui::CatalogFilter catalogFilter = petui::CatalogFilter::Unowned;
  bool writable = true, failSave = false;
  int projectSlot = 1, page = 8;
  unsigned saves = 0;
  uint8_t previewItem = uint8_t(pet::objectAt(1, 40));
  struct Renderer {
    int width = 480;
    int getScreenWidth() const { return width; }
    int getScreenHeight() const { return width == 480 ? 800 : 480; }
  } renderer;
  Selection() {
    profile.created = true;
    profile.readingSeconds = 72000;
    pet::own(profile, pet::objectAt(1, 0));
    pet::own(profile, pet::objectAt(1, 2));
    profile.equipped[1] = uint8_t(pet::objectAt(1, 0));
  }
  bool commit() { ++saves; if (failSave) return false; profile = draft; return true; }
  void show(pet::View next) { view = next; }
  void changeTab(int) { view = pet::View::Home; }
  void select(int value, freeink::ui::ActionId action = petui::SelectProject) {
    draft = profile;
    switch (action) {
BRANCH
      default: return;
    }
  }
};
int main() {
  for (int width : {480, 800}) {
    Selection instant;
    instant.renderer.width = width;
    instant.select(instant.previewItem);
    assert(instant.saves == 1 && !pet::owns(instant.profile, instant.previewItem));
    assert(pet::projectRemaining(instant.profile) == pet::projectRequiredSeconds(2));
    assert(instant.view == pet::View::ProjectPreview && instant.catalogFilter == petui::CatalogFilter::Unowned);
    assert(pet::addReading(instant.profile, instant.profile.readingSeconds + pet::projectRemaining(instant.profile)));
    assert(pet::owns(instant.profile, instant.previewItem));
    assert(instant.profile.equipped[1] == instant.previewItem);
    assert(!petui::catalogMatches(instant.profile, instant.previewItem, instant.catalogFilter));
    Selection pending;
    pending.renderer.width = width;
    pending.profile.readingSeconds = 1200;
    pending.select(pending.previewItem);
    assert(pending.saves == 1 && pending.view == pet::View::ProjectPreview);
    assert(!pet::owns(pending.profile, pending.previewItem));
    assert(pending.profile.project == pending.previewItem);
    assert(pending.catalogFilter == petui::CatalogFilter::Unowned);
    const auto ordinal = petui::catalogOrdinal(pending.profile, 1, pending.catalogFilter, pending.previewItem);
    assert(ordinal < pet::OBJECTS_PER_SLOT && pending.page == int(ordinal / (width == 480 ? 6 : 4)));
    Selection owned;
    owned.renderer.width = width;
    pet::own(owned.profile, owned.previewItem);
    owned.catalogFilter = petui::CatalogFilter::Owned;
    owned.select(owned.previewItem);
    assert(owned.saves == 1 && owned.catalogFilter == petui::CatalogFilter::Owned && owned.page == 0);
    assert(owned.profile.equipped[1] == owned.previewItem);
    const auto placed = owned.profile;
    auto removed = placed;
    removed.equipped[1] = 255;
    owned.select(owned.previewItem, petui::RemoveProject);
    assert(owned.saves == 2 && owned.view == pet::View::Home);
    assert(!std::memcmp(&removed, &owned.profile, sizeof(removed)));
    assert(pet::owns(owned.profile, owned.previewItem));
    assert(petui::catalogItem(owned.profile, 1, petui::CatalogFilter::Owned, 0) == pet::objectAt(1, 0));
    uint8_t bytes[pet::STATE_BYTES];
    pet::encode(owned.profile, bytes);
    pet::Profile loaded;
    assert(pet::decode(bytes, loaded, sizeof(bytes)) && loaded.equipped[1] == 255 && pet::owns(loaded, owned.previewItem));
    owned.view = pet::View::ProjectPreview;
    owned.select(owned.previewItem, petui::RemoveProject);
    assert(owned.saves == 2);
    owned.select(owned.previewItem);
    assert(owned.saves == 3 && owned.profile.equipped[1] == owned.previewItem);
    owned.failSave = true;
    const auto beforeFailedRemove = owned.profile;
    owned.select(owned.previewItem, petui::RemoveProject);
    assert(owned.saves == 4 && !std::memcmp(&beforeFailedRemove, &owned.profile, sizeof(owned.profile)));
    owned.failSave = false;
    owned.writable = false;
    owned.select(owned.previewItem, petui::RemoveProject);
    assert(owned.saves == 4 && owned.profile.equipped[1] == owned.previewItem);
    assert(!pet::unequip(loaded, pet::OBJECT_COUNT));
    Selection failed;
    failed.failSave = true;
    const auto before = failed.profile;
    failed.select(failed.previewItem);
    assert(failed.saves == 1 && failed.catalogFilter == petui::CatalogFilter::Unowned);
    assert(!std::memcmp(&before, &failed.profile, sizeof(before)));
    Selection readonly;
    readonly.writable = false;
    readonly.select(readonly.previewItem);
    assert(!readonly.saves && readonly.catalogFilter == petui::CatalogFilter::Unowned);
    Selection stale;
    stale.select(stale.previewItem + 1);
    assert(!stale.saves);
  }
  std::puts("PET selection: fresh project budgets, completion/owned, remove/restore, persisted empty slot, failed saves, read-only and stale input passed in both layouts");
}
'''.replace("BRANCH", branch)
(OUT / "selection.cpp").write_text(program, "utf-8")
wasm = os.name == "nt"
compiler = ROOT / ".venv/emsdk/upstream/emscripten/em++.exe" if wasm else "c++"
binary = OUT / ("selection.cjs" if wasm else "selection")
flags = ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sEXIT_RUNTIME=1"] if wasm else []
subprocess.run([str(compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I.",
                "-Ifreeink-sdk/libs/ui/FreeInkUI/include", "-Ifreeink-sdk/libs/assets/Icons/include",
                "-Ilib/EpdFont", "-Ilib/I18n", str(OUT / "selection.cpp"),
                "src/apps_local/pet/PetState.cpp", *flags, "-o", str(binary)], cwd=ROOT, check=True)
command = [str(ROOT / ".venv/emsdk/node/24.19.0_64bit/node.exe")] if wasm else []
subprocess.run([*command, str(binary)], cwd=ROOT, check=True)
