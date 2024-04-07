#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void __debug_save_texture_png(gs_stagesurf_t *stagesurf, uint32_t cx, uint32_t cy, const char* path) {
    uint8_t *videoData = new uint8_t[cx*cy*4];
	uint32_t videoLinesize = 0;
	gs_stagesurface_map(stagesurf, &videoData, &videoLinesize);

    //gs_texture_map(texture, videoData, &videoLinesize);
    stbi_write_png(path, cx, cy, 4, videoData, 0);
    delete videoData;
}

void __debug_save_texture_png(gs_texture_t *texture, uint32_t cx, uint32_t cy, const char* path) {
	//obs_enter_graphics();
	gs_stagesurf_t *stagesurf = gs_stagesurface_create(cx, cy, GS_RGBA);
	gs_stage_texture(stagesurf, texture);

	__debug_save_texture_png(stagesurf, cx, cy, path);

    gs_stagesurface_unmap(stagesurf);
	gs_stagesurface_destroy(stagesurf);
	//obs_leave_graphics();
}
