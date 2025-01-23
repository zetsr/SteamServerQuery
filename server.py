import socket
import json
import sys

def resolve_domain(domain):
    """
    Resolve a domain name to an IP address.
    """
    try:
        ip = socket.gethostbyname(domain)
        return ip
    except socket.gaierror:
        return None

def query_server_info(ip, port):
    """
    Query the server for detailed information using the Source Engine Query Protocol.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5)

    # A2S_INFO challenge request
    challenge_request = b'\xFF\xFF\xFF\xFF\x54Source Engine Query\x00'
    try:
        sock.sendto(challenge_request, (ip, port))

        # Receive the response
        response, _ = sock.recvfrom(4096)
        if response[4] == 0x41:  # Challenge response
            challenge_token = response[5:9]
            challenge_request += challenge_token
            sock.sendto(challenge_request, (ip, port))

            # Receive the detailed server info response
            response, _ = sock.recvfrom(4096)
            return parse_server_info(response, ip, port)
        elif response[4] == 0x49:  # Detailed info response directly
            return parse_server_info(response, ip, port)
        else:
            return None

    except socket.timeout:
        return None
    except Exception:
        return None
    finally:
        sock.close()

def parse_server_info(data, ip, port):
    """
    Parse the server information from a Source Engine query response.
    """
    try:
        info = {'ip': ip, 'port': port}
        offset = 6  # Skip the initial header (4 bytes) and the type byte (1 byte)

        # Read server name
        end = data.find(b'\x00', offset)
        info['name'] = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1

        # Read map name
        end = data.find(b'\x00', offset)
        info['map'] = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1

        # Read game directory
        end = data.find(b'\x00', offset)
        info['game_directory'] = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1

        # Read game description
        end = data.find(b'\x00', offset)
        info['game_description'] = data[offset:end].decode('utf-8', errors='ignore')
        offset = end + 1

        # Read app ID
        info['app_id'] = int.from_bytes(data[offset:offset+2], byteorder='little')
        offset += 2

        # Read number of players
        info['current_players'] = data[offset]
        offset += 1

        # Read max players
        info['max_players'] = data[offset]
        offset += 1

        # Read number of bots
        info['bots'] = data[offset]
        offset += 1

        # Read server type
        info['server_type'] = chr(data[offset])
        offset += 1

        # Read server OS
        info['os'] = 'Linux' if data[offset] == 0x6C else 'Windows' if data[offset] == 0x77 else 'macOS' if data[offset] == 0x6D else 'Unknown'
        offset += 1

        # Read VAC status
        info['vac'] = 'Enabled' if data[offset] == 0x01 else 'Disabled'
        offset += 1

        # Read server version
        end = data.find(b'\x00', offset)
        version = data[offset:end].decode('utf-8', errors='ignore')
        info['version'] = version.strip()  # 清理控制字符
        offset = end + 1

        return info
    except Exception:
        return None

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(json.dumps({'error': '需要传入 IP 和端口参数'}))
        sys.exit(1)

    ip = sys.argv[1]
    port = int(sys.argv[2])

    # Resolve domain if necessary
    if not ip.replace('.', '').isdigit():
        resolved_ip = resolve_domain(ip)
        if not resolved_ip:
            print(json.dumps({'error': '域名解析失败'}))
            sys.exit(1)
        ip = resolved_ip

    # Query server info
    server_info = query_server_info(ip, port)
    if not server_info:
        print(json.dumps({'error': '查询失败: 未获取到服务器信息'}))
        sys.exit(1)

    # Output the result
    print(json.dumps(server_info))