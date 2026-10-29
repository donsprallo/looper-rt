/*
 * Loading a compiled state model: one library, one exported function.
 *
 * A path arrives as UTF-8, which is what the control protocol carries.
 * Windows wants UTF-16 for a path that is more than ASCII, so it is
 * converted there; everywhere else the bytes are the path.
 */
#include "looper_model_library.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(_WIN32)

static void *load(const char *path)
{
    wchar_t wide[LOOPER_FILE_PATH_CAPACITY];

    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, (int)LOOPER_FILE_PATH_CAPACITY) == 0) {
        return NULL;
    }
    return (void *)LoadLibraryW(wide);
}

static looper_model_describe_t find(void *handle, const char *symbol)
{
    FARPROC found = GetProcAddress((HMODULE)handle, symbol);
    return (looper_model_describe_t)(void (*)(void))found;
}

static void unload(void *handle)
{
    FreeLibrary((HMODULE)handle);
}

#else

static void *load(const char *path)
{
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static looper_model_describe_t find(void *handle, const char *symbol)
{
    looper_model_describe_t describe = NULL;
    void *found = dlsym(handle, symbol);

    /* The POSIX way from a data pointer to a function pointer. */
    memcpy(&describe, &found, sizeof(describe));
    return describe;
}

static void unload(void *handle)
{
    dlclose(handle);
}

#endif

/* Whether the library holds the model the request describes. */
static const char *description_fault(const looper_model_abi_t *description, const looper_state_model_t *model)
{
    if (description == NULL || description->abi != LOOPER_MODEL_ABI || model->abi != LOOPER_MODEL_ABI) {
        return "a model library built for another version of the model interface";
    }
    if (description->states != model->states || description->inputs != model->inputs
        || description->outputs != model->outputs || description->constants != model->constant_count) {
        return "a model library whose model is not the size the request says";
    }
    if (description->advance == NULL) {
        return "a model library that cannot step its model";
    }
    return NULL;
}

const char *looper_model_library_open(const looper_state_model_t *model, looper_model_library_t *library)
{
    memset(library, 0, sizeof(*library));
    void *handle = load(model->library);
    if (handle == NULL) {
        return "a model library that cannot be loaded";
    }
    looper_model_describe_t describe = find(handle, model->symbol);
    const looper_model_abi_t *description = describe != NULL ? describe() : NULL;
    const char *fault = description_fault(description, model);
    if (fault != NULL) {
        unload(handle);
        return fault;
    }
    library->handle = handle;
    library->description = description;
    return NULL;
}

void looper_model_library_close(looper_model_library_t *library)
{
    if (library->handle != NULL) {
        unload(library->handle);
    }
    memset(library, 0, sizeof(*library));
}
