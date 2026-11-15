#include <Epub/blocks/ImageBlock.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <Epub/converters/JpegToFramebufferConverter.h>
#include <Epub/converters/PngToFramebufferConverter.h>
#include <GfxRenderer.h>
#include <HalStorage.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

ImageToFramebufferDecoder* ImageDecoderFactory::getDecoder(const std::string& path) {
  static JpegToFramebufferConverter jpeg;
  static PngToFramebufferConverter png;
  return path.ends_with(".png") ? static_cast<ImageToFramebufferDecoder*>(&png) : &jpeg;
}

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  unsigned checks = 0, failures = 0;
  const auto check = [&](bool result) {
    ++checks;
    if (!result) {
      ++failures;
      std::printf("Failed decoder check %u\n", checks);
    }
  };
  JpegToFramebufferConverter jpeg;
  PngToFramebufferConverter png;
  ImageToFramebufferDecoder* decoders[] = {&jpeg, &png};
  for (int n = 0; n < 2; ++n) {
    GfxRenderer renderer;
    renderer.frame.fill(0xa5);
    const auto unchanged = renderer.frame;
    auto cache = std::filesystem::temp_directory_path() / (n ? "crossnext-cancel.png.pxc" : "crossnext-cancel.jpg.pxc");
    RenderConfig config{};
    config.maxWidth = 32;
    config.maxHeight = 24;
    config.useExactDimensions = true;
    config.cacheOnly = true;
    config.cachePath = cache.string();
    unsigned calls = 0;
    config.workContext = &calls;
    config.continueWork = [](void* context) { return ++*static_cast<unsigned*>(context) < 3; };
    check(!decoders[n]->decodeToFramebuffer(argv[n + 1], renderer, config));
    check(calls >= 3);
    check(!std::filesystem::exists(cache));
    check(renderer.frame == unchanged);
    config.continueWork = nullptr;
    check(decoders[n]->decodeToFramebuffer(argv[n + 1], renderer, config));
    check(std::filesystem::file_size(cache) == 4 + 8 * 24);
    check(renderer.frame == unchanged);
    const auto read = [&]() {
      std::ifstream file(cache, std::ios::binary);
      return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    };
    const auto expected = read();
    check(decoders[n]->decodeToFramebuffer(argv[n + 1], renderer, config));
    check(read() == expected);
  }
  unsigned parityCases = 0;
  for (int n = 0; n < 2; ++n) {
    const std::string source = argv[n + 1];
    const std::string cache = source + ".32x24.pxc";
    ImageBlock image(source, "", 32, 24);
    const auto read = [&]() {
      std::ifstream file(cache, std::ios::binary);
      return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    };
    const int origins[][2] = {{1, 0}, {0, 1}, {3, 2}};
    for (int orientation = 0; orientation < 4; ++orientation) {
      for (const auto& origin : origins) {
        for (int mode = 0; mode < 3; ++mode) {
          ImageBlock::releaseRenderCache();
          ImageBlock::clearRenderFailures();
          std::remove(cache.c_str());
          GfxRenderer renderer;
          renderer.orientation = GfxRenderer::Orientation(orientation);
          renderer.mode = GfxRenderer::RenderMode(mode);
          renderer.dither = mode == GfxRenderer::BW;
          renderer.frame.fill(0xff);
          image.render(renderer, origin[0], origin[1]);
          check(std::filesystem::exists(cache));
          const auto expectedPixels = renderer.frame;
          const auto expectedCache = read();
          check(expectedCache.size() == 4 + 8 * 24);
          ImageBlock::releaseRenderCache();
          std::remove(cache.c_str());
          renderer.frame.fill(0xa5);
          const auto unchanged = renderer.frame;
          check(image.prepareCache(renderer, nullptr, nullptr, origin[0], origin[1]));
          check(renderer.frame == unchanged);
          check(read() == expectedCache);
          renderer.frame.fill(0xff);
          image.render(renderer, origin[0], origin[1]);
          check(renderer.frame == expectedPixels);
          ++parityCases;
        }
      }
    }
    ImageBlock::releaseRenderCache();
  }
  std::printf("Real JPEG/PNG cancellation: %u checks, %u failures\n", checks, failures);
  std::printf("Cold/prepared parity: %u cases at nonzero origins, BW and gray planes, four orientations\n",
              parityCases);
  return failures ? 1 : 0;
}
