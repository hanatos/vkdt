#include <string.h>
#include <stdio.h>
#include <vulkan/vulkan.h>

const char *validation_layers[] = {"VK_LAYER_LUNARG_standard_validation"};
int validation_layers_cnt = 1;
VkDebugUtilsMessengerEXT vk_debug_callback;

const char* device_extensions[] = {""};
const int device_extensions_cnt = 0;

// validation prints helpful stuff when things go unexpected
static inline int is_validation_available()
{
  uint32_t num_layers;
  vkEnumerateInstanceLayerProperties(&num_layers, 0);

  VkLayerProperties layers[num_layers];
  vkEnumerateInstanceLayerProperties(&num_layers, layers);

  for(int k=0;k<validation_layers_cnt;k++)
  {
    int layerFound = 0;
    for(int i=0;i<num_layers;i++)
      if (!strcmp(validation_layers[k], layers[i].layerName))
        layerFound = 1;
    if (!layerFound) return 0;
  }
  return 1;
}

static inline VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pCallback)
{
  PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (func)
    return func(instance, pCreateInfo, pAllocator, pCallback);
  else
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static inline void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT vk_debug_callback,
    const VkAllocationCallbacks* pAllocator)
{
  PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func)
    func(instance, vk_debug_callback, pAllocator);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
  fprintf(stderr, "[vkrun] validation layer: %s\n", pCallbackData->pMessage);
  return VK_FALSE;
}

static inline void make_debug_callback(VkInstance instance)
{
  if (!validation_layers_cnt) return;

  VkDebugUtilsMessengerCreateInfoEXT info = {};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = debug_callback;

  if (CreateDebugUtilsMessengerEXT(instance, &info, 0, &vk_debug_callback) != VK_SUCCESS)
    fprintf(stderr, "[vkrun] could not set up debug callback, panic now!\n");
}

// this is our vulkan entry point
static inline VkInstance create_instance()
{
  if(validation_layers_cnt && !is_validation_available())
    validation_layers_cnt = 0;

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "app name";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "super awesome engine name, so that the driver can identify it and optimize stuff for exatly this engine + version";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  info.pApplicationInfo = &appInfo;

  const char *ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  if(validation_layers_cnt)
  {
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = &ext;
  }

  info.enabledLayerCount = validation_layers_cnt;
  info.ppEnabledLayerNames = validation_layers;

  VkInstance instance;
  if (vkCreateInstance(&info, 0, &instance) != VK_SUCCESS)
    fprintf(stderr, "[vkrun] could not create instance, panic now!");

  make_debug_callback(instance);
  return instance;
}

static inline void destroy_instance(VkInstance instance)
{
  if (validation_layers_cnt)
    DestroyDebugUtilsMessengerEXT(instance, vk_debug_callback, 0);
  vkDestroyInstance(instance, 0);
}

// device creation
typedef struct queue_t
{
  uint32_t invalid;
  uint32_t idx;
  VkQueue que;
}
queue_t;

typedef struct device_queues_t
{
  queue_t graphics;
  queue_t compute;
}
device_queues_t;

static inline int device_queues_valid(device_queues_t *q)
{
  return (q->graphics.idx != ~0u) && (q->compute.idx != ~0u);
}

