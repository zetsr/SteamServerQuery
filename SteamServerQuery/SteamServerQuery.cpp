#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

struct ServerInfo {
    std::string ip;
    int port;
    std::string name;
    std::string map;
    std::string game_directory;
    std::string game_description;
    int app_id;
    int current_players;
    int max_players;
    int bots;
    char server_type;
    std::string os;
    std::string vac;
    std::string version;
};

bool is_valid_ip(const std::string& ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
}

std::string resolve_domain(const std::string& domain) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return "";
    }

    struct addrinfo hints, * res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(domain.c_str(), nullptr, &hints, &res) != 0) {
        std::cerr << "域名解析失败: " << domain << std::endl;
        WSACleanup();
        return "";
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr, ip, INET_ADDRSTRLEN);
    freeaddrinfo(res);
    WSACleanup();
    return ip;
}

ServerInfo query_server_info(const std::string& ip, int port) {
    ServerInfo info;
    info.ip = ip;
    info.port = port;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return info;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "无法创建套接字" << std::endl;
        WSACleanup();
        return info;
    }

    // 设置超时
    DWORD timeout = 5000; // 5秒
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    // A2S_INFO challenge request
    const char challenge_request[] = "\xFF\xFF\xFF\xFF\x54Source Engine Query\x00";
    if (sendto(sockfd, challenge_request, sizeof(challenge_request) - 1, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "发送请求失败: " << ip << ":" << port << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return info;
    }

    char recv_buf[4096];
    int server_len = sizeof(server_addr);
    int len = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&server_addr, &server_len);
    if (len == SOCKET_ERROR) {
        std::cerr << "接收响应失败: " << ip << ":" << port << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return info;
    }

    if (recv_buf[4] == 0x41) { // Challenge response
        char challenge_token[4];
        std::memcpy(challenge_token, recv_buf + 5, 4);
        char challenge_request_with_token[29];
        std::memcpy(challenge_request_with_token, challenge_request, 25);
        std::memcpy(challenge_request_with_token + 25, challenge_token, 4);

        if (sendto(sockfd, challenge_request_with_token, sizeof(challenge_request_with_token), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "发送挑战请求失败: " << ip << ":" << port << std::endl;
            closesocket(sockfd);
            WSACleanup();
            return info;
        }

        len = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&server_addr, &server_len);
        if (len == SOCKET_ERROR) {
            std::cerr << "接收挑战响应失败: " << ip << ":" << port << std::endl;
            closesocket(sockfd);
            WSACleanup();
            return info;
        }
    }

    if (recv_buf[4] == 0x49) { // Detailed info response
        size_t offset = 6;
        size_t end = std::find(recv_buf + offset, recv_buf + len, '\0') - recv_buf;
        info.name = std::string(recv_buf + offset, recv_buf + end);
        offset = end + 1;

        end = std::find(recv_buf + offset, recv_buf + len, '\0') - recv_buf;
        info.map = std::string(recv_buf + offset, recv_buf + end);
        offset = end + 1;

        end = std::find(recv_buf + offset, recv_buf + len, '\0') - recv_buf;
        info.game_directory = std::string(recv_buf + offset, recv_buf + end);
        offset = end + 1;

        end = std::find(recv_buf + offset, recv_buf + len, '\0') - recv_buf;
        info.game_description = std::string(recv_buf + offset, recv_buf + end);
        offset = end + 1;

        info.app_id = *reinterpret_cast<unsigned short*>(recv_buf + offset);
        offset += 2;

        info.current_players = static_cast<unsigned char>(recv_buf[offset++]); // 修复：使用unsigned char
        info.max_players = static_cast<unsigned char>(recv_buf[offset++]);     // 修复：使用unsigned char
        info.bots = static_cast<unsigned char>(recv_buf[offset++]);            // 修复：使用unsigned char
        info.server_type = recv_buf[offset++];
        info.os = (recv_buf[offset] == 0x6C) ? "Linux" : (recv_buf[offset] == 0x77) ? "Windows" : (recv_buf[offset] == 0x6D) ? "macOS" : "Unknown";
        offset++;
        info.vac = (recv_buf[offset] == 0x01) ? "Enabled" : "Disabled";
        offset++;

        end = std::find(recv_buf + offset, recv_buf + len, '\0') - recv_buf;
        info.version = std::string(recv_buf + offset, recv_buf + end);
    }
    else {
        std::cerr << "服务器 " << ip << ":" << port << " 返回了未知的响应类型：" << static_cast<int>(recv_buf[4]) << std::endl;
    }

    closesocket(sockfd);
    WSACleanup();
    return info;
}

void print_server_info(const std::vector<ServerInfo>& servers) {
    if (servers.empty()) {
        std::cout << "没有找到在线的服务器。" << std::endl;
        return;
    }

    // 定义列宽
    const int ip_width = 22;
    const int port_width = 8;
    const int name_width = 30;
    const int map_width = 20;
    const int game_dir_width = 20;
    const int players_width = 12;
    const int type_width = 10;
    const int os_width = 10;
    const int vac_width = 10;

    // 打印表头
    std::cout << "\n=== 服务器详细信息 ===" << std::endl;
    std::cout << std::left
        << std::setw(ip_width) << "IP"
        << std::setw(port_width) << "端口"
        << std::setw(name_width) << "名称"
        << std::setw(map_width) << "地图"
        << std::setw(game_dir_width) << "游戏目录"
        << std::setw(players_width) << "玩家"
        << std::setw(type_width) << "类型"
        << std::setw(os_width) << "操作系统"
        << std::setw(vac_width) << "VAC"
        << std::endl;

    // 打印服务器信息
    for (const auto& info : servers) {
        std::cout << std::left
            << std::setw(ip_width) << info.ip
            << std::setw(port_width) << info.port
            << std::setw(name_width) << info.name
            << std::setw(map_width) << info.map
            << std::setw(game_dir_width) << info.game_directory
            << std::setw(players_width) << (std::to_string(info.current_players) + "/" + std::to_string(info.max_players))
            << std::setw(type_width) << info.server_type
            << std::setw(os_width) << info.os
            << std::setw(vac_width) << info.vac
            << std::endl;
    }

    std::cout << "总计在线服务器数量: " << servers.size() << std::endl;
}

