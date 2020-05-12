# Simple FTP Client

## 简介

这是一个很简单的 FTP 客户端，使用流式套接字(SOCKET_STREAM)实现。

## 编译

```console
g++ client.cpp -o client -std=c++11
```

## 运行

```console
./console <server address> <port>
```

## 实现命令

```text
FTP Client commands:

 put  <filename>  --- Upload a file from local to server
 get  <filename>  --- Download a file from server to local
  ls              --- List all files under the present directory of the server
 !ls              --- List all files under the present directory of the client
 pwd              --- Display the present working directory of the server
!pwd              --- Display the present working directory of the client
  cd  <directory> --- Change the present working directory of the server
 !cd  <directory> --- Change the present working directory of the client
quit              --- Quit
```
