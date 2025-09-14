#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkColor.h"
#include "include/gpu/vk/VulkanBackendContext.h"
#include "include/gpu/ganesh/vk/GrVkTypes.h"

// #include "include/gpu/vk/GrVkImageInfo.h"
#include "include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "include/core/SkSurface.h"



// Vulkan 전역 변수
VkInstance instance;
VkPhysicalDevice physicalDevice;
VkDevice device;
VkQueue graphicsQueue;
uint32_t graphicsQueueIndex;
VkSurfaceKHR surface;
VkSwapchainKHR swapchain;
std::vector<VkImage> swapchainImages;
std::vector<VkImageView> swapchainImageViews;
std::vector<SkSurface*> skiaSurfaces;
GrDirectContext* skiaContext = nullptr;

// Vulkan 동기화 객체
std::vector<VkFence> inFlightFences;
std::vector<VkSemaphore> imageAvailableSemaphores;
std::vector<VkSemaphore> renderFinishedSemaphores;
const int MAX_FRAMES_IN_FLIGHT = 2;

// Vulkan 및 Skia 리소스 해제
void cleanup() {
    for (auto surface : skiaSurfaces) {
        surface->unref();
    }
    skiaContext->releaseResourcesAndAbandonContext();
    skiaContext->unref();
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwTerminate();
}

// Vulkan 인스턴스 생성
bool createVulkanInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Skia Vulkan Full Example";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    return vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS;
}

// 물리 디바이스 및 논리 디바이스 생성
bool createLogicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) return false;
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    
    physicalDevice = devices[0]; // 단순화

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = 0;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    VkPhysicalDeviceFeatures deviceFeatures{};
    
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    
    const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) return false;
    
    vkGetDeviceQueue(device, 0, 0, &graphicsQueue);
    graphicsQueueIndex = 0;
    
    return true;
}

// 스왑체인 생성
bool createSwapchain(GLFWwindow* window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = 2; // 더블 버퍼링
    createInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = { (uint32_t)width, (uint32_t)height };
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) return false;

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    return true;
}

// 이미지 뷰 및 동기화 객체 생성
bool createSyncObjects() {
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}


void createSkiaContextAndSurfaces(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue graphicsQueue,
    uint32_t graphicsQueueIndex,
    const std::vector<VkImage>& swapchainImages,
    std::vector<sk_sp<SkSurface>>& skiaSurfaces,
    sk_sp<GrDirectContext>& skiaContext) 
{
    // // 1. Create and populate the VulkanBackendContext struct
    // skgpu::VulkanBackendContext vkBackendContext;
    // vkBackendContext.fInstance = instance;
    // vkBackendContext.fPhysicalDevice = physicalDevice;
    // vkBackendContext.fDevice = device;
    // vkBackendContext.fQueue = graphicsQueue;
    // vkBackendContext.fGraphicsQueueIndex = graphicsQueueIndex;
    
    // // 2. Add the getProc function pointer directly to the struct
    // vkBackendContext.fGet  = [](const char* name, VkInstance instance, VkDevice device) {
    //     if (instance) {
    //         return vkGetInstanceProcAddr(instance, name);
    //     }
    //     return vkGetDeviceProcAddr(device, name);
    // };

    // // 3. Pass the populated struct to the GrDirectContext factory function
    // skiaContext = GrDirectContext::MakeVulkan(vkBackendContext);

    // if (!skiaContext) {
    //     std::cerr << "Failed to create Skia GrDirectContext!" << std::endl;
    //     return;
    // }

    // // Loop for creating SkSurfaces remains the same
    // for (const auto& image : swapchainImages) {
    //     GrVkImageInfo vkImageInfo{
    //         .fImage = image,
    //         .fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    //         .fFormat = VK_FORMAT_B8G8R8A8_UNORM,
    //         .fImageTiling = VK_IMAGE_TILING_OPTIMAL,
    //         .fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    //         .fSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    //         .fCurrentQueueFamily = graphicsQueueIndex
    //     };
        
    //     GrBackendTexture backendTexture(1920, 1080, vkImageInfo);
        
    //     sk_sp<SkSurface> surface = SkSurface::MakeFromBackendTexture(
    //         skiaContext.get(),
    //         backendTexture,
    //         kTopLeft_GrSurfaceOrigin,
    //         1,
    //         kBGRA_8888_SkColorType
    //     );

    //     if (!surface) {
    //         continue;
    //     }

    //     skiaSurfaces.push_back(surface);
    // }
}


// 렌더링 루프
void drawFrame() {
    static size_t currentFrame = 0;
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    SkCanvas* canvas = skiaSurfaces[imageIndex]->getCanvas();
    if (canvas) {
        canvas->clear(SK_ColorBLUE);
        SkPaint paint;
        paint.setColor(SK_ColorRED);
        SkRect rect = SkRect::MakeXYWH(100.0f, 100.0f, 200.0f, 150.0f);
        canvas->drawRect(rect, paint);
    }
    
    skiaContext->flushAndSubmit();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 0; // Skia가 내부적으로 처리
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];
    
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        std::cerr << "Failed to submit draw command buffer!" << std::endl;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    
    vkQueuePresentKHR(graphicsQueue, &presentInfo);
    
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Skia Vulkan Full Example", nullptr, nullptr);
    
    if (!createVulkanInstance() ||
        glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS ||
        !createLogicalDevice() ||
        !createSwapchain(window) ||
        !createSyncObjects()) {
        std::cerr << "Vulkan setup failed!" << std::endl;
        return 1;
    }
    
    // createSkiaContextAndSurfaces();
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // drawFrame();
    }
    
    vkDeviceWaitIdle(device);
    cleanup();
    
    return 0;
}