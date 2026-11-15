#pragma once
#include <memory>
#include <string>
#include <vector>
class GfxRenderer;
inline unsigned preparedImages = 0;
inline bool failImages = false;
enum PageElementTag { TAG_PageImage = 2 };
class ImageBlock {
 public:
  bool prepareCache(GfxRenderer&, void*, bool (*)(void*, const char*, const char*)) const {
    ++preparedImages;
    return !failImages;
  }
  const std::string& getImagePath() const {
    static const std::string p = "image.png";
    return p;
  }
};
class PageImage {
  ImageBlock image;

 public:
  PageElementTag getTag() const { return TAG_PageImage; }
  const ImageBlock& getImageBlock() const { return image; }
};
class Page {
 public:
  std::vector<std::unique_ptr<PageImage>> elements;
  Page() {
    elements.reserve(1);
    elements.push_back(std::make_unique<PageImage>());
  }
};