static inline device_queues_t get_queue_family_indices(VkPhysicalDevice d)
{
  device_queues_t qs = {
    {.invalid=~0u, .idx=~0u, .que=VK_NULL_HANDLE},
    {.invalid=~0u, .idx=~0u, .que=VK_NULL_HANDLE}};

  uint32_t num_families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(d, &num_families, 0);

  assert(num_families <= 20);
  VkQueueFamilyProperties families[20];
  vkGetPhysicalDeviceQueueFamilyProperties(d, &num_families, families);

  for(int i=0;i<num_families;i++)
  {
    if(families[i].queueCount > 0 && families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      qs.graphics.idx = i;
    if(families[i].queueCount > 0 && families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
      qs.compute.idx = i;
    if(device_queues_valid(&qs)) return qs;
  }
  fprintf(stderr, "[vkrun] no valid device queues found!\n");
  return qs;
}

static inline int check_device_extensions(VkPhysicalDevice device)
{
  return 1; // all good, we know we don't need any!
#if 0
  uint32_t num_extensions;
  vkEnumerateDeviceExtensionProperties(device, 0, &num_extensions, 0);

  VkExtensionPropertios availableExtensions[num_extensions];
  vkEnumerateDeviceExtensionProperties(device, 0, &num_extensions, availableExtensions);

  std::set<std::string> required(device_extensions.begin(), device_extensions.end());
  for(int k=0;k<num_extensions;k++)

  for (const auto& extension : availableExtensions) {
    required.erase(extension.extensionName);
  }

  return required.empty();
#endif
}

static inline int check_device_compatibility(VkPhysicalDevice device)
{
  device_queues_t que = get_queue_family_indices(device);
  return device_queues_valid(&que) && check_device_extensions(device);
}

// return number of devices
static inline int get_physical_devices(
    VkInstance instance, VkPhysicalDevice *dev, int maxd)
{
  uint32_t num_devices = 0;
  vkEnumeratePhysicalDevices(instance, &num_devices, 0);

  if (num_devices == 0) return 0;

  num_devices = num_devices > maxd ? maxd : num_devices;

  vkEnumeratePhysicalDevices(instance, &num_devices, dev);

  for(int i=0;i<num_devices;i++)
  {
    if (check_device_compatibility(dev[i])) continue;
    num_devices--;
    VkPhysicalDevice tmp = dev[num_devices];
    dev[num_devices] = dev[i];
    dev[i] = tmp;
  }

  return num_devices;
}

static inline VkDevice create_logic_device(
    VkPhysicalDevice pd, device_queues_t* qs)
{
  *qs = get_queue_family_indices(pd);

  VkDeviceQueueCreateInfo queue_infos[2] = {{0}};
  uint32_t unique_qs[] = {qs->graphics.idx, qs->compute.idx};

  int ucnt = 2;
  float priority = 1.0f;
  // dedup:
  for(int j=0, k=0;k<ucnt;k++,j++)
  {
    queue_infos[k].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[k].queueFamilyIndex = unique_qs[j];
    queue_infos[k].queueCount = 1;
    queue_infos[k].pQueuePriorities = &priority;
    for(int i=0;i<k;i++)
    {
      if(queue_infos[i].queueFamilyIndex == queue_infos[k].queueFamilyIndex)
      {
        k--; ucnt--;
        break;
      }
    }
  }

  VkPhysicalDeviceFeatures deviceFeatures = {0};

  VkDeviceCreateInfo device_info = {0};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

  device_info.queueCreateInfoCount = ucnt;
  device_info.pQueueCreateInfos = queue_infos;

  device_info.pEnabledFeatures = &deviceFeatures;

  device_info.enabledExtensionCount = device_extensions_cnt;
  device_info.ppEnabledExtensionNames = device_extensions;

  device_info.enabledLayerCount = validation_layers_cnt;
  device_info.ppEnabledLayerNames = validation_layers;

  VkDevice device;
  if (vkCreateDevice(pd, &device_info, 0, &device) != VK_SUCCESS)
    fprintf(stderr, "[vkrun] failed to create logical device!\n");

  vkGetDeviceQueue(device, qs->graphics.idx, 0, &qs->graphics.que);
  vkGetDeviceQueue(device, qs->compute.idx, 0, &qs->compute.que);

  return device;
}

static inline void destroy_device(VkDevice device)
{
  vkDestroyDevice(device, 0);
}

static inline VkCommandPool create_commandpool(
    VkDevice d, uint32_t queue_index)
{
  VkCommandPoolCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.queueFamilyIndex = queue_index;

  VkCommandPool pool;
  if (vkCreateCommandPool(d, &info, 0, &pool) != VK_SUCCESS)
    fprintf(stderr, "[vkrun] could not create command pool!\n");

  return pool;
}

static inline void destroy_commandpool(VkDevice d, VkCommandPool p)
{
  vkDestroyCommandPool(d, p, 0);
}

static inline VkCommandBuffer create_commandbuffer(VkDevice d, VkCommandPool p)
{
  VkCommandBufferAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.commandPool = p;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  info.commandBufferCount = 1;

  VkCommandBuffer cb;
  if (vkAllocateCommandBuffers(d, &info, &cb) != VK_SUCCESS)
    fprintf(stderr, "[vkrun] could not allocate command buffer!\n");

  return cb;
}

static inline void *read_file(const char *filename, size_t *len)
{
  FILE *f = fopen(filename, "rb");
  if(!f) { perror("can't fopen"); return 0;}
  fseek(f, 0, SEEK_END);
  const size_t filesize = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *file = malloc(filesize+1);

  size_t rd = fread(file, sizeof(char), filesize, f);
  file[filesize] = 0;
  if(rd != filesize)
  {
    free(file);
    file = 0;
    fclose(f);
    return 0;
  }
  if(len) *len = filesize;
  fclose(f);
  return file;
}

static inline VkShaderModule create_shader_module(
    VkDevice d, const char *filename)
{
  size_t len;
  void *data = read_file(filename, &len);

  VkShaderModuleCreateInfo info = {0};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = len;
  info.pCode = data;

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(d, &info, 0, &shaderModule) != VK_SUCCESS)
    fprintf(stderr, "[vkrun] failed to create shader module!\n");

  free(data);

  return shaderModule;
}

typedef struct pipe_t
{
  VkPipeline pipeline;
  VkPipelineLayout layout;
  VkDescriptorSetLayout ds_layout;
  VkDescriptorPool pool;
}
pipe_t;

pipe_t create_compute_pipeline(
    VkDevice d,
    const char *filename,
    const VkDescriptorSetLayoutBinding *bindings,
    const int bindings_cnt,
    uint32_t ds_count)
{
  uint8_t shader_const_data[1024];
  size_t shader_const_data_size = 0;
  VkSpecializationMapEntry shader_const_map[1024]; // {id, start, size}
  uint32_t shader_const_map_cnt = 0;
  VkSpecializationInfo info = {0};
  info.mapEntryCount = shader_const_map_cnt;
  info.pMapEntries = shader_const_map;
  info.dataSize = shader_const_data_size;
  info.pData = shader_const_data;

  pipe_t p;

  // create a descriptor set layout
  VkDescriptorSetLayoutCreateInfo ds_layout_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = bindings_cnt,
    .pBindings = bindings
  };

  if (vkCreateDescriptorSetLayout(d, &ds_layout_info, 0, &p.ds_layout) != VK_SUCCESS)
  {
    fprintf(stderr, "[vkrun] could not create descriptor set layout!\n");
    return p;
  }

  // create the pipeline layout
  VkPipelineLayoutCreateInfo layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &p.ds_layout,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges = 0,
  };

  if (vkCreatePipelineLayout(d, &layout_info, 0, &p.layout) != VK_SUCCESS)
  {
    fprintf(stderr, "[vkrun] could not create pipeline layout!\n");
    return p;
  }

  // create the shader stages (only compute)
  VkPipelineShaderStageCreateInfo stage_info = {};
  stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage_info.pSpecializationInfo = &info;
  stage_info.pName = "main";
  stage_info.module = create_shader_module(d, filename);


  // finally create the pipeline
  VkComputePipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.stage = stage_info;
  pipeline_info.layout = p.layout;

  if (vkCreateComputePipelines(d, VK_NULL_HANDLE, 1, &pipeline_info, 0, &p.pipeline) != VK_SUCCESS)
  {
    fprintf(stderr, "[vkrun] could not create pipeline!\n");
    return p;
  }

  // we don't need the module any more
  vkDestroyShaderModule(d, stage_info.module, 0);


  assert(bindings_cnt <= 20);
  VkDescriptorPoolSize pool_sizes[20] = {{0}};
  int pool_sizes_cnt = 0;
  for(int k=0;k<bindings_cnt;k++)
  {
    int i=0;
    for(;i<pool_sizes_cnt;i++)
    {
      if(bindings[k].descriptorType == pool_sizes[i].type)
      {
        pool_sizes[i].descriptorCount+=ds_count;
        break;
      }
    }
    if(i==pool_sizes_cnt)
    {
      pool_sizes_cnt++;
      pool_sizes[i] = (VkDescriptorPoolSize){.type=bindings[k].descriptorType, .descriptorCount=ds_count};
    }
  }

  VkDescriptorPoolCreateInfo pool_info = {0};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = pool_sizes_cnt;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = ds_count;

  if (vkCreateDescriptorPool(d, &pool_info, 0, &p.pool) != VK_SUCCESS)
  {
    fprintf(stderr, "[vkrun] could not create descriptor pool!\n");
    return p;
  }

  return p;
}

