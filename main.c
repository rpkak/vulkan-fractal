#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#ifndef NDEBUG
#define VALIDATION_LAYER
#endif

#ifdef VALIDATION_LAYER
static const char *validation_layer_name = "VK_LAYER_KHRONOS_validation";
#endif

#define MAX_FRAMES_IN_FLIGHT 3

typedef struct borders
{
    float x_left;
    float x_right;
    float y_top;
    float y_bottom;
} borders;

static void error_glfw(int error_code, const char *description)
{
    fprintf(stderr, "glfw error (%d): %s\n", error_code, description);
}

GLFWwindow *window;
VkInstance instance = VK_NULL_HANDLE;
VkPhysicalDevice physical_device = VK_NULL_HANDLE;
uint32_t graphics_queue_family_index;
uint32_t present_queue_family_index;
VkQueue graphics_queue = VK_NULL_HANDLE;
VkQueue present_queue = VK_NULL_HANDLE;
VkDevice device = VK_NULL_HANDLE;
VkSurfaceKHR surface = VK_NULL_HANDLE;
VkSwapchainKHR swapchain = VK_NULL_HANDLE;
uint32_t swapchain_image_count;
VkImage *swapchain_images;
VkImageView *swapchain_image_views;
VkSurfaceFormatKHR surface_format;
VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
VkShaderModule fragment_shader_module = VK_NULL_HANDLE;
VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
VkRenderPass render_pass = VK_NULL_HANDLE;
VkPipeline pipeline = VK_NULL_HANDLE;
VkFramebuffer *framebuffers;
VkCommandPool command_pool = VK_NULL_HANDLE;
VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
VkSemaphore image_acquired_semaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
VkFence frame_finished_fences[MAX_FRAMES_IN_FLIGHT];

VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
VkBuffer border_uniform_buffers[MAX_FRAMES_IN_FLIGHT];
VkDeviceMemory border_uniform_buffer_memories[MAX_FRAMES_IN_FLIGHT];
borders *border_uniform_buffer_mappings[MAX_FRAMES_IN_FLIGHT];
VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];

double zoom_level = 0;
double x_center = 0;
double y_center = 0;

VkExtent2D extent;
VkPresentModeKHR present_mode;

bool framebuffer_size_changed = false;

static void framebuffer_size_change_glfw(GLFWwindow *window, int width, int height)
{
    framebuffer_size_changed = true;
}

void scroll_callback_glfw(GLFWwindow *window, double xoffset, double yoffset)
{
    double xpos;
    double ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);
    // printf("%f %f\n", xpos / width - 0.5, ypos / height - 0.5);
    double x = x_center + pow(0.8, zoom_level) * (2.0 * xpos / width - 1.0);
    double y = y_center + pow(0.8, zoom_level) * (2.0 * ypos / height - 1.0) * height / width;
    zoom_level += yoffset;
    x_center = x - pow(0.8, zoom_level) * (2.0 * xpos / width - 1.0);
    y_center = y - pow(0.8, zoom_level) * (2.0 * ypos / height - 1.0) * height / width;
    // printf("%f %f\n", x, y);
}

static void cleanup_swapchain(void)
{
    for (uint32_t i = 0; i < swapchain_image_count; i++)
    {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
        vkDestroyImageView(device, swapchain_image_views[i], NULL);
    }
    free(framebuffers);
    free(swapchain_image_views);

    vkDestroySwapchainKHR(device, swapchain, NULL);
}

