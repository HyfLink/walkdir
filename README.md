# walkdir

递归遍历文件夹的工具。。。

### Usage

- `walkdir` 等价于 `walkdir .`。

- `walkdir <rootpath>` 遍历`<rootpat>`。

### Output

输出在`output.dat`，***无表头***，每行保存一个目录项，格式为：

```{hash}\t{size}\t{depth}\t{width}\t{length}\t{mode}\t{ctime}\t{mtime}\t{atime}\t{path}\n```

- `hash` path的哈希值
  - 16位十六进制数，补充前导0
  - 使用默认`std::hash`(`g++ (GCC) 13.1.1 20230714`, `--std=C++20`)计算。

- `size` 
  - 十进制无符号整数
  - 对于非目录项，文件字节数
  - 对于目录项，所有一级目录项`size`之和，（等价于所有子目录非文件夹项`size`之和）。

- `depth`
  - 十进制无符号整数
    - 从根目录路径到当地路径的距离
      - 根目录为0

- `width` 文件夹下一级目录项的个数
  - 十进制无符号整数
  - 仅对文件夹有效
    - 0 对于空目录
  - 0 对于非文件夹项

- `length` 当前路径到最深子目录的距离
  - 十进制无符号整数
  - 仅对文件夹有效
    - 0 对于空目录
    - 1 对于不包含子目录的非空目录
  - 0 对于非文件夹项

- `mode` 文件类型
  - 十进制无符号整数
  - 参考 [`std::filesystem::file_type`](https://en.cppreference.com/w/cpp/filesystem/file_type)

- `ctime` 创建时间(created)
  - 格式`YYYY-MM-DDThh:mm:ssZ`，如`2023-07-28T20:39:06Z`

- `mtime` 上次修改时间(modified)
  - 格式`YYYY-MM-DDThh:mm:ssZ`，如`2023-07-28T20:39:06Z`

- `atime` 上次获取时间(accessed)
  - 格式`YYYY-MM-DDThh:mm:ssZ`，如`2023-07-28T20:39:06Z`

- `path` 目录项路径
