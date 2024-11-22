#if defined (_WIN32)
#include <D3d11.h>
#include <GL/gl.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <GL/gl.h>
#elif defined(__APPLE__)
#include <GL/glew.h>
#endif
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <unordered_map>
#include <string>

static std::unordered_map<std::string, void*> sImageCache;

#if defined(_WIN32)
static inline ID3D11ShaderResourceView* LoadTextureDX11(void* data, int width, int height) {
    // Create texture
    extern ID3D11Device* g_pd3dDevice;
    D3D11_TEXTURE2D_DESC desc;
    ID3D11ShaderResourceView* view;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D* pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &view);
    pTexture->Release();
    return view;
}
#elif defined(__linux__) || defined(__APPLE__)
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
#endif

void* LoadTextureByName(const char* path, int* width, int* height) {
    if (sImageCache.contains(path)) {
        return sImageCache.at(path);
    }
    void* imageData;
    off_t fileSize;
    #if defined(_WIN32)
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == nullptr) {
        return nullptr;
    }
    LARGE_INTEGER sizeW;
    GetFileSizeEx(hFile, &sizeW);
    fileSize = sizeW.QuadPart;
    HANDLE mappingObj = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mappingObj == nullptr) {
        CloseHandle(hFile);
        return nullptr;
    }
    imageData = MapViewOfFile(mappingObj, FILE_MAP_READ, 0, 0, 0);
    
    stbi_uc* image = stbi_load_from_memory((stbi_uc*)imageData, (int)fileSize, width, height, nullptr, 4);
    void* texId = (void*)(uintptr_t)LoadTextureDX11(image, *width, *height);

    UnmapViewOfFile(imageData);
    CloseHandle(mappingObj);
    CloseHandle(hFile);

    #elif defined(__linux__) || defined(__APPLE__)
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
    #endif
    stbi_image_free(image);
    sImageCache[path] = texId;
    
    return texId;

}