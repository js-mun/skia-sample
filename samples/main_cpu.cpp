#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFont.h"
#include "include/core/SkImage.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFontMgr.h"
#include "include/encode/SkPngEncoder.h"
#include "include/core/SkStream.h"
#include "include/core/SkTextBlob.h"
#include <iostream>

int main() {
    // SkSurfaces::Raster 함수는 SkSurface.h에 포함되어 있습니다.
    SkImageInfo info = SkImageInfo::MakeN32Premul(800, 600);
    
    // SkSurfaces::Raster를 직접 호출합니다.
    auto surface = SkSurfaces::Raster(info);
    if (!surface) {
        std::cerr << "Failed to create raster surface." << std::endl;
        return 1;
    }
    
    SkCanvas* canvas = surface->getCanvas();

    SkPaint paint;
    paint.setColor(SK_ColorBLUE);
    paint.setAntiAlias(true);
    canvas->drawRect(SkRect::MakeXYWH(50, 50, 300, 200), paint);

    SkPath triangle;
    triangle.moveTo(200, 300);
    triangle.lineTo(100, 450);
    triangle.lineTo(300, 450);
    triangle.close();
    paint.setColor(SK_ColorRED);
    canvas->drawPath(triangle, paint);

    std::cout << "Skia CPU sample rendered successfully." << std::endl;

    sk_sp<SkImage> image = surface->makeImageSnapshot();
    if (!image) {
        std::cerr << "Failed to create image snapshot." << std::endl;
        return 1;
    }

    SkFILEWStream file("output_skia_sample.png");
    if (!file.isValid()) {
        std::cerr << "Failed to open output file." << std::endl;
        return 1;
    }
    SkPngEncoder::Options options;
    options.fZLibLevel = 9;
    options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;
    // GrDirectContext 없는 CPU용 버전
    auto pngData = SkPngEncoder::Encode(nullptr, image.get(), options);
    if (pngData) {
        file.write(pngData->data(), pngData->size());
        std::cout << "PNG saved successfully!" << std::endl;
    } else {
        std::cerr << "Failed to encode PNG." << std::endl;
        return 1;
    }

    return 0;
}