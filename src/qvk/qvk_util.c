/*
Copyright (C) 2018 Christoph Schied

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "qvk_util.h"
#include "qvk.h"

#include <assert.h>

uint32_t
qvk_memory_get_staging()
{
  for(uint32_t i = 0; i < qvk.mem_properties.memoryTypeCount; i++)
  {
    VkMemoryPropertyFlagBits f = qvk.mem_properties.memoryTypes[i].propertyFlags;
    int test =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|
      VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    if((f & test) == test) return i;
  }
	return 0;
}
uint32_t
qvk_memory_get_uniform()
{
	for(uint32_t i = 0; i < qvk.mem_properties.memoryTypeCount; i++)
  {
    VkMemoryPropertyFlagBits f = qvk.mem_properties.memoryTypes[i].propertyFlags;
    int test =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|
      VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    if((f & test) == test) return i;
  }
	return 0;
}
uint32_t
qvk_memory_get_device()
{
	for(uint32_t i = 0; i < qvk.mem_properties.memoryTypeCount; i++)
  {
    VkMemoryPropertyFlagBits f = qvk.mem_properties.memoryTypes[i].propertyFlags;
    int test  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;// | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT)
    int testn =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|
      VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    if(((f & test) == test) && ((f & testn) == 0)) return i;
  }
	return 0;
}

const char *
qvk_colourspace_to_string(VkColorSpaceKHR c)
{
  switch(c) {
  case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "sRGB nonlinear";
  case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "display P3";
  case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return "sRGB linear";
  case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT: return "display P3 linear";
  case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT: return "DCI P3 nonlinear";
  case VK_COLOR_SPACE_BT709_LINEAR_EXT: return "BT709 linear";
  case VK_COLOR_SPACE_BT709_NONLINEAR_EXT: return "BT709 nonlinear";
  case VK_COLOR_SPACE_BT2020_LINEAR_EXT: return "BT2020 linear";
  case VK_COLOR_SPACE_HDR10_ST2084_EXT: return "HDR10 ST2084";
  case VK_COLOR_SPACE_DOLBYVISION_EXT: return "dolbyvision (deprecated)";
  case VK_COLOR_SPACE_HDR10_HLG_EXT: return "HDR10 HLG";
  case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT: return "Adobe RGB linear";
  case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT: return "Adobe RGB nonlinear";
  case VK_COLOR_SPACE_PASS_THROUGH_EXT: return "pass through";
  case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT: return "ext sRGB nonlinear";
  case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD: return "display native";
  default: return "AAARRGHH";
  }
}

const char *
qvk_result_to_string(VkResult result)
{
  switch(result) {
  case VK_SUCCESS: return "VK_SUCCESS";
  case VK_NOT_READY: return "VK_NOT_READY";
  case VK_TIMEOUT: return "VK_TIMEOUT";
  case VK_EVENT_SET: return "VK_EVENT_SET";
  case VK_EVENT_RESET: return "VK_EVENT_RESET";
  case VK_INCOMPLETE: return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
  case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
  case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
  case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
  case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
  case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
  case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
  case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
  case VK_ERROR_FRAGMENTATION_EXT: return "VK_ERROR_FRAGMENTATION_EXT";
  case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
  case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT: return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
  case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
  case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM";
  default: return "AAARRGHH";
  };
}

const char *
qvk_format_to_string(VkFormat format)
{
	switch(format) {
	case  0: return "UNDEFINED";
	case  1: return "R4G4_UNORM_PACK8";
	case  2: return "R4G4B4A4_UNORM_PACK16";
	case  3: return "B4G4R4A4_UNORM_PACK16";
	case  4: return "R5G6B5_UNORM_PACK16";
	case  5: return "B5G6R5_UNORM_PACK16";
	case  6: return "R5G5B5A1_UNORM_PACK16";
	case  7: return "B5G5R5A1_UNORM_PACK16";
	case  8: return "A1R5G5B5_UNORM_PACK16";
	case  9: return "R8_UNORM";
	case  10: return "R8_SNORM";
	case  11: return "R8_USCALED";
	case  12: return "R8_SSCALED";
	case  13: return "R8_UINT";
	case  14: return "R8_SINT";
	case  15: return "R8_SRGB";
	case  16: return "R8G8_UNORM";
	case  17: return "R8G8_SNORM";
	case  18: return "R8G8_USCALED";
	case  19: return "R8G8_SSCALED";
	case  20: return "R8G8_UINT";
	case  21: return "R8G8_SINT";
	case  22: return "R8G8_SRGB";
	case  23: return "R8G8B8_UNORM";
	case  24: return "R8G8B8_SNORM";
	case  25: return "R8G8B8_USCALED";
	case  26: return "R8G8B8_SSCALED";
	case  27: return "R8G8B8_UINT";
	case  28: return "R8G8B8_SINT";
	case  29: return "R8G8B8_SRGB";
	case  30: return "B8G8R8_UNORM";
	case  31: return "B8G8R8_SNORM";
	case  32: return "B8G8R8_USCALED";
	case  33: return "B8G8R8_SSCALED";
	case  34: return "B8G8R8_UINT";
	case  35: return "B8G8R8_SINT";
	case  36: return "B8G8R8_SRGB";
	case  37: return "R8G8B8A8_UNORM";
	case  38: return "R8G8B8A8_SNORM";
	case  39: return "R8G8B8A8_USCALED";
	case  40: return "R8G8B8A8_SSCALED";
	case  41: return "R8G8B8A8_UINT";
	case  42: return "R8G8B8A8_SINT";
	case  43: return "R8G8B8A8_SRGB";
	case  44: return "B8G8R8A8_UNORM";
	case  45: return "B8G8R8A8_SNORM";
	case  46: return "B8G8R8A8_USCALED";
	case  47: return "B8G8R8A8_SSCALED";
	case  48: return "B8G8R8A8_UINT";
	case  49: return "B8G8R8A8_SINT";
	case  50: return "B8G8R8A8_SRGB";
	case  51: return "A8B8G8R8_UNORM_PACK32";
	case  52: return "A8B8G8R8_SNORM_PACK32";
	case  53: return "A8B8G8R8_USCALED_PACK32";
	case  54: return "A8B8G8R8_SSCALED_PACK32";
	case  55: return "A8B8G8R8_UINT_PACK32";
	case  56: return "A8B8G8R8_SINT_PACK32";
	case  57: return "A8B8G8R8_SRGB_PACK32";
	case  58: return "A2R10G10B10_UNORM_PACK32";
	case  59: return "A2R10G10B10_SNORM_PACK32";
	case  60: return "A2R10G10B10_USCALED_PACK32";
	case  61: return "A2R10G10B10_SSCALED_PACK32";
	case  62: return "A2R10G10B10_UINT_PACK32";
	case  63: return "A2R10G10B10_SINT_PACK32";
	case  64: return "A2B10G10R10_UNORM_PACK32";
	case  65: return "A2B10G10R10_SNORM_PACK32";
	case  66: return "A2B10G10R10_USCALED_PACK32";
	case  67: return "A2B10G10R10_SSCALED_PACK32";
	case  68: return "A2B10G10R10_UINT_PACK32";
	case  69: return "A2B10G10R10_SINT_PACK32";
	case  70: return "R16_UNORM";
	case  71: return "R16_SNORM";
	case  72: return "R16_USCALED";
	case  73: return "R16_SSCALED";
	case  74: return "R16_UINT";
	case  75: return "R16_SINT";
	case  76: return "R16_SFLOAT";
	case  77: return "R16G16_UNORM";
	case  78: return "R16G16_SNORM";
	case  79: return "R16G16_USCALED";
	case  80: return "R16G16_SSCALED";
	case  81: return "R16G16_UINT";
	case  82: return "R16G16_SINT";
	case  83: return "R16G16_SFLOAT";
	case  84: return "R16G16B16_UNORM";
	case  85: return "R16G16B16_SNORM";
	case  86: return "R16G16B16_USCALED";
	case  87: return "R16G16B16_SSCALED";
	case  88: return "R16G16B16_UINT";
	case  89: return "R16G16B16_SINT";
	case  90: return "R16G16B16_SFLOAT";
	case  91: return "R16G16B16A16_UNORM";
	case  92: return "R16G16B16A16_SNORM";
	case  93: return "R16G16B16A16_USCALED";
	case  94: return "R16G16B16A16_SSCALED";
	case  95: return "R16G16B16A16_UINT";
	case  96: return "R16G16B16A16_SINT";
	case  97: return "R16G16B16A16_SFLOAT";
	case  98: return "R32_UINT";
	case  99: return "R32_SINT";
	case  100: return "R32_SFLOAT";
	case  101: return "R32G32_UINT";
	case  102: return "R32G32_SINT";
	case  103: return "R32G32_SFLOAT";
	case  104: return "R32G32B32_UINT";
	case  105: return "R32G32B32_SINT";
	case  106: return "R32G32B32_SFLOAT";
	case  107: return "R32G32B32A32_UINT";
	case  108: return "R32G32B32A32_SINT";
	case  109: return "R32G32B32A32_SFLOAT";
	case  110: return "R64_UINT";
	case  111: return "R64_SINT";
	case  112: return "R64_SFLOAT";
	case  113: return "R64G64_UINT";
	case  114: return "R64G64_SINT";
	case  115: return "R64G64_SFLOAT";
	case  116: return "R64G64B64_UINT";
	case  117: return "R64G64B64_SINT";
	case  118: return "R64G64B64_SFLOAT";
	case  119: return "R64G64B64A64_UINT";
	case  120: return "R64G64B64A64_SINT";
	case  121: return "R64G64B64A64_SFLOAT";
	case  122: return "B10G11R11_UFLOAT_PACK32";
	case  123: return "E5B9G9R9_UFLOAT_PACK32";
	case  124: return "D16_UNORM";
	case  125: return "X8_D24_UNORM_PACK32";
	case  126: return "D32_SFLOAT";
	case  127: return "S8_UINT";
	case  128: return "D16_UNORM_S8_UINT";
	case  129: return "D24_UNORM_S8_UINT";
	case  130: return "D32_SFLOAT_S8_UINT";
	case  131: return "BC1_RGB_UNORM_BLOCK";
	case  132: return "BC1_RGB_SRGB_BLOCK";
	case  133: return "BC1_RGBA_UNORM_BLOCK";
	case  134: return "BC1_RGBA_SRGB_BLOCK";
	case  135: return "BC2_UNORM_BLOCK";
	case  136: return "BC2_SRGB_BLOCK";
	case  137: return "BC3_UNORM_BLOCK";
	case  138: return "BC3_SRGB_BLOCK";
	case  139: return "BC4_UNORM_BLOCK";
	case  140: return "BC4_SNORM_BLOCK";
	case  141: return "BC5_UNORM_BLOCK";
	case  142: return "BC5_SNORM_BLOCK";
	case  143: return "BC6H_UFLOAT_BLOCK";
	case  144: return "BC6H_SFLOAT_BLOCK";
	case  145: return "BC7_UNORM_BLOCK";
	case  146: return "BC7_SRGB_BLOCK";
	case  147: return "ETC2_R8G8B8_UNORM_BLOCK";
	case  148: return "ETC2_R8G8B8_SRGB_BLOCK";
	case  149: return "ETC2_R8G8B8A1_UNORM_BLOCK";
	case  150: return "ETC2_R8G8B8A1_SRGB_BLOCK";
	case  151: return "ETC2_R8G8B8A8_UNORM_BLOCK";
	case  152: return "ETC2_R8G8B8A8_SRGB_BLOCK";
	case  153: return "EAC_R11_UNORM_BLOCK";
	case  154: return "EAC_R11_SNORM_BLOCK";
	case  155: return "EAC_R11G11_UNORM_BLOCK";
	case  156: return "EAC_R11G11_SNORM_BLOCK";
	case  157: return "ASTC_4x4_UNORM_BLOCK";
	case  158: return "ASTC_4x4_SRGB_BLOCK";
	case  159: return "ASTC_5x4_UNORM_BLOCK";
	case  160: return "ASTC_5x4_SRGB_BLOCK";
	case  161: return "ASTC_5x5_UNORM_BLOCK";
	case  162: return "ASTC_5x5_SRGB_BLOCK";
	case  163: return "ASTC_6x5_UNORM_BLOCK";
	case  164: return "ASTC_6x5_SRGB_BLOCK";
	case  165: return "ASTC_6x6_UNORM_BLOCK";
	case  166: return "ASTC_6x6_SRGB_BLOCK";
	case  167: return "ASTC_8x5_UNORM_BLOCK";
	case  168: return "ASTC_8x5_SRGB_BLOCK";
	case  169: return "ASTC_8x6_UNORM_BLOCK";
	case  170: return "ASTC_8x6_SRGB_BLOCK";
	case  171: return "ASTC_8x8_UNORM_BLOCK";
	case  172: return "ASTC_8x8_SRGB_BLOCK";
	case  173: return "ASTC_10x5_UNORM_BLOCK";
	case  174: return "ASTC_10x5_SRGB_BLOCK";
	case  175: return "ASTC_10x6_UNORM_BLOCK";
	case  176: return "ASTC_10x6_SRGB_BLOCK";
	case  177: return "ASTC_10x8_UNORM_BLOCK";
	case  178: return "ASTC_10x8_SRGB_BLOCK";
	case  179: return "ASTC_10x10_UNORM_BLOCK";
	case  180: return "ASTC_10x10_SRGB_BLOCK";
	case  181: return "ASTC_12x10_UNORM_BLOCK";
	case  182: return "ASTC_12x10_SRGB_BLOCK";
	case  183: return "ASTC_12x12_UNORM_BLOCK";
	case  184: return "ASTC_12x12_SRGB_BLOCK";
	case  1000156000: return "G8B8G8R8_422_UNORM";
	case  1000156001: return "B8G8R8G8_422_UNORM";
	case  1000156002: return "G8_B8_R8_3PLANE_420_UNORM";
	case  1000156003: return "G8_B8R8_2PLANE_420_UNORM";
	case  1000156004: return "G8_B8_R8_3PLANE_422_UNORM";
	case  1000156005: return "G8_B8R8_2PLANE_422_UNORM";
	case  1000156006: return "G8_B8_R8_3PLANE_444_UNORM";
	case  1000156007: return "R10X6_UNORM_PACK16";
	case  1000156008: return "R10X6G10X6_UNORM_2PACK16";
	case  1000156009: return "R10X6G10X6B10X6A10X6_UNORM_4PACK16";
	case  1000156010: return "G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
	case  1000156011: return "B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
	case  1000156012: return "G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
	case  1000156013: return "G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
	case  1000156014: return "G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
	case  1000156015: return "G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
	case  1000156016: return "G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
	case  1000156017: return "R12X4_UNORM_PACK16";
	case  1000156018: return "R12X4G12X4_UNORM_2PACK16";
	case  1000156019: return "R12X4G12X4B12X4A12X4_UNORM_4PACK16";
	case  1000156020: return "G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
	case  1000156021: return "B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
	case  1000156022: return "G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
	case  1000156023: return "G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
	case  1000156024: return "G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
	case  1000156025: return "G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
	case  1000156026: return "G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
	case  1000156027: return "G16B16G16R16_422_UNORM";
	case  1000156028: return "B16G16R16G16_422_UNORM";
	case  1000156029: return "G16_B16_R16_3PLANE_420_UNORM";
	case  1000156030: return "G16_B16R16_2PLANE_420_UNORM";
	case  1000156031: return "G16_B16_R16_3PLANE_422_UNORM";
	case  1000156032: return "G16_B16R16_2PLANE_422_UNORM";
	case  1000156033: return "G16_B16_R16_3PLANE_444_UNORM";
	case  1000054000: return "PVRTC1_2BPP_UNORM_BLOCK_IMG";
	case  1000054001: return "PVRTC1_4BPP_UNORM_BLOCK_IMG";
	case  1000054002: return "PVRTC2_2BPP_UNORM_BLOCK_IMG";
	case  1000054003: return "PVRTC2_4BPP_UNORM_BLOCK_IMG";
	case  1000054004: return "PVRTC1_2BPP_SRGB_BLOCK_IMG";
	case  1000054005: return "PVRTC1_4BPP_SRGB_BLOCK_IMG";
	case  1000054006: return "PVRTC2_2BPP_SRGB_BLOCK_IMG";
	case  1000054007: return "PVRTC2_4BPP_SRGB_BLOCK_IMG";
	default: break;
	}
	return "";
}
