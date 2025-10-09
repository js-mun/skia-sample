#include <GLFW/glfw3.h>
#include "include/gpu/ganesh/GrDirectContext.h"

#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/core/SkColorSpace.h"

#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include <GL/gl.h>

#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkStream.h"
#include "include/core/SkPath.h"

#include <iostream>

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Skia OpenGL Triangle", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // ✅ Skia OpenGL backend 연결
    sk_sp<const GrGLInterface> interface = GrGLMakeNativeInterface();
    sk_sp<GrDirectContext> context = GrDirectContexts::MakeGL(interface);
    if (!context) {
        std::cerr << "Failed to create GrDirectContext\n";
        return -1;
    }

    int width = 800, height = 600;
    int sampleCnt = 0;
    int stencilBits = 8;

    GrGLFramebufferInfo fbInfo;
    fbInfo.fFBOID = 0; // 기본 framebuffer
    fbInfo.fFormat = GL_RGBA8; // framebuffer format

    GrBackendRenderTarget backendRT =
        GrBackendRenderTargets::MakeGL(width, height, sampleCnt, stencilBits, fbInfo);

    SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
    SkColorType colorType = kRGBA_8888_SkColorType;
    sk_sp<SkSurface> surface = SkSurfaces::WrapBackendRenderTarget(
        context.get(), backendRT, kBottomLeft_GrSurfaceOrigin, colorType, nullptr, &props);
    if (!surface) {
        std::cerr << "Failed to create SkSurface\n";
        return -1;
    }

    SkCanvas* canvas = surface->getCanvas();

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        canvas->clear(SK_ColorWHITE);

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);
        paint.setColor(SK_ColorRED);

        // 🔺 빨간 삼각형
        SkPath triangle;
        triangle.moveTo(400, 100);
        triangle.lineTo(200, 500);
        triangle.lineTo(600, 500);
        triangle.close();

        canvas->drawPath(triangle, paint);

        // 스왑 버퍼 전 flush
        context->flush();
        context->submit();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    context->releaseResourcesAndAbandonContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
