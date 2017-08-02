//------------------------------------------------------------------------------
//  offscreen-emsc.c
//------------------------------------------------------------------------------
#include <stddef.h>     /* offsetof */
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "HandmadeMath.h"
#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol_gfx.h"

const int WIDTH = 640;
const int HEIGHT = 480;
sg_pass offscreen_pass;
sg_draw_state offscreen_draw_state;
sg_draw_state default_draw_state;
sg_pass_action offscreen_pass_action;
sg_pass_action default_pass_action;
float rx = 0.0f;
float ry = 0.0f;
hmm_mat4 view_proj;

typedef struct {
    hmm_mat4 mvp;
} params_t;

void draw();

int main() {
    /* setup WebGL context */
    emscripten_set_canvas_size(WIDTH, HEIGHT);
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.antialias = true;
    ctx = emscripten_webgl_create_context(0, &attrs);
    emscripten_webgl_make_context_current(ctx);

    /* setup sokol_gfx */
    sg_desc desc = {0};
    sg_setup(&desc);
    assert(sg_isvalid());

    /* pass action for default pass, clearing to blue-ish */
    sg_init_pass_action(&default_pass_action);
    default_pass_action.color[0][0] = 0.0f;
    default_pass_action.color[0][1] = 0.25f;
    default_pass_action.color[0][2] = 1.0f;

    /* create one color- and one depth-rendertarget image */
    sg_image_desc img_desc = {
        .render_target = true,
        .width = 512,
        .height = 512,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .sample_count = sg_query_feature(SG_FEATURE_MSAA_RENDER_TARGETS) ? 4 : 1
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    sg_image depth_img = sg_make_image(&img_desc);

    /* an offscreen render pass into those images */
    offscreen_pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = color_img,
        .depth_stencil_attachment.image = depth_img
    });

    /* pass action for offscreen pass, clearing to black */ 
    sg_init_pass_action(&offscreen_pass_action);
    offscreen_pass_action.color[0][0] = 0.0f;
    offscreen_pass_action.color[0][1] = 0.0f;
    offscreen_pass_action.color[0][2] = 0.0f;

    /* cube vertex buffer with positions, colors and tex coords */
    float vertices[] = {
        /* pos                  color                       uvs */
        -1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.5f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     0.0f, 0.0f, 
         1.0f, -1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.5f, 1.0f, 0.5f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.5f, 0.5f, 1.0f, 1.0f,     0.0f, 1.0f,

         1.0f, -1.0f, -1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.5f, 0.0f, 1.0f,     0.0f, 1.0f,

        -1.0f, -1.0f, -1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     1.0f, 1.0f,
         1.0f, -1.0f, -1.0f,    0.0f, 0.5f, 1.0f, 1.0f,     0.0f, 1.0f,

        -1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 0.0f, 0.5f, 1.0f,     0.0f, 1.0f
    };
    sg_buffer_desc vbuf_desc = {
        .size = sizeof(vertices),
        .data_ptr = vertices,
        .data_size = sizeof(vertices)
    };
    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);

    /* an index buffer for the cube */
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer_desc ibuf_desc = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .data_ptr = indices,
        .data_size = sizeof(indices)
    };
    sg_buffer ibuf = sg_make_buffer(&ibuf_desc);

    /* shader for the non-textured cube, rendered in the offscreen pass */
    sg_shader_desc shd_desc;
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc, SG_SHADERSTAGE_VS, sizeof(params_t));
    sg_init_named_uniform(&shd_desc, SG_SHADERSTAGE_VS, "mvp", offsetof(params_t, mvp), SG_UNIFORMTYPE_MAT4, 1);
    shd_desc.vs.source = 
        "uniform mat4 mvp;\n"
        "attribute vec4 position;\n"
        "attribute vec4 color0;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  gl_Position = mvp * position;\n"
        "  color = color0;\n"
        "}\n";
    shd_desc.fs.source =
        "precision mediump float;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "  gl_FragColor = color;\n"
        "}\n";
    sg_shader offscreen_shd = sg_make_shader(&shd_desc);

    /* ...and a second shader for rendering a textured cube in the default pass */
    sg_init_shader_desc(&shd_desc);
    sg_init_uniform_block(&shd_desc, SG_SHADERSTAGE_VS, sizeof(params_t));
    sg_init_named_uniform(&shd_desc, SG_SHADERSTAGE_VS, "mvp", offsetof(params_t, mvp), SG_UNIFORMTYPE_MAT4, 1);
    sg_init_named_image(&shd_desc, SG_SHADERSTAGE_FS, "tex", SG_IMAGETYPE_2D);
    shd_desc.vs.source = 
        "uniform mat4 mvp;\n"
        "attribute vec4 position;\n"
        "attribute vec4 color0;\n"
        "attribute vec2 texcoord0;\n"
        "varying vec4 color;\n"
        "varying vec2 uv;\n"
        "void main() {\n"
        "  gl_Position = mvp * position;\n"
        "  color = color0;\n"
        "  uv = texcoord0;\n"
        "}\n";
    shd_desc.fs.source =
        "precision mediump float;"
        "uniform sampler2D tex;\n"
        "varying vec4 color;\n"
        "varying vec2 uv;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(tex, uv) + color * 0.5;\n"
        "}\n";
    sg_shader default_shd = sg_make_shader(&shd_desc);

    /* pipeline object for offscreen rendering, don't need texcoords here */
    sg_pipeline_desc pip_desc;
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 36);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", 12, SG_VERTEXFORMAT_FLOAT4);
    pip_desc.shader = offscreen_shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth_stencil.depth_write_enabled = true;
    pip_desc.rast.cull_mode = SG_CULLMODE_BACK;
    sg_pipeline offscreen_pip = sg_make_pipeline(&pip_desc);

    /* and another pipeline object for the default pass */
    sg_init_pipeline_desc(&pip_desc);
    sg_init_vertex_stride(&pip_desc, 0, 36);
    sg_init_named_vertex_attr(&pip_desc, 0, "position", 0, SG_VERTEXFORMAT_FLOAT3);
    sg_init_named_vertex_attr(&pip_desc, 0, "color0", 12, SG_VERTEXFORMAT_FLOAT4);
    sg_init_named_vertex_attr(&pip_desc, 0, "texcoord0", 28, SG_VERTEXFORMAT_FLOAT2);
    pip_desc.shader = default_shd;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth_stencil.depth_write_enabled = true;
    pip_desc.rast.cull_mode = SG_CULLMODE_BACK;
    sg_pipeline default_pip = sg_make_pipeline(&pip_desc);

    /* the draw state for offscreen rendering with all the required resources */
    offscreen_draw_state = (sg_draw_state){
        .pipeline = offscreen_pip,
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    /* and the draw state for the default pass where a textured cube will
       rendered, note how the color rendertarget image is used as texture here */
    default_draw_state = (sg_draw_state){
        .pipeline = default_pip,
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = color_img
    };

    /* view-projection matrix */
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)WIDTH/(float)HEIGHT, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    view_proj = HMM_MultiplyMat4(proj, view);

    /* hand off control to browser loop */
    emscripten_set_main_loop(draw, 0, 1);
    return 0;
}

void draw() {
    /* prepare the uniform block with the model-view-projection matrix,
        we just use the same matrix for the offscreen- and default-pass */
    params_t vs_params;
    rx += 1.0f; ry += 2.0f;
    hmm_mat4 model = HMM_MultiplyMat4(
        HMM_Rotate(rx, HMM_Vec3(1.0f, 0.0f, 0.0f)),
        HMM_Rotate(ry, HMM_Vec3(0.0f, 1.0f, 0.0f)));
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);

    /* offscreen pass, this renders a rotating, untextured cube to the
        offscreen render target */
    sg_begin_pass(offscreen_pass, &offscreen_pass_action);
    sg_apply_draw_state(&offscreen_draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();

    /* and the default pass, this renders a textured cube, using the 
        offscreen render target as texture image */
    sg_begin_default_pass(&default_pass_action, WIDTH, HEIGHT);
    sg_apply_draw_state(&default_draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}