static void create_swapchain(void)
{

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

    if (surface_capabilities.currentExtent.width != UINT32_MAX)
    {
        extent = surface_capabilities.currentExtent;
    }
    else
    {
        int width;
        int height;
        glfwGetFramebufferSize(window, &width, &height);

        extent = (VkExtent2D){
            .width = width < surface_capabilities.minImageExtent.width ? surface_capabilities.minImageExtent.width : width > surface_capabilities.maxImageExtent.width ? surface_capabilities.maxImageExtent.width
                                                                                                                                                                       : width,
            .height = height < surface_capabilities.minImageExtent.height ? surface_capabilities.minImageExtent.height : height > surface_capabilities.maxImageExtent.height ? surface_capabilities.maxImageExtent.height
                                                                                                                                                                             : height,
        };
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = (surface_capabilities.maxImageCount == 0 || surface_capabilities.maxImageCount > surface_capabilities.minImageCount) ? surface_capabilities.minImageCount + 1 : surface_capabilities.maxImageCount,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    if (graphics_queue_family_index == present_queue_family_index)
    {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else
    {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = (uint32_t[]){
            graphics_queue_family_index,
            present_queue_family_index,
        };
    }

    if (vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateSwapchainKHR error\n");
    }

    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, NULL);
    swapchain_images = malloc(sizeof(VkImage) * swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images);

    swapchain_image_views = malloc(sizeof(VkImageView) * swapchain_image_count);
    framebuffers = malloc(sizeof(VkFramebuffer) * swapchain_image_count);

    for (uint32_t i = 0; i < swapchain_image_count; i++)
    {
        VkImageViewCreateInfo image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,
            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };

        if (vkCreateImageView(device, &image_view_create_info, NULL, &swapchain_image_views[i]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkCreateImageView error\n");
        }

        VkFramebufferCreateInfo framebuffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &swapchain_image_views[i],
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        if (vkCreateFramebuffer(device, &framebuffer_create_info, NULL, &framebuffers[i]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkCreateFramebuffer error\n");
        }
    }
    free(swapchain_images);
}

static void recreate_swapchain(void)
{
    vkDeviceWaitIdle(device);
    cleanup_swapchain();
    create_swapchain();
}

int main()
{
    glfwSetErrorCallback(&error_glfw);

    if (!glfwInit())
    {
        fprintf(stderr, "glfw failed\n");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(800, 600, "Vulkan window", NULL, NULL);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_change_glfw);
    glfwSetScrollCallback(window, scroll_callback_glfw);

    if (!window)
    {
        fprintf(stderr, "glfw failed\n");
    }

    VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Fractal",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "NO",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions;

    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

#ifdef VALIDATION_LAYER
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties *layer_properties = malloc(layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layer_count, layer_properties);

    bool validation_possible = false;

    for (uint32_t i = 0; i < layer_count; i++)
    {
        if (!strcmp(layer_properties[i].layerName, validation_layer_name))
        {
            validation_possible = true;
            break;
        }
    }
    free(layer_properties);

    if (!validation_possible)
    {
        fprintf(stderr, "Validation is not possible\n");
    }
#endif

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
#ifdef VALIDATION_LAYER
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &validation_layer_name,
#else
        .enabledLayerCount = 0,
#endif
        .enabledExtensionCount = glfw_extension_count,
        .ppEnabledExtensionNames = glfw_extensions,
    };

    if (vkCreateInstance(&instance_create_info, NULL, &instance) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateInstance error\n");
    }

    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS)
    {
        fprintf(stderr, "glfwCreateWindowSurface error\n");
    }

    uint32_t physical_device_count;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);

    if (physical_device_count == 0)
    {
        fprintf(stderr, "physical_device_count = 0\n");
    }

    VkPhysicalDevice *physical_devices = malloc(sizeof(VkPhysicalDevice) * physical_device_count);
    vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);

    uint32_t surface_format_count;
    uint32_t present_mode_count;

    for (uint32_t i = 0; i < physical_device_count; i++)
    {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL, &extension_count, NULL);
        VkExtensionProperties *extension_properties = malloc(sizeof(VkExtensionProperties) * extension_count);
        vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL, &extension_count, extension_properties);

        bool swap_chain = false;

        for (uint32_t j = 0; j < extension_count; j++)
        {
            if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, extension_properties[j].extensionName))
            {
                swap_chain = true;
                break;
            }
        }
        free(extension_properties);

        if (!swap_chain)
        {
            continue;
        }

        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], surface, &surface_format_count, NULL);
        if (surface_format_count == 0)
        {
            continue;
        }

        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[i], surface, &present_mode_count, NULL);
        if (present_mode_count == 0)
        {
            continue;
        }

        uint32_t queue_family_property_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_property_count, NULL);
        VkQueueFamilyProperties *queue_family_properties = malloc(sizeof(VkQueueFamilyProperties) * queue_family_property_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_property_count, queue_family_properties);

        bool graphics_queue_family = false;
        bool present_queue_family = false;

        for (uint32_t j = 0; j < queue_family_property_count; j++)
        {
            if (queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphics_queue_family_index = j;
                graphics_queue_family = true;
            }

            VkBool32 surface_support;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[i], j, surface, &surface_support);
            if (surface_support)
            {
                present_queue_family_index = j;
                present_queue_family = true;
            }
        }
        free(queue_family_properties);

        if (graphics_queue_family && present_queue_family)
        {
            physical_device = physical_devices[i];
            break;
        }
    }
    free(physical_devices);

    if (physical_device == VK_NULL_HANDLE)
    {
        fprintf(stderr, "no possible physical_device found\n");
    }

    VkDeviceQueueCreateInfo device_queue_create_infos[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = graphics_queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = (float[]){1.0f},
        },
    };

    if (graphics_queue_family_index != present_queue_family_index)
    {
        device_queue_create_infos[1] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = present_queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = (float[]){1.0f},
        };
    }

    VkPhysicalDeviceFeatures physical_device_features = {};

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = graphics_queue_family_index == present_queue_family_index ? 1 : 2,
        .pQueueCreateInfos = device_queue_create_infos,
