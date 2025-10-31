#include "cglm/mat4.h"
#include "cglm/vec3.h"
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <glad/gl.h>

#include <cglm/cglm.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


/*
** Macros
*/

#define PROJECT_NAME "022_f"
#define ASSETS_FILE_PATH "assets.bin"
#define MAX_PATH_LENGTH 512

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_DEBUG(format_string, ...) \
    SDL_Log("debug:    %s:%d %s - " format_string "\n", __FILENAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_INFO(format_string, ...) \
    SDL_Log("info:     %s:%d %s - " format_string "\n", __FILENAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_WARNING(format_string, ...) \
    SDL_Log("warning:  %s:%d %s - " format_string "\n", __FILENAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_ERROR(format_string, ...) \
    SDL_Log("error:    %s:%d %s - " format_string "\n", __FILENAME__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_CRITICAL(format_string, ...) \
    SDL_Log("critical: %s:%d %s - " format_string "\n", __FILENAME__, __LINE__, __func__, ##__VA_ARGS__)


/*
** Structs
*/

typedef struct {
    char* path;
    size_t offset;
    size_t size;
} FileEntry;

typedef struct {
    SDL_Window *window;
    SDL_GLContext gl;
    
    int width;
    int height;

    int target_refresh_rate;
    int target_frame_delay_ms;
} Display;

typedef struct {
    vec3 position;
    vec3 rotation;

    vec3 up;
    vec3 front;
    vec3 right;
    vec3 target;
} Camera;

typedef struct {
    GLuint VAO;
    GLuint VBO;
    int vertex_count;
} Mesh;

typedef struct {
    uint64_t tick;
    Camera cam;

    Mesh mesh;
    GLuint shader;
} Game;

struct Context {
    Display display;
    Game g;

    SDL_IOStream* assets_io;
    FileEntry* assets_io_files;
    int assets_io_files_count;
    bool* keyboard_state;
} ctx = {0};



/*
** Declarations
*/

bool init_engine();
bool init_game();

void quit_engine();
void quit_game();


FileEntry* io_get_file_entry(const char* path);

char* io_read_text_file(const char* path);
GLuint io_load_texture(const char* path, GLint wrap_mode, GLint min_filter_mode, GLint mag_filter_mode, GLenum texture_format, bool flip_y, int* out_width, int* out_height);
void io_load_mesh_mdl(const char* path, Mesh* dest);

GLuint create_generic_shader(char* vertex_shader_source, char* fragment_shader_source);

void view_mat_from_cam(Camera* cam, mat4 dest);

char* bytes_to_human_readable(size_t bytes);
char* str_new_formatted(const char* fmt, ...);

/*
** Implementation
*/

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata(PROJECT_NAME, "0.0.0", "dev.ivan_reshetnikov." PROJECT_NAME);

    if (!init_engine()) {
        LOG_CRITICAL("Failed to initialise engine!");
        return SDL_APP_FAILURE;
    }

    if (!init_game()) {
        LOG_CRITICAL("Failed to initialise game!");
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_QUIT: {
            return SDL_APP_SUCCESS;
        } break;
        case SDL_EVENT_MOUSE_MOTION: {
            ctx.g.cam.rotation[1] += event->motion.xrel * 0.075;
            ctx.g.cam.rotation[0] += -event->motion.yrel * 0.075;

            ctx.g.cam.rotation[0] = glm_clamp(ctx.g.cam.rotation[0], -89.0f, 89.0f);
        } break;
    }

    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppIterate(void *appstate)
{
    /* Delta */
    float delta = 0.0;
    {
        uint64_t tick = SDL_GetTicks();
        delta = (float)(tick - ctx.g.tick) / (float)(1000.0f / ctx.display.target_refresh_rate);
        ctx.g.tick = tick;
    }

    /* Update */
    mat4 view_mat;
    mat4 proj_mat;
    {
        // Movement
        float speed = 0.25f;
        vec3 move_speed_vec = {1.0f * speed * delta, 1.0f * speed * delta, 1.0f * speed * delta};
        if (ctx.keyboard_state[SDL_SCANCODE_W]) glm_vec3_muladd(ctx.g.cam.front, move_speed_vec, ctx.g.cam.position);
        if (ctx.keyboard_state[SDL_SCANCODE_S]) glm_vec3_mulsub(ctx.g.cam.front, move_speed_vec, ctx.g.cam.position);
        if (ctx.keyboard_state[SDL_SCANCODE_A]) glm_vec3_mulsub(ctx.g.cam.right, move_speed_vec, ctx.g.cam.position);
        if (ctx.keyboard_state[SDL_SCANCODE_D]) glm_vec3_muladd(ctx.g.cam.right, move_speed_vec, ctx.g.cam.position);

        view_mat_from_cam(&ctx.g.cam, view_mat);
        glm_perspective(glm_rad(70.0f), (float)ctx.display.width / (float)ctx.display.height, 0.01f, 4096.0f, proj_mat);
    }

    /* Finall pass */
    {
        glEnable(GL_DEPTH_TEST);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw mesh
        mat4 model_mat = GLM_MAT4_IDENTITY_INIT;

        glUseProgram(ctx.g.shader);

        glUniformMatrix4fv(glGetUniformLocation(ctx.g.shader, "u_model_mat"), 1, GL_FALSE, &model_mat[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(ctx.g.shader, "u_view_mat"), 1, GL_FALSE, &view_mat[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(ctx.g.shader, "u_proj_mat"), 1, GL_FALSE, &proj_mat[0][0]);

        // Material uniforms
        // glActiveTexture(GL_TEXTURE0);
        // glBindTexture(GL_TEXTURE_2D, material->diffuse_map);
        // glUniform1i(glGetUniformLocation(ctx.g.shader, "u_texture"), 0);

        // Draw segment
        glBindVertexArray(ctx.g.mesh.VAO);
        glDrawArrays(GL_TRIANGLES, 0, ctx.g.mesh.vertex_count);
    }

    // Flush
    SDL_GL_SwapWindow(ctx.display.window);
    SDL_Delay(ctx.display.target_frame_delay_ms);
    return SDL_APP_CONTINUE;
}


void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    quit_game();
    quit_engine();

    LOG_INFO("Exit sucess");
}


bool init_engine()
{
    LOG_DEBUG("Initializing engine");

    LOG_DEBUG("Initializing SDL");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_CRITICAL("Failed to initialize SDL! SDL error:\n%s", SDL_GetError());
        return false;
    }

    /* Window */
    {
        LOG_DEBUG("Creating window");

        // Determine display preferences
        const SDL_DisplayID main_display = SDL_GetPrimaryDisplay();
        if (main_display == 0) {
            LOG_CRITICAL("Failed to get primary display id! SDL error:\n%s", SDL_GetError());
            return false;
        }

        const SDL_DisplayMode* main_screen_mode = SDL_GetCurrentDisplayMode(main_display);
        if (main_screen_mode == NULL) {
            LOG_CRITICAL("Failed to get primary display mode! SDL error:\n%s", SDL_GetError());
            return false;
        }

        ctx.display.width = main_screen_mode->w;
        ctx.display.height = main_screen_mode->h;

        ctx.display.target_refresh_rate = main_screen_mode->refresh_rate;
        ctx.display.target_frame_delay_ms = (int)floorf(1000.0f / ctx.display.target_refresh_rate);

        // Create window
        ctx.display.window = SDL_CreateWindow(PROJECT_NAME, ctx.display.width, ctx.display.height, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
        if (ctx.display.window == NULL) {
            LOG_CRITICAL("Failed to create window! SDL error:\n%s", SDL_GetError());
            return false;
        }
    }

    /* OpenGL context */
    {
        LOG_DEBUG("Creating OpenGL 3.3 Core context");

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);

        ctx.display.gl = SDL_GL_CreateContext(ctx.display.window);
        if (ctx.display.gl == NULL) {
            LOG_CRITICAL("Failed to create main window OpenGL context! SDL error: \n%s", SDL_GetError());
            return false;
        }

        LOG_DEBUG("Loading OpenGL functions");

        if (!gladLoadGL(SDL_GL_GetProcAddress)) {
            LOG_CRITICAL("Failed to load OpenGL implementation!");
            return false;
        }
    }

    /* Asset io */
    {
        LOG_DEBUG("Opening assets io stream");

        ctx.assets_io = SDL_IOFromFile(ASSETS_FILE_PATH, "rb");
        if (!ctx.assets_io) {
            LOG_CRITICAL("Failed to open assets io steam! SDL error: \n%s", SDL_GetError());
            return false;
        }

        // Get file count
        SDL_ReadIO(ctx.assets_io, &ctx.assets_io_files_count, sizeof(int));
        LOG_DEBUG("Loading %d files", ctx.assets_io_files_count);

        // Load file index
        ctx.assets_io_files = SDL_realloc(ctx.assets_io_files, sizeof(FileEntry) * ctx.assets_io_files_count);

        for (int i = 0; i < ctx.assets_io_files_count; i++) {
            FileEntry* f = &ctx.assets_io_files[i];

            // Path
            int path_size = 0;
            SDL_ReadIO(ctx.assets_io, &path_size, sizeof(int));
            f->path = SDL_malloc(path_size);
            SDL_ReadIO(ctx.assets_io, f->path, path_size);

            // Offset + size
            SDL_ReadIO(ctx.assets_io, &f->offset, sizeof(size_t));
            SDL_ReadIO(ctx.assets_io, &f->size, sizeof(size_t));
        }
    }

    /* Misc */
    {
        SDL_SetWindowIcon(ctx.display.window, SDL_LoadBMP("icon.bmp"));

        SDL_SetWindowRelativeMouseMode(ctx.display.window, 1);

        ctx.keyboard_state = (bool*)SDL_GetKeyboardState(NULL);
    }

    LOG_DEBUG("Initialised engine successfully");
    return true;
}


bool init_game()
{
    LOG_DEBUG("Initializing game");

    glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, ctx.g.cam.position);
    glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, ctx.g.cam.up);

    ctx.g.shader = create_generic_shader(
        io_read_text_file("./assets/shaders/level.vs"),
        io_read_text_file("./assets/shaders/level.fs"));

    io_load_mesh_mdl("./assets/models/levels/tot.mdl", &ctx.g.mesh);

    LOG_INFO("Initialised game successfully");
    return true;
}


void quit_engine()
{
    LOG_DEBUG("Exiting engine");

    SDL_SetWindowRelativeMouseMode(ctx.display.window, 0);

    SDL_CloseIO(ctx.assets_io);
    SDL_DestroyWindow(ctx.display.window);
    SDL_GL_DestroyContext(ctx.display.gl);
    SDL_Quit();
}

void quit_game()
{
    LOG_DEBUG("Exiting game");
}


FileEntry* io_get_file_entry(const char* path)
{
    FileEntry* file_entry = NULL;
    for (int file_index = 0; file_index < ctx.assets_io_files_count; file_index++)
    {
        FileEntry* candidate_file_entry = &ctx.assets_io_files[file_index];

        if (SDL_strcmp((const char*)candidate_file_entry->path, path) == 0) {
            file_entry = candidate_file_entry;
            break;
        }
    }

    if (file_entry == NULL) {
        LOG_ERROR("Could not find asset file entry with path: %s!", path);
    }

    return file_entry;
}


char* io_read_text_file(const char* path)
{
    FileEntry* file_entry = io_get_file_entry(path);
    if (file_entry == NULL) {
        LOG_ERROR("Can not find text file!");
        return NULL;
    }

    char* text_buffer = (char*)SDL_calloc(1, file_entry->size + 1);
    if (!text_buffer) {
        LOG_ERROR("Failed to allocate text file buffer!");
        return NULL;
    }

    SDL_SeekIO(ctx.assets_io, file_entry->offset, SDL_IO_SEEK_SET);
    SDL_ReadIO(ctx.assets_io, text_buffer, file_entry->size);

    text_buffer[file_entry->size] = '\0'; // Text files come without a null-terminator

    return text_buffer;
}


GLuint io_load_texture(const char* path, GLint wrap_mode, GLint min_filter_mode, GLint mag_filter_mode, GLenum texture_format, bool flip_y, int* out_width, int* out_height)
{
    FileEntry* file_entry = io_get_file_entry(path);
    if (file_entry == NULL) {
        LOG_ERROR("Could not find texture file!");
        return 0;
    }

    // Load image file's binary
    unsigned char* file_buffer = (unsigned char*)SDL_malloc(file_entry->size);
    SDL_SeekIO(ctx.assets_io, file_entry->offset, SDL_IO_SEEK_SET);
    SDL_ReadIO(ctx.assets_io, file_buffer, file_entry->size);

    // Load with stb_image
    stbi_set_flip_vertically_on_load(flip_y);

    int width, height, color_channel_count;

    unsigned char* data = stbi_load_from_memory(file_buffer, (int)file_entry->size, &width, &height, &color_channel_count, 0);

    // Check if data loaded
    if (!data) {
        LOG_ERROR("Could not load texture from file buffer! stbi_load_from_memory failed.");

        stbi_image_free(data);
        SDL_free(file_buffer);

        return 0;
    }

    // Detect texture format from color_channel_count if none given.
    if (texture_format == 0) {
        switch (color_channel_count)
        {
        case 1: texture_format = GL_RED; break;
        case 2: texture_format = GL_RG; break;
        case 3: texture_format = GL_RGB; break;
        case 4: texture_format = GL_RGBA; break;
        default: {
            LOG_ERROR("Failed to detect texture format! Unusual channel count of: %d", color_channel_count);    

            stbi_image_free(data);
            SDL_free(file_buffer);
            
            return 0;
        } break;
        }
    }
    
    // Upload to GPU
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter_mode);

    glTexImage2D(GL_TEXTURE_2D, 0, texture_format, width, height, 0, texture_format, GL_UNSIGNED_BYTE, data);
    
    // Generate mipmaps if used
    if (min_filter_mode == GL_LINEAR_MIPMAP_LINEAR || min_filter_mode == GL_LINEAR_MIPMAP_NEAREST) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // Set output variables if requested
    if (out_width != NULL) *out_width = width;
    if (out_height != NULL) *out_height = height;

    return texture;    
}


void io_load_mesh_mdl(const char* path, Mesh* mesh)
{
    FileEntry* file_entry = io_get_file_entry(path);
    if (file_entry == NULL) {
        LOG_ERROR("Can not find mdl file!");
        return;
    }

    // Load buffer
    size_t bytes_read;

    SDL_SeekIO(ctx.assets_io, file_entry->offset, SDL_IO_SEEK_SET);

    int tri_count = 0;
    bytes_read = SDL_ReadIO(ctx.assets_io, &tri_count, sizeof(int));
    if (bytes_read < sizeof(int)) {
        LOG_ERROR("Failed to read mesh size! SDL error:\n%s", SDL_GetError());
    }

    int floats_per_vertex = (
        3 // Position attribute
    );
    size_t vertex_buffer_size = sizeof(GLfloat) * floats_per_vertex * tri_count * 3;

    GLfloat* vertex_buffer = SDL_malloc(vertex_buffer_size);
    if (!vertex_buffer) {
        LOG_ERROR("Failed to allocate memory for the vertex buffer! SDL error:\n%s", SDL_GetError());
        return;
    }

    bytes_read = SDL_ReadIO(ctx.assets_io, vertex_buffer, vertex_buffer_size);
    if (bytes_read < vertex_buffer_size) {
        LOG_ERROR("Unexpected EOF or read error while loading the vertex buffer! SDL error:\n%s", SDL_GetError());
        SDL_free(vertex_buffer);
        return;
    }

    LOG_DEBUG("Loaded %d polygons %s", tri_count, bytes_to_human_readable(vertex_buffer_size));

    // Create mesh
    mesh->vertex_count = tri_count * 3;

    glGenVertexArrays(1, &mesh->VAO);
    glGenBuffers(1, &mesh->VBO);
    
    glBindVertexArray(mesh->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
    glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size, vertex_buffer, GL_STATIC_DRAW);

    // Position attribute
    size_t stride = sizeof(GLfloat) * floats_per_vertex;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // Free buffer from ram
    SDL_free(vertex_buffer);
}


GLuint create_generic_shader(char* vertex_shader_source, char* fragment_shader_source)
{
    if (strlen(vertex_shader_source) == 0) LOG_WARNING("Vertex shader source is empty!");
    if (strlen(fragment_shader_source) == 0) LOG_WARNING("Fragment shader source is empty!");

    int success;

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, (const GLchar* const*)&vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    // Check for errors
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int log_length;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_length);

        char* error_log = (char*)alloca(log_length * sizeof(char)); // `alloca()` does not require an explicit SDL_free()
        glGetShaderInfoLog(vertex_shader, log_length, NULL, error_log);

        LOG_ERROR("Failed to compile the vertex shader!\n%s", error_log);
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, (const GLchar* const*)&fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    // Check for errors
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int log_length;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_length);

        char* error_log = (char*)alloca(log_length * sizeof(char));
        glGetShaderInfoLog(fragment_shader, log_length, NULL, error_log);

        LOG_ERROR("Failed to compile the fragment shader!\n%s", error_log);
    }

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);
    // Check for errors
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        int log_length;
        glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);

        char* error_log = (char*)alloca(log_length * sizeof(char));
        glGetProgramInfoLog(shader_program, log_length, NULL, error_log);

        LOG_ERROR("Failed to link the shader program!\n%s", error_log);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    SDL_free(vertex_shader_source);
    SDL_free(fragment_shader_source);

    return shader_program;
}


