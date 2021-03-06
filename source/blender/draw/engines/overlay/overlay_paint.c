/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_mesh_types.h"

#include "DEG_depsgraph_query.h"

#include "overlay_private.h"

void OVERLAY_paint_cache_init(OVERLAY_Data *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  struct GPUShader *sh;
  DRWShadingGroup *grp;
  DRWState state;

  const bool use_alpha_blending = (draw_ctx->v3d->shading.type == OB_WIRE);
  const bool draw_contours = (pd->overlay.wpaint_flag & V3D_OVERLAY_WPAINT_CONTOURS) != 0;
  float opacity = 0.0f;

  switch (pd->ctx_mode) {
    case CTX_MODE_POSE:
    case CTX_MODE_PAINT_WEIGHT: {
      opacity = pd->overlay.weight_paint_mode_opacity;
      if (opacity > 0.0f) {
        state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
        state |= use_alpha_blending ? DRW_STATE_BLEND_ALPHA : DRW_STATE_BLEND_MUL;
        DRW_PASS_CREATE(psl->paint_color_ps, state | pd->clipping_state);

        sh = OVERLAY_shader_paint_weight();
        pd->paint_surf_grp = grp = DRW_shgroup_create(sh, psl->paint_color_ps);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_bool_copy(grp, "drawContours", draw_contours);
        DRW_shgroup_uniform_bool_copy(grp, "useAlphaBlend", use_alpha_blending);
        DRW_shgroup_uniform_float_copy(grp, "opacity", opacity);
        DRW_shgroup_uniform_texture(grp, "colorramp", G_draw.weight_ramp);
      }
      break;
    }
    case CTX_MODE_PAINT_VERTEX: {
      opacity = pd->overlay.vertex_paint_mode_opacity;
      if (opacity > 0.0f) {
        state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
        state |= use_alpha_blending ? DRW_STATE_BLEND_ALPHA : DRW_STATE_BLEND_MUL;
        DRW_PASS_CREATE(psl->paint_color_ps, state | pd->clipping_state);

        sh = OVERLAY_shader_paint_vertcol();
        pd->paint_surf_grp = grp = DRW_shgroup_create(sh, psl->paint_color_ps);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_bool_copy(grp, "useAlphaBlend", use_alpha_blending);
        DRW_shgroup_uniform_float_copy(grp, "opacity", opacity);
      }
      break;
    }
    case CTX_MODE_PAINT_TEXTURE: {
      const ImagePaintSettings *imapaint = &draw_ctx->scene->toolsettings->imapaint;
      const bool mask_enabled = imapaint->flag & IMAGEPAINT_PROJECT_LAYER_STENCIL &&
                                imapaint->stencil != NULL;

      opacity = mask_enabled ? pd->overlay.texture_paint_mode_opacity : 0.0f;
      if (opacity > 0.0f) {
        state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
        DRW_PASS_CREATE(psl->paint_color_ps, state | pd->clipping_state);

        GPUTexture *tex = GPU_texture_from_blender(imapaint->stencil, NULL, GL_TEXTURE_2D);

        const bool mask_premult = (imapaint->stencil->alpha_mode == IMA_ALPHA_PREMUL);
        const bool mask_inverted = (imapaint->flag & IMAGEPAINT_PROJECT_LAYER_STENCIL_INV) != 0;
        sh = OVERLAY_shader_paint_texture();
        pd->paint_surf_grp = grp = DRW_shgroup_create(sh, psl->paint_color_ps);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "opacity", opacity);
        DRW_shgroup_uniform_bool_copy(grp, "maskPremult", mask_premult);
        DRW_shgroup_uniform_vec3_copy(grp, "maskColor", imapaint->stencil_col);
        DRW_shgroup_uniform_bool_copy(grp, "maskInvertStencil", mask_inverted);
        DRW_shgroup_uniform_texture(grp, "maskImage", tex);
      }
      break;
    }
    default:
      BLI_assert(0);
      break;
  }

  if (opacity <= 0.0f) {
    psl->paint_color_ps = NULL;
    pd->paint_surf_grp = NULL;
  }

  {
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRW_PASS_CREATE(psl->paint_overlay_ps, state | pd->clipping_state);
    sh = OVERLAY_shader_paint_face();
    pd->paint_face_grp = grp = DRW_shgroup_create(sh, psl->paint_overlay_ps);
    DRW_shgroup_uniform_vec4_copy(grp, "color", (float[4]){1.0f, 1.0f, 1.0f, 0.2f});
    DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);

    sh = OVERLAY_shader_paint_wire();
    pd->paint_wire_selected_grp = grp = DRW_shgroup_create(sh, psl->paint_overlay_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_bool_copy(grp, "useSelect", true);
    DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);

    pd->paint_wire_grp = grp = DRW_shgroup_create(sh, psl->paint_overlay_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_bool_copy(grp, "useSelect", false);
    DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);

    sh = OVERLAY_shader_paint_point();
    pd->paint_point_grp = grp = DRW_shgroup_create(sh, psl->paint_overlay_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  }
}

void OVERLAY_paint_texture_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  struct GPUBatch *geom = NULL;

  const Mesh *me_orig = DEG_get_original_object(ob)->data;
  const bool use_face_sel = (me_orig->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;

  if (pd->paint_surf_grp) {
    geom = DRW_cache_mesh_surface_texpaint_single_get(ob);
    DRW_shgroup_call(pd->paint_surf_grp, geom, ob);
  }

  if (use_face_sel) {
    geom = DRW_cache_mesh_surface_get(ob);
    DRW_shgroup_call(pd->paint_face_grp, geom, ob);
  }
}

void OVERLAY_paint_vertex_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  struct GPUBatch *geom = NULL;

  const Mesh *me_orig = DEG_get_original_object(ob)->data;
  const bool use_wire = (pd->overlay.paint_flag & V3D_OVERLAY_PAINT_WIRE) != 0;
  const bool use_face_sel = (me_orig->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const bool use_vert_sel = (me_orig->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

  if (pd->paint_surf_grp) {
    if (ob->mode == OB_MODE_WEIGHT_PAINT) {
      geom = DRW_cache_mesh_surface_weights_get(ob);
      DRW_shgroup_call(pd->paint_surf_grp, geom, ob);
    }
  }

  if (use_face_sel || use_wire) {
    geom = DRW_cache_mesh_surface_edges_get(ob);
    DRW_shgroup_call(use_face_sel ? pd->paint_wire_selected_grp : pd->paint_wire_grp, geom, ob);
  }

  if (use_face_sel) {
    geom = DRW_cache_mesh_surface_get(ob);
    DRW_shgroup_call(pd->paint_face_grp, geom, ob);
  }

  if (use_vert_sel) {
    geom = DRW_cache_mesh_all_verts_get(ob);
    DRW_shgroup_call(pd->paint_point_grp, geom, ob);
  }
}

void OVERLAY_paint_weight_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_paint_vertex_cache_populate(vedata, ob);
}

void OVERLAY_paint_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (DRW_state_is_fbo()) {
    /* Pain overlay needs final color because of multiply blend mode. */
    GPU_framebuffer_bind(dfbl->default_fb);
  }

  if (psl->paint_color_ps) {
    DRW_draw_pass(psl->paint_color_ps);
  }
  DRW_draw_pass(psl->paint_overlay_ps);
}
