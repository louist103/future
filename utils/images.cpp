#if defined (_WIN32)
// TODO
#elif defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <GL/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <unordered_map>
#include <string>

static std::unordered_map<std::string, void*> sImageCache;


static inline GLuint LoadTextureGL(void* data, int width, int height) {
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    return image_texture;
}

void* LoadTextureByName(const char* path, int* width, int* height) {
    if (sImageCache.contains(path)) {
        return sImageCache.at(path);
    }
    void* imageData;
    off_t fileSize;
    #if defined(__WIN32)
    #elif defined(__linux__)
    int fd;
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return nullptr;
    }
    struct stat s;
    fstat(fd, &s);
    fileSize = s.st_size;
    imageData = mmap(nullptr, fileSize, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    stbi_uc* image = stbi_load_from_memory((stbi_uc*)imageData, (int)fileSize, width, height, nullptr, 4);
    void* texId = (void*)(uintptr_t)LoadTextureGL(image, *width, *height);
    munmap(imageData, fileSize);
    stbi_image_free(image);
    #endif
    sImageCache[path] = texId;
    
    return texId;

}