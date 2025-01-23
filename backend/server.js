const express = require('express');
const { exec } = require('child_process');
const path = require('path');
const minimist = require('minimist');
const os = require('os');

// 生成唯一的ID
let logId = 0;

// 自定义日志函数
function logWithIdAndTimestamp(message) {
    const now = new Date();
    const timestamp = `${now.getFullYear()}.${now.getMonth() + 1}.${now.getDate()}-${now.getHours()}:${now.getMinutes()}:${now.getSeconds()}`;
    console.log(`[${logId++}] [${timestamp}] ${message}`);
}

// 根据操作系统获取打开浏览器的命令
function getOpenBrowserCommand(url) {
    switch (os.platform()) {
        case 'win32': // Windows
            return `start ${url}`;
        case 'darwin': // macOS
            return `open ${url}`;
        case 'linux': // Linux
            return `xdg-open ${url}`;
        default:
            throw new Error('不支持的操作系统: ' + os.platform());
    }
}

// 自动打开浏览器
function openBrowser(url) {
    const command = getOpenBrowserCommand(url);
    exec(command, (error) => {
        if (error) {
            logWithIdAndTimestamp(`打开浏览器失败: ${error.message}`);
        } else {
            logWithIdAndTimestamp(`已在默认浏览器中打开: ${url}`);
        }
    });
}

// 检查IP是否为本地网络IP
function isLocalNetworkIP(ip) {
    const parts = ip.split('.').map(Number);
    if (parts.length !== 4) return false;

    // 10.0.0.0 - 10.255.255.255
    if (parts[0] === 10) return true;

    // 172.16.0.0 - 172.31.255.255
    if (parts[0] === 172 && parts[1] >= 16 && parts[1] <= 31) return true;

    // 192.168.0.0 - 192.168.255.255
    if (parts[0] === 192 && parts[1] === 168) return true;

    return false;
}

// 获取本地网络IPv4地址
function getLocalNetworkIP() {
    const interfaces = os.networkInterfaces();
    for (const interfaceName in interfaces) {
        for (const iface of interfaces[interfaceName]) {
            if (iface.family === 'IPv4' && !iface.internal && isLocalNetworkIP(iface.address)) {
                return iface.address;
            }
        }
    }
    return '127.0.0.1'; // 如果没有找到本地网络IP，返回127.0.0.1
}

// 获取所有非内部IPv4地址
function getAllIPv4Addresses() {
    const interfaces = os.networkInterfaces();
    const addresses = [];
    for (const interfaceName in interfaces) {
        for (const iface of interfaces[interfaceName]) {
            if (iface.family === 'IPv4' && !iface.internal) {
                addresses.push(iface.address);
            }
        }
    }
    return addresses;
}

// 解析命令行参数
const rawArgs = minimist(process.argv.slice(2), {
    string: ['ip', 'port'], // 将ip和port解析为字符串
    boolean: ['dedicated', 'force_lan', 'force_localhost'], // 将dedicated、force_lan和force_localhost解析为布尔值
    default: {
        ip: '127.0.0.1', // 默认IP
        port: '3000', // 默认端口
        dedicated: false, // 默认dedicated
        force_lan: false, // 默认force_lan
        force_localhost: false, // 默认force_localhost
    },
});

// 将参数键名统一转换为小写，确保大小写不敏感
const args = {};
for (const key in rawArgs) {
    args[key.toLowerCase()] = rawArgs[key];
}

// 处理布尔类型的参数
const booleanParams = ['dedicated', 'force_lan', 'force_localhost'];
for (const param of booleanParams) {
    if (typeof args[param] === 'string') {
        args[param] = args[param].toLowerCase() === 'true';
    }
}

// 如果force_localhost为true，强制绑定到127.0.0.1
if (args.force_localhost) {
    args.ip = '127.0.0.1';
    logWithIdAndTimestamp('强制绑定到127.0.0.1');
}
// 如果force_lan为true，检查IP是否为本地网络IP
else if (args.force_lan) {
    if (!isLocalNetworkIP(args.ip)) {
        const localIP = getLocalNetworkIP();
        logWithIdAndTimestamp(`IP ${args.ip} 不是本地网络IP。强制绑定到本地网络IP: ${localIP}`);
        args.ip = localIP;
    }
}

// 打印解析后的参数
logWithIdAndTimestamp('解析后的参数: ' + JSON.stringify(args));

// 验证端口是否为有效数字
const port = parseInt(args.port);
if (isNaN(port) || port < 1 || port > 65535) {
    logWithIdAndTimestamp('无效的端口号。使用默认端口3000');
    args.port = 3000;
}