static inline VkDescriptorSet create_descriptorset(VkDevice d, pipe_t p)
{
  VkDescriptorSetAllocateInfo dset_info = {};
  dset_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  dset_info.descriptorPool = p.pool;
  dset_info.descriptorSetCount = 1;
  dset_info.pSetLayouts = &p.ds_layout;

  VkDescriptorSet dset;
  if (vkAllocateDescriptorSets(d, &dset_info, &dset) != VK_SUCCESS)
  {
    fprintf(stderr, "[vkrun] could not create descriptor set");
    return 0;
  }
  return dset;
}

static inline void destroy_pipeline(VkDevice d, pipe_t p)
{
  vkDestroyPipeline(d, p.pipeline, 0);
  vkDestroyPipelineLayout(d, p.layout, 0);
  vkDestroyDescriptorSetLayout(d, p.ds_layout, 0);
  vkDestroyDescriptorPool(d, p.pool, 0);
}

static inline uint32_t get_memory_type(
    VkPhysicalDevice pd, uint32_t bits, VkMemoryPropertyFlags properties)
{
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(pd, &mem_props);

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++)
    if ((bits & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
      return i;

  return ~0u;
}

static inline void create_buffer(
    VkPhysicalDevice pd, VkDevice d, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer* buffer, VkDeviceMemory* mem)
{
  VkBufferCreateInfo buffer_info = {0};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  vkCreateBuffer(d, &buffer_info, 0, buffer);

  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(d, *buffer, &requirements);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = requirements.size;
  alloc_info.memoryTypeIndex = get_memory_type(pd, requirements.memoryTypeBits, properties);
  vkAllocateMemory(d, &alloc_info, 0, mem);

  vkBindBufferMemory(d, *buffer, *mem, 0);
}

static inline void destroy_buffer(VkDevice d, VkBuffer b, VkDeviceMemory mem)
{
  vkDestroyBuffer(d, b, 0);
  vkFreeMemory(d, mem, 0);
}

static inline int vkrun(
    // void        *data_uniform,
    // size_t       data_uniform_size,
    const float *data_in,
    const size_t data_in_size,
    float       *data_out,
    const size_t data_out_size,
    const int    wd,
    const int    ht,
    const char  *fname_spv,     // compiled shader file name
    const int    num_iterations,
    const int    par0)
{
  uint32_t data_uniform[5] = {0, num_iterations, wd, ht, par0};
  size_t data_uniform_size = sizeof(data_uniform);

  VkInstance instance = create_instance();
  VkPhysicalDevice pdevices[10];
  // int num_pdev =
  get_physical_devices(instance, pdevices, 10);
  device_queues_t dqueues;
  VkDevice device = create_logic_device(pdevices[0], &dqueues);

  VkDescriptorSetLayoutBinding bindings[] = {
    {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
  };
  pipe_t pipe = create_compute_pipeline(
      device,
      fname_spv,
      bindings,
      sizeof(bindings)/sizeof(bindings[0]),
      2); // we'll have two descriptor sets

  VkDescriptorSet ds  = create_descriptorset(device, pipe);
  VkDescriptorSet ds2 = create_descriptorset(device, pipe);
  VkCommandPool pool = create_commandpool(device, dqueues.compute.idx);

  VkBuffer ub, bin, bout, bout2, bstaging;
  VkDeviceMemory memub, memin, memout, memout2, memstaging;
  // uniforms
  create_buffer(pdevices[0],
                device,
                data_uniform_size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &ub,
                &memub);
  // input
  create_buffer(pdevices[0],
                device,
                data_in_size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &bin,
                &memin);
  // ping pong buffer
  // output
  create_buffer(pdevices[0],
                device,
                data_out_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &bout2,
                &memout2);
  // output
  create_buffer(pdevices[0],
                device,
                data_out_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &bout,
                &memout);
  // intermediate buffer for memcopies:
  create_buffer(pdevices[0],
                device,
                data_in_size > data_out_size ? data_in_size : data_out_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &bstaging,
                &memstaging);

  VkDescriptorBufferInfo dset_infos[4] = {{0}};
  dset_infos[0].buffer = ub;
  dset_infos[0].range = VK_WHOLE_SIZE;

  dset_infos[1].buffer = bin;
  dset_infos[1].range = VK_WHOLE_SIZE;

  dset_infos[2].buffer = bout;
  dset_infos[2].range = VK_WHOLE_SIZE;

  dset_infos[3].buffer = bout2;
  dset_infos[3].range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writes[8] = {{0}};
  // ping:
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = ds;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &dset_infos[0];

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = ds;
  writes[1].dstBinding = 1;
  writes[1].dstArrayElement = 0;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].descriptorCount = 1;
  writes[1].pBufferInfo = &dset_infos[1];

  writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[2].dstSet = ds;
  writes[2].dstBinding = 2;
  writes[2].dstArrayElement = 0;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[2].descriptorCount = 1;
  writes[2].pBufferInfo = &dset_infos[2];

  writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[3].dstSet = ds;
  writes[3].dstBinding = 3;
  writes[3].dstArrayElement = 0;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[3].descriptorCount = 1;
  writes[3].pBufferInfo = &dset_infos[3];

  // pong:
  writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[4].dstSet = ds2;
  writes[4].dstBinding = 0;
  writes[4].dstArrayElement = 0;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[4].descriptorCount = 1;
  writes[4].pBufferInfo = &dset_infos[0];

  writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[5].dstSet = ds2;
  writes[5].dstBinding = 1;
  writes[5].dstArrayElement = 0;
  writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[5].descriptorCount = 1;
  writes[5].pBufferInfo = &dset_infos[1];

  writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[6].dstSet = ds2;
  writes[6].dstBinding = 3;
  writes[6].dstArrayElement = 0;
  writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[6].descriptorCount = 1;
  writes[6].pBufferInfo = &dset_infos[2];

  writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[7].dstSet = ds2;
  writes[7].dstBinding = 2;
  writes[7].dstArrayElement = 0;
  writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[7].descriptorCount = 1;
  writes[7].pBufferInfo = &dset_infos[3];

  vkUpdateDescriptorSets(device, 8, writes, 0, 0);

  // copy input to device:
  void *pmapped;
  vkMapMemory(device, memstaging, 0, data_in_size, 0, &pmapped);
  memcpy(pmapped, data_in, data_in_size);
  vkUnmapMemory(device, memstaging);

  // submit to command buffer
  VkCommandBuffer cb = create_commandbuffer(device, pool);

  VkCommandBufferBeginInfo begin_info = {0};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cb, &begin_info);

  VkBufferCopy copyRegion = {};
  copyRegion.size = data_in_size;
  vkCmdCopyBuffer(cb, bstaging, bin, 1, &copyRegion);

  {
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = dqueues.compute.idx;
    barrier.dstQueueFamilyIndex = dqueues.compute.idx;
    barrier.buffer = bin;
    barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 1, &barrier, 0, 0);
  }

  for(int i=0;i<num_iterations;i++)
  {
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.pipeline);
    if(i&1) // pong
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.layout, 0, 1, &ds2, 0, 0);
    else    // ping
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe.layout, 0, 1, &ds, 0, 0);
    // copy uniform data
    data_uniform[0] = i;
    vkCmdUpdateBuffer(cb, ub, 0, data_uniform_size, data_uniform);

    vkCmdDispatch(cb, wd, ht, 1);

    {
      VkBufferMemoryBarrier barrier = {};
      barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      // transfer read only needed for last iteration really:
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
      barrier.srcQueueFamilyIndex = dqueues.compute.idx;
      barrier.dstQueueFamilyIndex = dqueues.compute.idx;
      if(i&1) barrier.buffer = bout2;
      else    barrier.buffer = bout;
      barrier.size = VK_WHOLE_SIZE;
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          0, 0, 0, 1, &barrier, 0, 0);
    }
  }

  // depending on even/odd, copy ping or pong buffer
  if((num_iterations-1)&1)
    vkCmdCopyBuffer(cb, bout2, bstaging, 1, &copyRegion);
  else
    vkCmdCopyBuffer(cb, bout, bstaging, 1, &copyRegion);
  vkEndCommandBuffer(cb);

  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cb;
  // don't need waiting/signaling semaphores

  VkFence fence;
  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  vkCreateFence(device, &fence_info, 0, &fence);

  vkQueueSubmit(dqueues.compute.que, 1, &submit, fence);

  vkWaitForFences(device, 1, &fence, VK_TRUE, 1ul<<40);
  vkDestroyFence(device, fence, 0);

  // copy result back:
  vkMapMemory(device, memstaging, 0, data_out_size, 0, &pmapped);
  memcpy(data_out, pmapped, data_out_size);
  vkUnmapMemory(device, memstaging);

  destroy_buffer(device, ub, memub);
  destroy_buffer(device, bin, memin);
  destroy_buffer(device, bout, memout);
  destroy_buffer(device, bout2, memout2);
  destroy_buffer(device, bstaging, memstaging);

  vkFreeCommandBuffers(device, pool, 1, &cb);

  destroy_commandpool(device, pool);
  destroy_pipeline(device, pipe);
  destroy_device(device);
  destroy_instance(instance);

  return 0;
}
