const char* gsColorFormatToString(gs_color_format format) {
    switch (format) {
        case GS_UNKNOWN:
            return "GS_UNKNOWN";
        case GS_A8:
            return "GS_A8";
        case GS_R8:
            return "GS_R8";
        case GS_RGBA:
            return "GS_RGBA";
        case GS_BGRX:
            return "GS_BGRX";
        case GS_BGRA:
            return "GS_BGRA";
        case GS_R10G10B10A2:
            return "GS_R10G10B10A2";
        case GS_RGBA16:
            return "GS_RGBA16";
        case GS_R16:
            return "GS_R16";
        case GS_RGBA16F:
            return "GS_RGBA16F";
        case GS_RGBA32F:
            return "GS_RGBA32F";
        case GS_RG16F:
            return "GS_RG16F";
        case GS_RG32F:
            return "GS_RG32F";
        case GS_R16F:
            return "GS_R16F";
        case GS_R32F:
            return "GS_R32F";
        case GS_DXT1:
            return "GS_DXT1";
        case GS_DXT3:
            return "GS_DXT3";
        case GS_DXT5:
            return "GS_DXT5";
        case GS_R8G8:
            return "GS_R8G8";
        case GS_RGBA_UNORM:
            return "GS_RGBA_UNORM";
        case GS_BGRX_UNORM:
            return "GS_BGRX_UNORM";
        case GS_BGRA_UNORM:
            return "GS_BGRA_UNORM";
        case GS_RG16:
            return "GS_RG16";
        default:
            return "Unknown Format";
    }
}

const char* gsColorSpaceToString(gs_color_space space) {
    switch (space) {
        case GS_CS_SRGB:
            return "GS_CS_SRGB";
        case GS_CS_SRGB_16F:
            return "GS_CS_SRGB_16F";
        case GS_CS_709_EXTENDED:
            return "GS_CS_709_EXTENDED";
        case GS_CS_709_SCRGB:
            return "GS_CS_709_SCRGB";
        default:
            return "Unknown Color Space";
    }
}
