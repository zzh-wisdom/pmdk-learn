# API 文档

## config

### 创建与销毁

```cpp
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_new(struct pmem2_config **cfg);
int pmem2_config_delete(struct pmem2_config **cfg);
```

pmem2_config_new() 函数实例化了一个新的（不透明的）配置结构体 pmem2_config，用于定义 pmem2_map_new() 函数的映射参数，并通过 *cfg 中的指针返回。

新配置始终**使用大多数参数的默认值进行初始化**，这些参数与相应的 setter 函数一起指定。**应用程序必须显式设置映射的粒度值**。

pmem2_config_delete() 函数释放 pmem2_config_new() 返回的 *cfg 并将 *cfg 设置为 NULL。**如果 *cfg 为 NULL，则不执行任何操作**。

**返回值：**

- pmem2_config_new() 函数成功时返回 0，失败时返回负错误代码。pmem2_config_new() 确保在失败时将 *cfg 设置为 NULL。
- pmem2_config_new()返回的错误代码有：-ENOMEM - out of memory内存不足
- pmem2_config_delete() 函数总是返回 0。

### Config中的参数设置

#### length 

```cpp
int pmem2_config_set_length(struct pmem2_config *config, size_t length);
```

pmem2_config_set_length() 函数设置将用于映射的长度。 此时，*config 需要已被初始化了。length 必须是数据源所需对齐方式的倍数，该数据源将用于与配置一起映射。要检索 *pmem2_source** 的特定实例所需的对齐方式，请使用 pmem2_source_alignment(3)。**默认情况下，长度等于被映射文件的大小**。

返回值总是0。

测试

```cpp
// 超过文件的大小（10+10）, 发生error，提示length is not a multiple of 4096
    // pmem2_config_set_length(cfg, file_size+10);

    // 超过文件的大小，且是4kb的倍数, 发生error，提示mapping larger than file size
    // pmem2_config_set_length(cfg, (file_size+4096)/4096*4096);

    // 等于文件的大小10，发生error，提示length is not a multiple of 4096
    // pmem2_config_set_length(cfg, file_size);

    // 等于文件大小10，设置映射粒度为PMEM2_GRANULARITY_BYTE，发生error，提示length is not a multiple of 4096
    // pmem2_config_set_length(cfg, file_size);

    // 小于文件大小，error，length is not a multiple of 4096
    // pmem2_config_set_length(cfg, 8);

    // 将文件大小扩大为5120
    for(int i=0; i<1024; i++) {
        write(fd, "00000", 5);
    }
    // 超过文件的大小, mapping larger than file size
    // pmem2_config_set_length(cfg, 8192);
    // 小于文件大小，是4kb的倍数，运行通过
    pmem2_config_set_length(cfg, 4096);
```

结论：

1. 映射长度不能超过文件大小
2. length与映射粒度无关，最小的映射长度要为4096的倍数，也就是完整的页，这是由操作系统的内存管理方式决定的
3. 设置映射长度后，只能使用文件开头的length个字节，超出范围则发生段错误

#### 池
