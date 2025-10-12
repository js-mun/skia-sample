#define GLFW_INCLUDE_VULKAN

#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>

// Skia includes
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"

#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "include/gpu/ganesh/vk/GrVkTypes.h"
#include "include/gpu/vk/VulkanBackendContext.h"

#define WIDTH 800
#define HEIGHT 600

// 구조체로 Vulkan 객체 관리
struct VulkanContext
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    VkQueue queue;
    uint32_t queueFamilyIndex;
    VkSwapchainKHR swapchain;
    VkFormat format;
    VkExtent2D extent;
    VkCommandPool cmdPool;
    std::vector<VkImage> images;
    std::vector<sk_sp<SkSurface>> skSurfaces;
};

bool setupVulkan(GLFWwindow *window, VulkanContext &vkCtx, sk_sp<GrDirectContext> &skContext)
{
    // --- Vulkan Instance ---
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Skia Vulkan Example";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtCount = 0;
    const char **glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

    VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = glfwExtCount;
    instInfo.ppEnabledExtensionNames = glfwExts;

    if (vkCreateInstance(&instInfo, nullptr, &vkCtx.instance) != VK_SUCCESS)
    {
        std::cerr << "Failed to create Vulkan instance\n";
        return false;
    }

    // --- Vulkan Surface via GLFW ---
    if (glfwCreateWindowSurface(vkCtx.instance, window, nullptr, &vkCtx.surface) != VK_SUCCESS)
    {
        std::cerr << "Failed to create GLFW Vulkan surface\n";
        return false;
    }

    // --- Physical Device ---
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkCtx.instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vkCtx.instance, &deviceCount, devices.data());
    vkCtx.physicalDevice = VK_NULL_HANDLE;

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    for (auto device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            std::cout << "Ignore Software GPU (llvmpipe)" << std::endl;
            continue;
        }

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            bestDevice = device;
            break;
        }
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            bestDevice = device; // 외장 없을 경우 사용 (iGPU)
        }
    }
    if (bestDevice == VK_NULL_HANDLE) {
        std::cerr << "No suitable hardware GPU found!" << std::endl;
        return false;
    }
    vkCtx.physicalDevice = bestDevice;
    
    // --- Queue Family ---
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkCtx.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vkCtx.physicalDevice, &queueFamilyCount, queueFamilies.data());
    bool found = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(vkCtx.physicalDevice, i, vkCtx.surface, &presentSupport);
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
            vkCtx.queueFamilyIndex = i;
            found = true;
            break;
        }
    }
    if (!found) {
        std::cerr << "No suitable queue family\n";
        return false;
    }
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = vkCtx.queueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    const char *deviceExts[] = {"VK_KHR_swapchain"};
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExts;

    if (vkCreateDevice(vkCtx.physicalDevice, &deviceInfo, nullptr, &vkCtx.device) != VK_SUCCESS) {
        std::cerr << "Failed to create device" << std::endl;
        return false;
    }
    vkGetDeviceQueue(vkCtx.device, vkCtx.queueFamilyIndex, 0, &vkCtx.queue);

    // --- Swapchain ---
    VkSurfaceCapabilitiesKHR surfCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkCtx.physicalDevice, vkCtx.surface, &surfCaps);
    vkCtx.extent = {WIDTH, HEIGHT};

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkCtx.physicalDevice, vkCtx.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkCtx.physicalDevice, vkCtx.surface, &formatCount, formats.data());

    // UNORM format 우선 (SRGB는 Skia wrap 설정이 어려움, 생성 실패할 수 있음)
    vkCtx.format = formats[0].format;
    VkColorSpaceKHR colorSpace = formats[0].colorSpace;

    for (const auto &fmt : formats)
    {
        std::cout << "Available: format=" << fmt.format << ", colorSpace=" << fmt.colorSpace << std::endl;
        // BGRA UNORM 우선
        if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM)
        {
            vkCtx.format = fmt.format;
            colorSpace = fmt.colorSpace;
            std::cout << "Selected: VK_FORMAT_B8G8R8A8_UNORM (" << vkCtx.format << ")" << std::endl;
            break;
        }
        if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            vkCtx.format = fmt.format;
            colorSpace = fmt.colorSpace;
            std::cout << "Selected: VK_FORMAT_R8G8B8A8_UNORM (" << vkCtx.format << ")" << std::endl;
            break;
        }
        if (vkCtx.format == formats[0].format)
        {
            std::cout << "Using default format: " << vkCtx.format << std::endl;
        }
    }

    VkSwapchainCreateInfoKHR swapInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapInfo.surface = vkCtx.surface;
    swapInfo.minImageCount = surfCaps.minImageCount + 1;
    swapInfo.imageFormat = vkCtx.format;
    swapInfo.imageColorSpace = colorSpace;
    swapInfo.imageExtent = vkCtx.extent;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform = surfCaps.currentTransform;
    swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapInfo.clipped = VK_TRUE;
    swapInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(vkCtx.device, &swapInfo, nullptr, &vkCtx.swapchain) != VK_SUCCESS)
    {
        std::cerr << "Failed to create swapchain\n";
        return false;
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(vkCtx.device, vkCtx.swapchain, &imageCount, nullptr);
    vkCtx.images.resize(imageCount);
    vkGetSwapchainImagesKHR(vkCtx.device, vkCtx.swapchain, &imageCount, vkCtx.images.data());

    std::cout << "SurfCaps imageCount: " << surfCaps.minImageCount 
            << ", Swapchain imageCount: " << imageCount << std::endl;


    // --- Skia Vulkan Context ---
    skgpu::VulkanBackendContext backendContext{};
    backendContext.fInstance = vkCtx.instance;
    backendContext.fPhysicalDevice = vkCtx.physicalDevice;
    backendContext.fDevice = vkCtx.device;
    backendContext.fQueue = vkCtx.queue;
    backendContext.fGraphicsQueueIndex = vkCtx.queueFamilyIndex;
    backendContext.fMaxAPIVersion = VK_API_VERSION_1_3;
    backendContext.fGetProc = [](const char *procName, VkInstance inst, VkDevice dev) -> PFN_vkVoidFunction
    {
        PFN_vkVoidFunction func = nullptr;
        if (dev != VK_NULL_HANDLE)
        {
            func = vkGetDeviceProcAddr(dev, procName);
        }
        if (!func && inst != VK_NULL_HANDLE)
        {
            func = vkGetInstanceProcAddr(inst, procName);
        }
        if (!func)
        {
            func = vkGetInstanceProcAddr(VK_NULL_HANDLE, procName);
        }
        // std::cout << "func ret = " << func << ", proc name: " << procName << std::endl;
        return func;
    };

    std::cout << "Instance: " << vkCtx.instance
              << ", PhysicalDevice: " << vkCtx.physicalDevice
              << ", Device: " << vkCtx.device
              << ", Queue: " << vkCtx.queue
              << ", QueueFamilyIndex: " << vkCtx.queueFamilyIndex
              << std::endl;

    skContext = GrDirectContexts::MakeVulkan(backendContext);
    if (!skContext)
    {
        std::cerr << "Failed to create Skia Vulkan context\n";
        return false;
    }

    // Create Skia surfaces
    vkCtx.skSurfaces.resize(vkCtx.images.size());
    for (size_t i = 0; i < vkCtx.images.size(); ++i)
    {
        GrVkImageInfo imgInfo{};
        imgInfo.fImage = vkCtx.images[i];
        imgInfo.fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.fFormat = vkCtx.format;
        imgInfo.fLevelCount = 1;
        imgInfo.fCurrentQueueFamily = vkCtx.queueFamilyIndex;

        GrBackendRenderTarget backendRT = GrBackendRenderTargets::MakeVk(WIDTH, HEIGHT, imgInfo);
        if (!backendRT.isValid())
        {
            std::cerr << "Invalid GrBackendRenderTarget for image " << i << std::endl;
            continue;
        }

        SkColorType colorType;
        if (vkCtx.format == VK_FORMAT_B8G8R8A8_UNORM || vkCtx.format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            colorType = kBGRA_8888_SkColorType;
        }
        else if (vkCtx.format == VK_FORMAT_R8G8B8A8_UNORM || vkCtx.format == VK_FORMAT_R8G8B8A8_SRGB)
        {
            colorType = kRGBA_8888_SkColorType;
        }
        else
        {
            colorType = kRGBA_8888_SkColorType;
        }

        vkCtx.skSurfaces[i] = SkSurfaces::WrapBackendRenderTarget(
            skContext.get(),
            backendRT,
            kTopLeft_GrSurfaceOrigin,
            colorType,
            nullptr, // colorSpace
            nullptr  // surfaceProps
        );

        if (!vkCtx.skSurfaces[i])
        {
            std::cerr << "Failed to wrap surface " << i << std::endl;
            return false;
        }
    }

    return true;
}

