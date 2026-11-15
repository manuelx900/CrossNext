#include "BookPreparationGeometry.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <Memory.h>

#include "../../components/CoverGridHomeUi.h"
#include "../home/HomeScreen.h"
#include "../home/LicoriceLayout.h"
#include "../library/LibraryControls.h"
#include "../ui/PixelMenuTheme.h"
namespace bookprep {
void collectCoverSizes(Job& job, GfxRenderer& renderer) {
  const auto orientation = renderer.getOrientation();
  for (const auto rotation : {GfxRenderer::Portrait, GfxRenderer::LandscapeClockwise, GfxRenderer::PortraitInverted,
                              GfxRenderer::LandscapeCounterClockwise}) {
    renderer.setOrientation(rotation);
    auto target = toybox::makeTarget(renderer);
    const auto device = target.deviceContext();
    job.addThumbnailHeight(CoverGridHomeUi::generatedThumbHeight(homeui::shelfLayout(device).artHeight));
    job.addThumbnailHeight(CoverGridHomeUi::preparationThumbHeight(renderer));
    for (const auto style : {homeui::Style::Framed, homeui::Style::Carousel, homeui::Style::Boox}) {
      // Home geometry exceeds the small stack budget; this cold allocation is
      // released before EPUB's parser starts, not retained alongside it.
      auto layout = makeUniqueNoThrow<homeui::Layout>(homeui::layout(device, 7, style));
      if (layout) {
        job.addThumbnailHeight(layout->cover.height);
        job.addThumbnailHeight(layout->sides[1].height);
      } else {
        LOG_ERR("PREP", "OOM: home cover geometry");
      }
    }
    job.addThumbnailHeight(libraryui::preparationCoverHeight(device, false));
    job.addThumbnailHeight(libraryui::preparationCoverHeight(device, true));
  }
  renderer.setOrientation(orientation);
  auto target = toybox::makeTarget(renderer);
  for (int n = 0; n < 2; ++n)
    job.nativeAreas[n] =
        libraryui::preparationGrid(target.deviceContext(), n != 0)
            .card(0)
            .inset(freeink::ui::Insets{pixelui::kBorder, pixelui::kBorder, pixelui::kBorder, pixelui::kBorder});
  job.addThumbnailHeight(400);
}
}  // namespace bookprep
