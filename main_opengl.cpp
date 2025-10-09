#include <iostream>
#include <vector>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

// Skia includes
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"

#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"

#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"


bool setupSkiaGL(int width, int height, sk_sp<GrDirectContext> &context, sk_sp<SkSurface> &surface) {
    sk_sp<const GrGLInterface> interface = GrGLMakeNativeInterface();
    context = GrDirectContexts::MakeGL(interface);
    if (!context) {
        std::cerr << "Failed to create Skia GrDirectContext for OpenGL!" << std::endl;
        return false;
    }

    int sampleCnt = 0;
    int stencilBits = 8;
    GrGLFramebufferInfo framebufferInfo;
    framebufferInfo.fFBOID = 0;
    framebufferInfo.fFormat = GL_RGBA8;
    GrBackendRenderTarget backendRT = GrBackendRenderTargets::MakeGL(
            width, height, sampleCnt, stencilBits, framebufferInfo);

    SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
    surface = SkSurfaces::WrapBackendRenderTarget(
        context.get(), backendRT, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, nullptr, &props);
    if (!surface) {
        std::cerr << "Failed to create SkSurface" << std::endl;
        return -1;
    }
    return true;
}

void drawFrame(SkCanvas* canvas) {
    glClear(GL_COLOR_BUFFER_BIT);
    canvas->clear(SK_ColorBLUE);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SK_ColorRED);

    SkPath triangle;
    triangle.moveTo(400, 100);
    triangle.lineTo(200, 500);
    triangle.lineTo(600, 500);
    triangle.close();

    canvas->drawPath(triangle, paint);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int width = 800, height = 600;
    GLFWwindow* window = glfwCreateWindow(width, height, "Skia OpenGL Example", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    sk_sp<GrDirectContext> context = nullptr;
    sk_sp<SkSurface> surface = nullptr;
    if (!setupSkiaGL(width, height, context, surface)) {
        std::cerr << "Setup failed!" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        drawFrame(surface->getCanvas());
        context->flushAndSubmit();

        glfwSwapBuffers(window);
    }
    
    surface.reset();
    context.reset();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}