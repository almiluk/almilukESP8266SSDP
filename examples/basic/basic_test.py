import sys
import socket
import struct
from time import sleep


SSDP_ADDR = "239.255.255.250";
SSDP_PORT = 1900;

device_domain = "almiluk-domain"
device_type = "esp8266-ssdp-test"
device_version = "1.0"

def test_ssdp_search() -> int:
    print("\n\nWaiting for responses to search requests...\n")
    mx_val = 5
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 5)
    sock.bind(('', 19001))
    
    sock.settimeout(mx_val * 1.1)

    responses_conter = 0

    sleep(1)
    st = f"urn:{device_domain}:device:{device_type}:{device_version}"
    # st = "upnp:rootdevice"
    ssdpRequest = ("M-SEARCH * HTTP/1.1\r\n"
                + "HOST: %s:%d\r\n" % (SSDP_ADDR, SSDP_PORT)
                + "MAN: \"ssdp:discover\"\r\n"
                + "MX: %d\r\n" % (mx_val, )
                + "ST: %s\r\n" % (st, ) + "\r\n")
    sock.sendto(ssdpRequest.encode(), (SSDP_ADDR, SSDP_PORT))
    while True:
        try:
            data, addr = sock.recvfrom(10240)
            data_str = data.decode()
            print(data_str)
            if ("SERVER: Arduino/1.0 UPNP/2.0" in data_str
                and "HTTP/1.1 200 OK" in data_str):
                print("Device found, address: ", addr)
                responses_conter += 1
        except socket.timeout:
            break
    
    sock.close()
    return responses_conter
    

if __name__ == '__main__':
    print("Test device found: ", test_ssdp_search())