void query_all_servers(const std::vector<std::string>& ips, const std::vector<int>& ports) {
    std::vector<ServerInfo> servers;
    std::vector<std::thread> threads;
    std::mutex mutex;

    for (const auto& ip : ips) {
        for (const auto& port : ports) {
            threads.emplace_back([&servers, &mutex, ip, port]() {
                ServerInfo info = query_server_info(ip, port);
                if (!info.name.empty()) {
                    std::lock_guard<std::mutex> lock(mutex);
                    servers.push_back(info);
                }
                });
        }
    }

    for (auto& thread : threads) {
        thread.join();
    }

    print_server_info(servers);
}

std::pair<std::vector<std::string>, std::vector<int>> get_user_input() {
    std::vector<std::string> ips;
    std::vector<int> ports;

    std::string user_input;
    std::cout << "请输入服务器IP地址或域名（例如 127.0.0.1 或 example.com:27015）：";
    std::getline(std::cin, user_input);

    if (user_input.empty()) {
        std::cout << "未输入IP地址或域名，使用默认IP和端口：127.0.0.1:27015" << std::endl;
        ips.push_back("127.0.0.1");
        ports.push_back(27015);
        return std::make_pair(ips, ports);
    }

    size_t colon_pos = user_input.find(':');
    if (colon_pos != std::string::npos) {
        std::string ip_or_domain = user_input.substr(0, colon_pos);
        std::string port_str = user_input.substr(colon_pos + 1);

        int port = 27015;
        try {
            port = std::stoi(port_str);
        }
        catch (std::invalid_argument&) {
            std::cout << "端口格式错误，使用默认端口：27015" << std::endl;
        }

        // 检查输入的是IP地址还是域名
        if (is_valid_ip(ip_or_domain)) {
            // std::cout << "输入的是有效的IP地址: " << ip_or_domain << std::endl;
            ips.push_back(ip_or_domain);
        }
        else {
            std::cout << "正在解析域名: " << ip_or_domain << "..." << std::endl;
            std::string ip = resolve_domain(ip_or_domain);
            if (ip.empty()) {
                std::cout << "无法解析域名，使用默认IP：127.0.0.1" << std::endl;
                ip = "127.0.0.1";
            }
            else {
                std::cout << "域名解析成功: " << ip_or_domain << " -> " << ip << std::endl;
            }
            ips.push_back(ip);
        }

        ports.push_back(port);
        return std::make_pair(ips, ports);
    }

    std::string ip_or_domain = user_input;
    // 检查输入的是IP地址还是域名
    if (is_valid_ip(ip_or_domain)) {
        std::cout << "输入的是有效的IP地址: " << ip_or_domain << std::endl;
        ips.push_back(ip_or_domain);
    }
    else {
        std::cout << "正在解析域名: " << ip_or_domain << "..." << std::endl;
        std::string ip = resolve_domain(ip_or_domain);
        if (ip.empty()) {
            std::cout << "无法解析域名，使用默认IP：127.0.0.1" << std::endl;
            ip = "127.0.0.1";
        }
        else {
            std::cout << "域名解析成功: " << ip_or_domain << " -> " << ip << std::endl;
        }
        ips.push_back(ip);
    }

    std::cout << "请输入端口（例如 27015）或端口范围（例如 27015-27025）：";
    std::getline(std::cin, user_input);

    if (user_input.empty()) {
        std::cout << "未输入端口，使用默认端口：27015" << std::endl;
        ports.push_back(27015);
    }
    else if (user_input.find('-') != std::string::npos) {
        size_t dash_pos = user_input.find('-');
        std::string start_port_str = user_input.substr(0, dash_pos);
        std::string end_port_str = user_input.substr(dash_pos + 1);

        try {
            int start_port = std::stoi(start_port_str);
            int end_port = std::stoi(end_port_str);

            for (int port = start_port; port <= end_port; ++port) {
                ports.push_back(port);
            }
        }
        catch (std::invalid_argument&) {
            std::cout << "端口范围格式错误，使用默认端口：27015" << std::endl;
            ports.push_back(27015);
        }
    }
    else {
        try {
            int port = std::stoi(user_input);
            ports.push_back(port);
        }
        catch (std::invalid_argument&) {
            std::cout << "端口格式错误，使用默认端口：27015" << std::endl;
            ports.push_back(27015);
        }
    }

    return std::make_pair(ips, ports);
}

int main() {
    SetConsoleTitle(L"github.com/zetsr");

    while (true) {
        try {
            auto input = get_user_input();
            query_all_servers(input.first, input.second);
        }
        catch (std::exception& e) {
            std::cerr << "发生未知错误: " << e.what() << std::endl;
        }
    }

    return 0;
}