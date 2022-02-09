import sys
import socket
import struct
from time import sleep


SSDP_ADDR = "239.255.255.250";
SSDP_PORT = 1900;

SERVICES_NUM = 3


def get_notifications_from_arduino() -> int:
    print("Waiting for notifications...\n")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    sock.bind(('', SSDP_PORT))
    mreq = struct.pack("4sl", socket.inet_aton(SSDP_ADDR), socket.INADDR_ANY)

    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    sock.settimeout(30)

    notifications_counter = 0
    
    while notifications_counter < SERVICES_NUM + 3:
        try:
            data, addr = sock.recvfrom(10240)
            data_str = data.decode()
            if ("SERVER: Arduino/1.0 UPNP/2.0" in data_str
                and "NOTIFY" in data_str
                and "NTS: ssdp:alive" in data_str):
                notifications_counter += 1
                print(data_str)
                if not "alive_header: alive_header_val" in data_str:
                    print("ERROR: notifications does not contain an additional header\n\n")
        except socket.timeout:
            break
    sock.close()
    return notifications_counter


def test_ssdp_search() -> int:
    print("\n\nTest responses to search requests...\n")
    mx_val = 5
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 5)
    sock.bind(('', 19001))
    
    sock.settimeout(mx_val * 1.1)

    responses_conter = 0

    sleep(1)
    for st in [ "upnp:rootdevice", "ssdp:all",
    "urn:almiluk-domain:device:esp8266-ssdp-test:1.0", 
    "urn:almiluk-domain:service:service1:v1"]:
        ssdpRequest = ("M-SEARCH * HTTP/1.1\r\n"
                    + "HOST: %s:%d\r\n" % (SSDP_ADDR, SSDP_PORT)
                    + "MAN: \"ssdp:discover\"\r\n"
                    + "MX: %d\r\n" % (mx_val, )
                    + "ST: %s\r\n" % (st, ) + "\r\n")
        sock.sendto(ssdpRequest.encode(), (SSDP_ADDR, SSDP_PORT))
        while responses_conter < SERVICES_NUM + 3 + 3:
            try:
                data, addr = sock.recvfrom(10240)
                data_str = data.decode()
                print(data_str)
                if ("SERVER: Arduino/1.0 UPNP/2.0" in data_str
                    and "HTTP/1.1 200 OK" in data_str):
                    responses_conter += 1
                    if not "resp_header: resp_header_val" in data_str:
                            print("ERROR: response does not contain an additional header\n\n")
            except socket.timeout:
                break
    
    sock.close()
    return responses_conter


def get_byebye_from_arduino() -> int:
    print("\n\nWaiting for byebye message...\n")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    sock.bind(('', SSDP_PORT))
    mreq = struct.pack("4sl", socket.inet_aton(SSDP_ADDR), socket.INADDR_ANY)

    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    sock.settimeout(20)

    notifications_counter = 0
    
    while notifications_counter < SERVICES_NUM + 3:
        try:
            data, addr = sock.recvfrom(10240)
            data_str = data.decode()
            print(data_str)
            if ("SERVER: Arduino/1.0 UPNP/2.0" in data_str
                and "NOTIFY" in data_str
                and "NTS: ssdp:byebye" in data_str):
                notifications_counter += 1
                print(data_str)
                if not "byebye_header: byebye_header_val" in data_str:
                    print("ERROR: byebye message does not contain an additional header\n\n")
        except socket.timeout:
            break
    sock.close()
    return notifications_counter
    


def main():
    nt_cnt = get_notifications_from_arduino()
    rsp_cnt = test_ssdp_search()
    bb_cnt = get_byebye_from_arduino()
    print(f"Notifications: {nt_cnt}/{SERVICES_NUM + 3}")
    print(f"Search responses: {rsp_cnt}/{SERVICES_NUM + 3 + 3}")
    print(f"Byebye messages: {bb_cnt}/{SERVICES_NUM + 3}")

if __name__ == '__main__':
    main()
