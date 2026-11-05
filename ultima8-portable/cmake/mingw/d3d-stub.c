// mingw builds: libmatoya's Direct3D back ends need shaders compiled by Microsoft's fxc, so
// they are left out and report that they cannot start; the port then draws with Vulkan.
#include "matoya.h"

#include "gfx/mod.h"
#include "gfx/mod-ctx.h"
#include "gfx/mod-ui.h"

#define STUB_GFX(api) \
	struct gfx *mty##api##create(MTY_Device *d, uint8_t l) { (void) d; (void) l; return NULL; } \
	void mty##api##destroy(struct gfx **g, MTY_Device *d) { (void) g; (void) d; } \
	bool mty##api##render(struct gfx *g, MTY_Device *d, MTY_Context *c, const void *i, \
		const MTY_RenderDesc *r, MTY_Surface *s) { (void) g; (void) d; (void) c; (void) i; (void) r; (void) s; return false; } \
	void mty##api##clear(struct gfx *g, MTY_Device *d, MTY_Context *c, uint32_t w, uint32_t h, \
		float r, float gr, float b, float a, MTY_Surface *s) { (void) g; (void) d; (void) c; (void) w; (void) h; \
		(void) r; (void) gr; (void) b; (void) a; (void) s; } \
	struct gfx_ctx *mty##api##ctx_create(void *w, bool v) { (void) w; (void) v; return NULL; } \
	void mty##api##ctx_destroy(struct gfx_ctx **c) { (void) c; } \
	void mty##api##ctx_present(struct gfx_ctx *c) { (void) c; } \
	void mty##api##ctx_get_size(struct gfx_ctx *c, uint32_t *w, uint32_t *h) { (void) c; *w = *h = 0; } \
	MTY_Device *mty##api##ctx_get_device(struct gfx_ctx *c) { (void) c; return NULL; } \
	MTY_Context *mty##api##ctx_get_context(struct gfx_ctx *c) { (void) c; return NULL; } \
	MTY_Surface *mty##api##ctx_get_surface(struct gfx_ctx *c) { (void) c; return NULL; } \
	void mty##api##ctx_set_sync_interval(struct gfx_ctx *c, uint32_t i) { (void) c; (void) i; } \
	bool mty##api##ctx_lock(struct gfx_ctx *c) { (void) c; return false; } \
	void mty##api##ctx_unlock(void) {} \
	struct gfx_ui *mty##api##ui_create(MTY_Device *d) { (void) d; return NULL; } \
	void mty##api##ui_destroy(struct gfx_ui **u, MTY_Device *d) { (void) u; (void) d; } \
	bool mty##api##ui_render(struct gfx_ui *u, MTY_Device *d, MTY_Context *c, const MTY_DrawData *dd, \
		MTY_Hash *h, MTY_Surface *s) { (void) u; (void) d; (void) c; (void) dd; (void) h; (void) s; return false; } \
	void *mty##api##ui_create_texture(struct gfx_ui *u, MTY_Device *d, const void *p, uint32_t w, uint32_t h) \
		{ (void) u; (void) d; (void) p; (void) w; (void) h; return NULL; } \
	void mty##api##ui_destroy_texture(struct gfx_ui *u, void **t, MTY_Device *d) { (void) u; (void) t; (void) d; }

STUB_GFX(_d3d11_)
STUB_GFX(_d3d12_)
