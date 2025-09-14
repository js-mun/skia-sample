#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFont.h"
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

    SkFont font;
    paint.setColor(SK_ColorWHITE);
    canvas->drawString("Hello Skia!", 100, 100, font, paint);

    std::cout << "Skia CPU sample rendered successfully." << std::endl;
    return 0;
}