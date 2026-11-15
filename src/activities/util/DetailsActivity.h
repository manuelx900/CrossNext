#pragma once

#include <memory>
#include <string>

#include "activities/Activity.h"
#include "components/UiAppHost.h"

// Content for the shared details page (fui::publicationPage layout).
struct DetailsSpec {
  std::string header;       // header band title
  std::string title;        // bold hero title
  std::string author;       // hero line under the title
  std::string source;       // hero line under the author (e.g. repository)
  std::string metadata;     // short facts (version, status, ...)
  std::string description;  // plain text, or HTML when descriptionHtml
  bool descriptionHtml = false;
  std::string actionLabel;  // primary button; confirming finishes with a non-cancelled result
};

// Title, metadata and the start of the description, with the primary action
// and More (full paged description through the existing dictionary viewer).
class DetailsActivity final : public Activity, private UiAppHost {
 public:
  DetailsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, DetailsSpec spec);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static void detailsScreen(UiScreen& screen, void* user);
  static void onPrimaryEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onMoreEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildScreen(UiScreen& screen);
  void confirm();
  void cancel();
  void openMore();

  DetailsSpec spec;
  // Kept off the render task stack: a CJK title's SD-font glyph load runs on
  // top of publicationPage(). buildScreen() refills it.
  freeink::ui::PublicationPageProps pageProps{};
  std::string summary;
};
