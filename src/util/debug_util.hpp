#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void __debug_save_texture_png(gs_texture_t *texture, uint32_t cx, uint32_t cy, const char* path) {
	//obs_enter_graphics();
    const enum gs_color_space space = GS_CS_SRGB;
	const enum gs_color_format format = gs_get_format_from_space(space);
	gs_stagesurf_t *stagesurf = gs_stagesurface_create(cx, cy, GS_RGBA);

    uint8_t *videoData = nullptr;
	uint32_t videoLinesize = 0;
	gs_stage_texture(stagesurf, texture);
	gs_stagesurface_map(stagesurf, &videoData, &videoLinesize);

    //gs_texture_map(texture, videoData, &videoLinesize);
    stbi_write_png(path, cx, cy, 4, videoData, 0);

    gs_stagesurface_unmap(stagesurf);
	gs_stagesurface_destroy(stagesurf);
	//obs_leave_graphics();
}
