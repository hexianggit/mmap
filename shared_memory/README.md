# 进程间通信实例

## 使用实例
```bash
cd shared_memory

# 编译写入进程
g++ writer.cpp -o writer -lrt

# 编译读取进程
g++ reader.cpp -o reader -lrt

# 终端1：运行读取进程
./reader

# 终端2：运行写入进程
./writer
```