void view_mat_from_cam(Camera* cam, mat4 dest)
{
    SDL_assert(cam->up[0] + cam->up[1] + cam->up[2] != 0.0);

    // Front direction from pitch (x) and yaw (y)
    cam->front[0] = cosf(glm_rad(cam->rotation[1])) * cosf(glm_rad(cam->rotation[0]));
    cam->front[1] = sinf(glm_rad(cam->rotation[0]));
    cam->front[2] = sinf(glm_rad(cam->rotation[1])) * cosf(glm_rad(cam->rotation[0]));
    glm_normalize(cam->front);

    // Right vector
    glm_cross(cam->front, cam->up, cam->right);
    glm_normalize(cam->right);

    // Target
    glm_vec3_add(cam->position, cam->front, cam->target);

    // Build view matrix
    glm_lookat(cam->position, cam->target, cam->up, dest);
}


char* bytes_to_human_readable(size_t bytes)
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    
    const int units_count = sizeof(units) / sizeof(char*);
    double count = (double)bytes;
    int i = 0;

    while (count >= 1024.0 && i < units_count) {
        count /= 1024.0;
        i++;
    }

    return str_new_formatted("%.2f %s", count, units[i]);
}


char* str_new_formatted(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int str_len = SDL_vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);

    char* str = (char*)SDL_calloc(1, str_len);
    if (!str) {
        LOG_ERROR("Failed to allocate enough memory for the new string.");
        return NULL;
    }

    va_start(args, fmt);
    SDL_vsnprintf(str, str_len, fmt, args);
    va_end(args);

    return str;
}