#ifdef VALIDATION_LAYER
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &validation_layer_name,
#else
        .enabledLayerCount = 0,
#endif
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = (const char *[]){VK_KHR_SWAPCHAIN_EXTENSION_NAME},
        .pEnabledFeatures = &physical_device_features,
    };

    if (vkCreateDevice(physical_device, &device_create_info, NULL, &device) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateDevice error\n");
    }

    vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);
    vkGetDeviceQueue(device, present_queue_family_index, 0, &present_queue);

    VkSurfaceFormatKHR *surface_formats = malloc(sizeof(VkSurfaceFormatKHR) * surface_format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, surface_formats);

    uint32_t surface_format_index = 0;

    for (uint32_t i = 0; i < surface_format_count; i++)
    {
        if (surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surface_format_index = i;
        }
    }
    surface_format = surface_formats[surface_format_index];
    free(surface_formats);

    VkRenderPassCreateInfo render_pass_create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = (VkAttachmentDescription[]){
            {
                .format = surface_format.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        },
        .subpassCount = 1,
        .pSubpasses = (VkSubpassDescription[]){
            {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = 0,
                .colorAttachmentCount = 1,
                .pColorAttachments = (VkAttachmentReference[]){
                    {
                        .attachment = 0,
                        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    },
                },
                .preserveAttachmentCount = 0,
            },
        },
        .dependencyCount = 1,
        .pDependencies = (VkSubpassDependency[]){
            {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
        },
    };

    if (vkCreateRenderPass(device, &render_pass_create_info, NULL, &render_pass) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateRenderPass error\n");
    }

    VkPresentModeKHR *present_modes = malloc(sizeof(VkPresentModeKHR) * present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes);

    present_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (uint32_t i = 0; i < present_mode_count; i++)
    {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }
    free(present_modes);

    create_swapchain();

    VkDescriptorSetLayoutBinding border_descriptor_set_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &border_descriptor_set_layout_binding,
    };

    if (vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, NULL, &descriptor_set_layout))
    {
        fprintf(stderr, "vkCreateDescriptorSetLayout error\n");
    }

    FILE *vertex_shader_file = fopen("shaders/vert.spv", "r");
    if (fseek(vertex_shader_file, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "fseek end vertex_shader_file error\n");
    }
    long vertex_shader_file_size = ftell(vertex_shader_file);
    if (vertex_shader_file_size == -1)
    {
        fprintf(stderr, "ftell vertex_shader_file error\n");
    }
    if (fseek(vertex_shader_file, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "fseek set vertex_shader_file error\n");
    }
    uint32_t *vertex_shader_file_buffer = malloc(vertex_shader_file_size);
    if (fread(vertex_shader_file_buffer, vertex_shader_file_size, 1, vertex_shader_file) != 1)
    {
        fprintf(stderr, "fread vertex_shader_file error\n");
    }
    if (fclose(vertex_shader_file) != 0)
    {
        fprintf(stderr, "fclose vertex_shader_file error\n");
    }

    VkShaderModuleCreateInfo vertex_shader_module_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertex_shader_file_size,
        .pCode = vertex_shader_file_buffer,
    };
    if (vkCreateShaderModule(device, &vertex_shader_module_create_info, NULL, &vertex_shader_module) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateShaderModule for vertex error\n");
    }
    free(vertex_shader_file_buffer);

    FILE *fragment_shader_file = fopen("shaders/frag.spv", "r");
    if (fseek(fragment_shader_file, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "fseek end fragment_shader_file error\n");
    }
    long fragment_shader_file_size = ftell(fragment_shader_file);
    if (fragment_shader_file_size == -1)
    {
        fprintf(stderr, "ftell fragment_shader_file error\n");
    }
    if (fseek(fragment_shader_file, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "fseek set fragment_shader_file error\n");
    }
    uint32_t *fragment_shader_file_buffer = malloc(fragment_shader_file_size);
    if (fread(fragment_shader_file_buffer, fragment_shader_file_size, 1, fragment_shader_file) != 1)
    {
        fprintf(stderr, "fread fragment_shader_file error\n");
    }
    if (fclose(fragment_shader_file) != 0)
    {
        fprintf(stderr, "fclose fragment_shader_file error\n");
    }

    VkShaderModuleCreateInfo fragment_shader_module_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragment_shader_file_size,
        .pCode = fragment_shader_file_buffer,
    };
    if (vkCreateShaderModule(device, &fragment_shader_module_create_info, NULL, &fragment_shader_module) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateShaderModule for fragment error\n");
    }
    free(fragment_shader_file_buffer);

    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_infos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader_module,
            .pName = "main",
        },
    };

    VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = (VkDynamicState[]){
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        },
    };

    VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };

    VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1,
    };

    VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = (VkPipelineColorBlendAttachmentState[]){
            {
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            },
        },
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_set_layout,
        .pushConstantRangeCount = 0,
    };

    if (vkCreatePipelineLayout(device, &pipeline_layout_create_info, NULL, &pipeline_layout) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreatePipelineLayout error\n");
    }

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = pipeline_shader_stage_create_infos,
        .pVertexInputState = &pipeline_vertex_input_state_create_info,
        .pInputAssemblyState = &pipeline_input_assembly_state_create_info,
        .pViewportState = &pipeline_viewport_state_create_info,
        .pRasterizationState = &pipeline_rasterization_state_create_info,
        .pMultisampleState = &pipeline_multisample_state_create_info,
        .pColorBlendState = &pipeline_color_blend_state_create_info,
        .pDynamicState = &pipeline_dynamic_state_create_info,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, NULL, &pipeline) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateGraphicsPipelines error\n");
    }

    vkDestroyShaderModule(device, vertex_shader_module, NULL);
    vkDestroyShaderModule(device, fragment_shader_module, NULL);

    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_queue_family_index,
    };

    if (vkCreateCommandPool(device, &command_pool_create_info, NULL, &command_pool) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateCommandPool error\n");
    }

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };

    if (vkAllocateCommandBuffers(device, &command_buffer_allocate_info, command_buffers) != VK_SUCCESS)
    {
        fprintf(stderr, "vkAllocateCommandBuffers error\n");
    }

    for (uint8_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkBufferCreateInfo buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(borders),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        if (vkCreateBuffer(device, &buffer_create_info, NULL, &border_uniform_buffers[i]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkCreateBuffer error\n");
        }

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device, border_uniform_buffers[i], &memory_requirements);

        VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);

        uint32_t memory_type_index;
        bool memory_type_found = false;

        for (uint32_t j = 0; j < physical_device_memory_properties.memoryTypeCount; j++)
        {
            if ((memory_requirements.memoryTypeBits & (1 << j)) && (physical_device_memory_properties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && (physical_device_memory_properties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                memory_type_index = j;
                memory_type_found = true;
            }
        }

        if (!memory_type_found)
        {
            fprintf(stderr, "memory type not found\n");
        }

        VkMemoryAllocateInfo memory_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = memory_type_index,
        };

        if (vkAllocateMemory(device, &memory_allocate_info, NULL, &border_uniform_buffer_memories[i]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkAllocateMemory error\n");
        }

        if (vkBindBufferMemory(device, border_uniform_buffers[i], border_uniform_buffer_memories[i], 0) != VK_SUCCESS)
        {
            fprintf(stderr, "vkBindBufferMemory error\n");
        }

        if (vkMapMemory(device, border_uniform_buffer_memories[i], 0, sizeof(borders), 0, (void **)&border_uniform_buffer_mappings[i]))
        {
            fprintf(stderr, "vkMapMemory error\n");
        }
    }

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = (VkDescriptorPoolSize[]){
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT,
            },
        },
    };

    if (vkCreateDescriptorPool(device, &descriptor_pool_create_info, NULL, &descriptor_pool) != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateDescriptorPool error\n");
    }

    VkDescriptorSetLayout descriptor_set_layouts[MAX_FRAMES_IN_FLIGHT];
    for (uint8_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        descriptor_set_layouts[i] = descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = descriptor_set_layouts,
    };

    if (vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, descriptor_sets))
    {
        fprintf(stderr, "vkAllocateDescriptorSets error\n");
    }
    for (uint8_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkWriteDescriptorSet write_descriptor_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &(VkDescriptorBufferInfo){
                .buffer = border_uniform_buffers[i],
                .offset = 0,
                .range = sizeof(borders),
            },
        };

        vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, NULL);
    }

    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (uint64_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device, &semaphore_create_info, NULL, &image_acquired_semaphores[i]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkCreateSemaphore error\n");
        }

        if (vkCreateSemaphore(device, &semaphore_create_info, NULL, &render_finished_semaphores[i]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkCreateSemaphore error\n");
        }

        if (vkCreateFence(device, &fence_create_info, NULL, &frame_finished_fences[i]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkCreateFence error\n");
        }
    }

    uint8_t current_frame = 0;
    uint64_t ticks = 0;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (vkWaitForFences(device, 1, &frame_finished_fences[current_frame], VK_TRUE, UINT64_MAX) != VK_SUCCESS)
        {
            fprintf(stderr, "vkWaitForFences error\n");
        }
        uint32_t image_index;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_acquired_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreate_swapchain();
            continue;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            fprintf(stderr, "vkAcquireNextImageKHR error\n");
        }

        if (vkResetFences(device, 1, &frame_finished_fences[current_frame]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkResetFences error\n");
        }

        border_uniform_buffer_mappings[current_frame]->x_left = x_center - pow(0.8, zoom_level);
        border_uniform_buffer_mappings[current_frame]->x_right = x_center + pow(0.8, zoom_level);
        border_uniform_buffer_mappings[current_frame]->y_top = y_center + pow(0.8, zoom_level) * extent.height / extent.width;
        border_uniform_buffer_mappings[current_frame]->y_bottom = y_center - pow(0.8, zoom_level) * extent.height / extent.width;

        vkResetCommandBuffer(command_buffers[current_frame], 0);

        VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        if (vkBeginCommandBuffer(command_buffers[current_frame], &command_buffer_begin_info) != VK_SUCCESS)
        {
            fprintf(stderr, "vkBeginCommandBuffer error\n");
        }

        VkRenderPassBeginInfo render_pass_begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = framebuffers[image_index],
            .renderArea.offset.x = 0,
            .renderArea.offset.y = 0,
            .renderArea.extent = extent,
            .clearValueCount = 1,
            .pClearValues = (VkClearValue[]){
                {
                    .color.float32 = {0, 0, 0, 1},
                },
            },
        };

        vkCmdBeginRenderPass(command_buffers[current_frame], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffers[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = extent.width,
            .height = extent.height,
            .minDepth = 0,
            .maxDepth = 1,
        };
        vkCmdSetViewport(command_buffers[current_frame], 0, 1, &viewport);

        VkRect2D scissor = {
            .offset.x = 0,
            .offset.y = 0,
            .extent = extent,
        };
        vkCmdSetScissor(command_buffers[current_frame], 0, 1, &scissor);

        vkCmdBindDescriptorSets(command_buffers[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_sets[current_frame], 0, NULL);

        vkCmdDraw(command_buffers[current_frame], 4, 1, 0, 0);

        vkCmdEndRenderPass(command_buffers[current_frame]);

        if (vkEndCommandBuffer(command_buffers[current_frame]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkEndCommandBuffer error\n");
        }

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &image_acquired_semaphores[current_frame],
            .pWaitDstStageMask = (VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffers[current_frame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_finished_semaphores[current_frame],
        };

        if (vkQueueSubmit(graphics_queue, 1, &submit_info, frame_finished_fences[current_frame]) != VK_SUCCESS)
        {
            fprintf(stderr, "vkQueueSubmit error\n");
        }

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_finished_semaphores[current_frame],
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index,
        };

        result = vkQueuePresentKHR(present_queue, &present_info);

        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR || framebuffer_size_changed)
        {
            recreate_swapchain();
        }
        else if (result != VK_SUCCESS)
        {
            fprintf(stderr, "vkQueuePresentKHR error\n");
        }

        // printf("%lu\n", ticks);
        ticks++;
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    if (vkDeviceWaitIdle(device) != VK_SUCCESS)
    {
        fprintf(stderr, "vkDeviceWaitIdle error\n");
    }

    cleanup_swapchain();

    for (uint64_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyBuffer(device, border_uniform_buffers[i], NULL);
        vkFreeMemory(device, border_uniform_buffer_memories[i], NULL);
    }

    vkDestroyDescriptorPool(device, descriptor_pool, NULL);

    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, NULL);
    for (uint64_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, image_acquired_semaphores[i], NULL);
        vkDestroySemaphore(device, render_finished_semaphores[i], NULL);
        vkDestroyFence(device, frame_finished_fences[i], NULL);
    }
    vkDestroyCommandPool(device, command_pool, NULL);
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyRenderPass(device, render_pass, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
