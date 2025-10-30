#include <assert.h>

#include <SDL3/SDL.h>

/*
** Macros
*/

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
    char path[MAX_PATH_LENGTH];
    size_t size;
    size_t offset;
} FileEntry;

/*
** Declarations
*/

size_t get_file_size(char* path);

void scan_dir(char* path);

SDL_EnumerationResult list_dir(void *userdata, const char *dirname, const char *fname);

bool str_starts_with(char* str, char* prefix);
char* str_override(char* dest, char* str);

/*
** Implementation
*/

char** in_files = NULL;
int in_files_count = 0;

int main(int args_count, char* args[])
{
    // Parse args
    char* in_path = SDL_strdup("assets");
    char* out_path = SDL_strdup("assets.bin");

    for (int arg_index = 1; arg_index < args_count; arg_index++) {
        char* arg = args[arg_index];

        if (str_starts_with(arg, "-i:")) {
            in_path = str_override(in_path, arg + strlen("-i:"));
        }

        if (str_starts_with(arg, "-o:")) {
            out_path = str_override(out_path, arg + strlen("-o:"));
        }
    }

    LOG_INFO("Input dir: %s", in_path);
    LOG_INFO("Ouput file: %s", out_path);

    // Scan
    LOG_INFO("Scanning input directory");
    scan_dir(in_path);

    LOG_INFO("Creating output file");
    SDL_IOStream* out_file = SDL_IOFromFile(out_path, "w");
    if (!out_file) {
        LOG_ERROR("Failed to open output file! SDL error:\n%s", SDL_GetError());
        return 1;
    }

    LOG_DEBUG("Writing header");
    SDL_WriteIO(out_file, &in_files_count, sizeof(int));

    LOG_DEBUG("Creating file index");
    FileEntry* file_entires = NULL;
    int file_entires_count = 0;

    // We will first write the header & the file entires index then pack
    // then raw data of each file, one after another.
    // We use increment this variable evry time a file's data is written to track when the next one begins.
    size_t header_size = SDL_TellIO(out_file);
    size_t file_entires_size_max = sizeof(file_entires) * in_files_count;

    size_t file_entry_begin_offset = header_size + file_entires_size_max;

    for (int i = 0; i < in_files_count; i++) {
        char* file_path = in_files[i];
        LOG_DEBUG("Creating entry #%d: %s", i, file_path);

        // Realloc
        size_t new_size = sizeof(FileEntry) * (file_entires_count + 1);
        file_entires = SDL_realloc(file_entires, new_size);
        FileEntry* f = &file_entires[file_entires_count++];

        // Write file entry
        {
            // Path
            SDL_memset(f->path, 0, MAX_PATH_LENGTH);
            SDL_memcpy(f->path, file_path, strlen(file_path) + 1);
    
            // File size
            size_t file_size = get_file_size(f->path);
            if (file_size == 0) {
                LOG_ERROR("Failed to measure file size: %s, skipping!", f->path);
                continue;
            }

            f->size = file_size;

            // Offset
            f->offset = file_entry_begin_offset;

            file_entry_begin_offset += file_size;
        }
    }

    // Write file index
    LOG_DEBUG("Writing file index");
    SDL_WriteIO(out_file, file_entires, file_entires_size_max);

    // Write file contents
    LOG_DEBUG("Writing file contents");
    for (int i = 0; i < file_entires_count; i++) {
        FileEntry* file_entry = &file_entires[i];
        
        LOG_INFO("Dumping: %s", file_entry->path);

        SDL_IOStream* file_io = SDL_IOFromFile(file_entry->path, "r");
        if (!file_io) {
            LOG_ERROR("Failed to open file: %s, skipping! SDL error:\n%s", file_entry->path, SDL_GetError());
            continue;
        }

        void* file_buffer = SDL_malloc(file_entry->size);
        if (!file_buffer) {
            LOG_ERROR("Failed to allocate file buffer: %s, skipping! SDL error:\n%s", file_entry->path, SDL_GetError());
            SDL_CloseIO(file_io);
            continue;
        }

        size_t written_bytes = SDL_WriteIO(out_file, file_buffer, file_entry->size);
        if (written_bytes < file_entry->size) {
            LOG_ERROR("Failed to fully write the input file into the output file! Written: %zu/%zu bytes", written_bytes, file_entry->size);
            SDL_CloseIO(file_io);
            continue;
        }

        SDL_free(file_buffer);

        SDL_CloseIO(file_io);
    }

    SDL_CloseIO(out_file);

    LOG_INFO("%s created successfully!", out_path);

    return 0;
}


size_t get_file_size(char* path)
{
    SDL_IOStream* f = SDL_IOFromFile(path, "r");
    if (!f) {
        LOG_ERROR("Failed to open file: %s! SDL error:\n%s", path, SDL_GetError());
        return 0;
    }

    SDL_SeekIO(f, 0, SDL_IO_SEEK_END);

    size_t file_size = SDL_TellIO(f);

    SDL_CloseIO(f);

    return file_size;
}


void scan_dir(char* path)
{
    if (!SDL_EnumerateDirectory(path, list_dir, NULL)) {
        LOG_ERROR("Failed to list directory: %s! SDL error:\n%s", path, SDL_GetError());
    }
}


SDL_EnumerationResult list_dir(void *userdata, const char *dirname, const char *fname) {
    char full_path[MAX_PATH_LENGTH];
    SDL_snprintf(full_path, MAX_PATH_LENGTH, "%s%s", dirname, fname);

    SDL_PathInfo info;
    if (SDL_GetPathInfo(full_path, &info)) {
        if (info.type == SDL_PATHTYPE_DIRECTORY) {
            scan_dir(full_path);
        }
        else if (info.type == SDL_PATHTYPE_FILE) {
            char file_path[MAX_PATH_LENGTH];
            SDL_snprintf(file_path, MAX_PATH_LENGTH, "%s/%s", dirname, fname);

            size_t new_size = sizeof(char*) * (in_files_count + 1);
            in_files = SDL_realloc(in_files, new_size);
            
            in_files[in_files_count] = SDL_strdup(full_path);

            in_files_count++;
        }
        
    } else {
        LOG_ERROR("Failed to get path info for: %s! SDL error:\n%s", full_path, SDL_GetError());
    }

    return SDL_ENUM_CONTINUE;
}


bool str_starts_with(char* str, char* prefix) {
    size_t len_prefix = strlen(prefix);

    assert(strlen(str) > len_prefix);

    return strncmp(str, prefix, len_prefix) == 0;
}


char* str_override(char* dest, char* str)
{
    SDL_free(dest);
    char* new_dest = SDL_strdup(str);
    return new_dest;
}