// 打印最终参数
logWithIdAndTimestamp('最终参数: ' + JSON.stringify({
    ip: args.ip,
    port: args.port,
    force_lan: args.force_lan,
    force_localhost: args.force_localhost,
    dedicated: args.dedicated,
}));

const app = express();

// 提供静态文件（前端）
app.use(express.static(path.join(__dirname, '../frontend')));

// 解析JSON请求体
app.use(express.json());

// 查询服务器信息的API
app.post('/query', (req, res) => {
    const { ip, port } = req.body;
    logWithIdAndTimestamp(`收到查询请求: IP=${ip}, Port=${port}`);

    // 调用Python脚本
    exec(`python server.py ${ip} ${port}`, { encoding: 'utf8' }, (error, stdout, stderr) => {
        if (error) {
            logWithIdAndTimestamp(`执行错误: ${error}`);
            return res.status(500).send(stderr);
        }

        // 解析Python脚本输出
        try {
            const serverInfo = JSON.parse(stdout);

            if (serverInfo.error) {
                // 如果返回了错误信息
                logWithIdAndTimestamp(`查询失败: ${serverInfo.error}`);
                return res.status(500).send(serverInfo.error);
            }

            // 清理版本字段中的控制字符
            if (serverInfo.version) {
                serverInfo.version = serverInfo.version.replace(/[^\x20-\x7E]/g, '').trim();
            }

            // 将server_type映射为人类可读的字符串
            let serverType;
            switch (serverInfo.server_type) {
                case 'd':
                    serverType = '专用服务器';
                    break;
                case 'l':
                    serverType = '监听服务器';
                    break;
                case 'p':
                    serverType = 'SourceTV';
                    break;
                case 'h':
                    serverType = '混合服务器';
                    break;
                case 'm':
                    serverType = '多人游戏服务器';
                    break;
                default:
                    serverType = '未知';
                    break;
            }

            // 格式化服务器信息以供显示（中文）
            const formattedOutput = `
=== 服务器信息 ===
IP: ${serverInfo.ip}
端口: ${serverInfo.port}
名称: ${serverInfo.name}
地图: ${serverInfo.map || '未知'}
游戏目录: ${serverInfo.game_directory}
游戏描述: ${serverInfo.game_description}
当前玩家: ${serverInfo.current_players}
最大玩家: ${serverInfo.max_players}
机器人数量: ${serverInfo.bots}
服务器类型: ${serverType}
操作系统: ${serverInfo.os}
VAC 保护: ${serverInfo.vac}
版本: ${serverInfo.version}
            `;

            // 记录用户输入
            logWithIdAndTimestamp(`用户输入: IP=${ip}, Port=${port}`);

            // 记录前端输出
            logWithIdAndTimestamp(`前端输出: ${formattedOutput}`);

            // 记录JSON格式的服务器信息
            logWithIdAndTimestamp('JSON格式的服务器信息: ' + JSON.stringify(serverInfo, null, 2));

            // 将格式化后的输出返回给前端（中文）
            res.send(formattedOutput);
        } catch (parseError) {
            logWithIdAndTimestamp('解析Python脚本输出失败: ' + parseError);
            res.status(500).send('解析服务器信息失败');
        }
    });
});

// 启动服务器
const server = app.listen(args.port, args.ip, () => {
    const address = server.address();
    let finalIP = address.address;

    // 如果绑定的IP是0.0.0.0，获取所有非内部IPv4地址
    if (finalIP === '0.0.0.0') {
        const ipAddresses = getAllIPv4Addresses();
        if (ipAddresses.length > 0) {
            finalIP = ipAddresses[0]; // 使用第一个非内部IPv4地址
            logWithIdAndTimestamp(`检测到实际绑定的IP: ${finalIP}`);
        } else {
            finalIP = '127.0.0.1'; // 如果没有找到其他IP，使用127.0.0.1
            logWithIdAndTimestamp('未检测到其他IP，使用127.0.0.1');
        }
    }

    const serverUrl = `http://${finalIP}:${address.port}`;
    logWithIdAndTimestamp(`服务器运行在: ${serverUrl}`);

    // 如果dedicated为false，自动打开浏览器
    if (!args.dedicated) {
        try {
            openBrowser(serverUrl);
        } catch (err) {
            logWithIdAndTimestamp(`打开浏览器失败: ${err.message}`);
        }
    } else {
        logWithIdAndTimestamp('dedicated模式已启用，跳过自动打开浏览器');
    }
}).on('error', (err) => {
    logWithIdAndTimestamp('服务器启动失败: ' + err);
});