void drawFrame(SkCanvas *canvas)
{
    canvas->clear(SK_ColorWHITE);
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

int main()
{
    if (!glfwInit()) {
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Skia Vulkan", nullptr, nullptr);
    if (!window) {
        return -1;
    }

    VulkanContext vkCtx{};
    sk_sp<GrDirectContext> skContext;

    if (!setupVulkan(window, vkCtx, skContext))
    {
        std::cerr << "Failed Vulkan setup\n";
        return -1;
    }
    std::cout << "Setup vulkan Successfully" << std::endl;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vkCtx.device, vkCtx.swapchain, UINT64_MAX,
                                                VK_NULL_HANDLE, VK_NULL_HANDLE, &imageIndex);
        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to acquire swapchain image" << std::endl;
        }

        if (vkCtx.skSurfaces[imageIndex])
        {
            drawFrame(vkCtx.skSurfaces[imageIndex]->getCanvas());
            skContext->flushAndSubmit();
        }

        // Present swapchain
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &vkCtx.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(vkCtx.queue, &presentInfo);
    }

    for (auto &s : vkCtx.skSurfaces)
        s.reset();
    skContext.reset();

    vkDestroySwapchainKHR(vkCtx.device, vkCtx.swapchain, nullptr);
    vkDestroyDevice(vkCtx.device, nullptr);
    vkDestroySurfaceKHR(vkCtx.instance, vkCtx.surface, nullptr);
    vkDestroyInstance(vkCtx.instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
