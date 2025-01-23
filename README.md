### 使用方法

1. 前往 [releases](https://github.com/zetsr/SteamServerQuery/releases) 下载项目文件

2. 运行 **start.bat**

### 启动命令

```node backend/server.js ```

### 启动项

#### ip

设置web端使用的ip，默认 **localhost** 

```--ip=0.0.0.0```

#### port

设置web端使用的端口，默认 **3000**

```--port=1337```

#### dedicated

如果启用，后端将不会自动打开本机浏览器，默认 **false**

```--dedicated=false```

#### force_lan

如果启用，web端将强制绑定局域网，默认 **false**

```--force_lan=false```

#### force_localhost

如果启用，web端将强制绑定localhost，默认 **false**

```--force_localhost=false```
