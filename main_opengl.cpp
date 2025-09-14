#include <iostream>
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Skia includes
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkColor.h"

// Skia 전역 변수
sk_sp<GrDirectContext> skiaContext = nullptr;
sk_sp<SkSurface> skiaSurface = nullptr;

// OpenGL 초기화 및 GrDirectContext 생성
bool setupOpenGLAndSkia(GLFWwindow* window) {
    // glfwMakeContextCurrent(window);

    // GLenum err = glewInit();
    // if (GLEW_OK != err) {
    //     std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
    //     return false;
    // }

    // auto getProc = [](const char* procName) -> GrGLFuncPtr {
    //     return (GrGLFuncPtr)glfwGetProcAddress(procName);
    // };
    // sk_sp<const GrGLInterface> glInterface = GrGLMakeNativeInterface();
    
    // skiaContext = GrDirectContext::MakeGL(glInterface);
    // if (!skiaContext) {
    //     std::cerr << "Failed to create Skia GrDirectContext for OpenGL!" << std::endl;
    //     return false;
    // }

    // int width, height;
    // glfwGetFramebufferSize(window, &width, &height);

    // GrGLFramebufferInfo framebufferInfo;
    // framebufferInfo.fFBOID = 0;
    // framebufferInfo.fFormat = GL_RGBA8;

    // GrBackendRenderTarget backendRT(width, height, 0, 8, framebufferInfo);

    // // 변경된 부분: SkSurfaces::WrapBackendRenderTarget 사용
    // skiaSurface = SkSurfaces::WrapBackendRenderTarget(
    //     skiaContext.get(),
    //     backendRT,
    //     kBottomLeft_GrSurfaceOrigin,
    //     kRGBA_8888_SkColorType
    // );

    // if (!skiaSurface) {
    //     std::cerr << "Failed to create Skia SkSurface!" << std::endl;
    //     return false;
    // }

    return true;
}

void drawFrame(GLFWwindow* window) {
    if (!skiaSurface) return;

    SkCanvas* canvas = skiaSurface->getCanvas();
    if (canvas) {
        canvas->clear(SK_ColorBLUE);
        SkPaint paint;
        paint.setColor(SK_ColorRED);
        SkRect rect = SkRect::MakeXYWH(100.0f, 100.0f, 200.0f, 150.0f);
        canvas->drawRect(rect, paint);
    }
    
    skiaContext->flushAndSubmit();
    glfwSwapBuffers(window);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Skia OpenGL Example", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }
    
    if (!setupOpenGLAndSkia(window)) {
        std::cerr << "Setup failed!" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame(window);
    }
    
    skiaSurface.reset();
    skiaContext.reset();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}