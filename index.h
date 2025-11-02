#ifdef _WIN32
    #include <direct.h>
    #define make_dir(name) _mkdir(name)
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define make_dir(name) mkdir(name, 0777)
#endif
