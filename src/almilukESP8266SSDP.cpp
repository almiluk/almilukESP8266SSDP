#ifndef LWIP_OPEN_SRC
#define LWIP_OPEN_SRC
#endif

#include <functional>
#include "almilukESP8266SSDP.h"
#include "WiFiUdp.h"
#include "debug.h"

extern "C" {
#include "osapi.h"
#include "ets_sys.h"
#include "user_interface.h"
}

#include "lwip/opt.h"
#include "lwip/udp.h"
#include "lwip/inet.h"
#include "lwip/igmp.h"
#include "lwip/mem.h"
#include "include/UdpContext.h"
//#define DEBUG_SSDP Serial

#define SSDP_PORT		 1900
#define SSDP_METHOD_SIZE 10
#define SSDP_URI_SIZE	 2
#define SSDP_BUFFER_SIZE 64

// ssdp ipv6 is FF05::C
// lwip-v2's igmp_joingroup only supports IPv4
#define SSDP_MULTICAST_ADDR 239, 255, 255, 250

static const char _ssdp_response_template[] PROGMEM =
"HTTP/1.1 200 OK\r\n"
"EXT:\r\n";

static const char _ssdp_notify_template[] PROGMEM =
"NOTIFY * HTTP/1.1\r\n"
"HOST: 239.255.255.250:1900\r\n"
"NTS: ssdp:alive\r\n";

static const char _ssdp_notify_bb_template[] PROGMEM =
"NOTIFY * HTTP/1.1\r\n"
"HOST: 239.255.255.250:1900\r\n"
"NTS: ssdp:byebye\r\n";

static const char _ssdp_packet_template[] PROGMEM =
"%s" // _ssdp_response_template / _ssdp_notify_template
"CACHE-CONTROL: max-age=%u\r\n" // _interval
"SERVER: Arduino/1.0 UPNP/2.0 %s/%s\r\n" // _modelName, _modelNumber
"USN: %s\r\n" // _uuid
"%s: %s\r\n"  // "NT" or "ST", serviceType or _deviceType or uuid
"LOCATION: http://%s:%u/%s\r\n" // WiFi.localIP(), _port, _schemaURL
"BOOTID.UPNP.ORG: %d\r\n" // _bootId
"CONFIGID.UPNP.ORG: %d\r\n"; // _configId

static const char _ssdp_schema_template[] PROGMEM =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/xml\r\n"
"Connection: close\r\n"
"Access-Control-Allow-Origin: *\r\n"
"\r\n"
"<?xml version=\"1.0\"?>"
"<root xmlns=\"urn:schemas-upnp-org:device-1-0\""
"	configId=\"%d\">"
"<specVersion>"
"<major>2</major>"
"<minor>0</minor>"
"</specVersion>"
"<URLBase>http://%s:%u/</URLBase>" // WiFi.localIP(), _port
"<device>"
"<deviceType>urn:%s</deviceType>"
"<friendlyName>%s</friendlyName>"
"<presentationURL>%s</presentationURL>"
"<serialNumber>%s</serialNumber>"
"<modelName>%s</modelName>"
"<modelNumber>%s</modelNumber>"
"<modelURL>%s</modelURL>"
"<manufacturer>%s</manufacturer>"
"<manufacturerURL>%s</manufacturerURL>"
"<UDN>%s</UDN>"
"</device>"
//"<iconList>"	
//"<icon>"	
//"<mimetype>image/png</mimetype>"	
//"<height>48</height>"	
//"<width>48</width>"	
//"<depth>24</depth>"	
//"<url>icon48.png</url>"	
//"</icon>"	
//"<icon>"	
//"<mimetype>image/png</mimetype>"	
//"<height>120</height>"	
//"<width>120</width>"	
//"<depth>24</depth>"	
//"<url>icon120.png</url>"	 
//"</icon>"	
//"</iconList>"
"</root>\r\n"
"\r\n";


struct SSDPTimer {
	ETSTimer timer;
};

SSDPClass::SSDPClass()
	: _addrForResponse(0, 0, 0, 0)
{
	_uuid[0] = '\0';
	_modelNumber[0] = '\0';
	sprintf(_deviceType, "schemas-upnp-org:device:Basic:1");
	_friendlyName[0] = '\0';
	_presentationURL[0] = '\0';
	_serialNumber[0] = '\0';
	_modelName[0] = '\0';
	_modelURL[0] = '\0';
	_manufacturer[0] = '\0';
	_manufacturerURL[0] = '\0';
	_serviceTypes = nullptr;
	_servicesNum = 0;
	sprintf(_schemaURL, "ssdp/schema.xml");
}

SSDPClass::~SSDPClass() {
	end();
	_deleteServiceTypes();
}

bool SSDPClass::begin() {
	end();
	
	// Generate uuid if it isn't set
	if (strcmp(_uuid, "") == 0) {
		uint32_t chipId = ESP.getChipId();
		sprintf_P(_uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
			(uint16_t)((chipId >> 16) & 0xff),
			(uint16_t)((chipId >> 8) & 0xff),
			(uint16_t)chipId & 0xff);
	}

	#ifdef DEBUG_SSDP
		DEBUG_SSDP.printf("SSDP UUID: %s\n", (char*)_uuid);
	#endif

	// Configurate udp server

	assert(NULL == _server);

	_server = new UdpContext;
	_server->ref();

	IPAddress local_addr = WiFi.localIP();
	IPAddress mcast_addr(SSDP_MULTICAST_ADDR);

	if (igmp_joingroup(local_addr, mcast_addr) != ERR_OK) {
	#ifdef DEBUG_SSDP
			DEBUG_SSDP.printf_P(PSTR("SSDP failed to join igmp group\n"));
	#endif
		return false;
	}

	if (!_server->listen(IP_ADDR_ANY, SSDP_PORT)) {
		return false;
	}

	_server->setMulticastInterface(local_addr);
	_server->setMulticastTTL(_ttl);
	_server->onRx(std::bind(&SSDPClass::_update, this));
	if (!_server->connect(mcast_addr, SSDP_PORT)) {
		return false;
	}

	_startTimer();

	return true;
}

void SSDPClass::end() {
	if (!_server)
		return; // object is zeroed already, nothing to do

	#ifdef DEBUG_SSDP
		DEBUG_SSDP.printf_P(PSTR("SSDP end ... "));
	#endif

	_advertiseAll(NOTIFY_BB);

	// undo all initializations done in begin(), in reverse order
	_stopTimer();

	_server->disconnect();

	IPAddress local_addr = WiFi.localIP();
	IPAddress mcast_addr(SSDP_MULTICAST_ADDR);

	if (igmp_leavegroup(local_addr, mcast_addr) != ERR_OK) {
	#ifdef DEBUG_SSDP
			DEBUG_SSDP.printf_P(PSTR("SSDP failed to leave igmp group\n"));
	#endif
	}

	_server->unref();
	_server = 0;

	#ifdef DEBUG_SSDP
		DEBUG_SSDP.printf_P(PSTR("ok\n"));
	#endif
}
void SSDPClass::_sendSSDPMessage(MessageType msg_type, const char* st_on_nt_val, const char* usn) {
	char buffer[1460];
	IPAddress ip = WiFi.localIP();

	char valueBuffer[strlen_P(_ssdp_notify_template) + 1];
	switch (msg_type) {
	case RESPONSE:
		strcpy_P(valueBuffer, _ssdp_response_template);
		break;
	case NOTIFY_ALIVE:
		strcpy_P(valueBuffer, _ssdp_notify_template);
		break;
	case NOTIFY_BB:
		strcpy_P(valueBuffer, _ssdp_notify_bb_template);
		break;
	default:
		#ifdef DEBUG_SSDP
				DEBUG_SSDP.print("SSDP ERROR: Incorrect type or method for sending message.");
		#endif
		return;
	}

	int len = snprintf_P(buffer, sizeof(buffer),
		_ssdp_packet_template,
		valueBuffer,
		_interval,
		_modelName,
		_modelNumber,
		usn,
		(msg_type == RESPONSE) ? "ST" : "NT",
		st_on_nt_val,
		ip.toString().c_str(), _port, _schemaURL,
		_bootId,
		_configId
	);

	_sending = true;
	_server->append(buffer, len);

	IPAddress remoteAddr;
	uint16_t remotePort;
	if (msg_type == RESPONSE) {
		on_response();
		remoteAddr = _addrForResponse;
		remotePort = _portForResponse;
		#ifdef DEBUG_SSDP
				DEBUG_SSDP.print("Sending Response to ");
		#endif
	} else {
		if (msg_type == NOTIFY_ALIVE)
			on_notify_alive();
		else if (msg_type == NOTIFY_BB)
			on_notify_bb();
		remoteAddr = IPAddress(SSDP_MULTICAST_ADDR);
		remotePort = SSDP_PORT;
		#ifdef DEBUG_SSDP
				DEBUG_SSDP.println("Sending Notify to ");
		#endif
	}
	_server->append("\r\n", 2);
	_sending = false;
	#ifdef DEBUG_SSDP
		DEBUG_SSDP.print(IPAddress(remoteAddr));
		DEBUG_SSDP.print(":");
		DEBUG_SSDP.println(remotePort);
		DEBUG_SSDP.print("Successfully sent: ");
		DEBUG_SSDP.println(_server->send(remoteAddr, remotePort));
	#else
		_server->send(remoteAddr, remotePort);
	#endif
}

void SSDPClass::_sendNotificationMessage(const char* nt_header, const char* usn_header) {
	_sendSSDPMessage(NOTIFY_ALIVE, nt_header, usn_header);
}

void SSDPClass::_advertiseTarget(MessageType msg_type, int16_t target) {
	char stnt_buff[SSDP_ST_VAL_SIZE] = { 0 };
	_getTargetStOrNtHeader(target, stnt_buff, sizeof(stnt_buff));
	char usn_buff[sizeof(stnt_buff) + 12] = { 0 };
	_getTargetUsnHeader(target, stnt_buff, usn_buff, sizeof(usn_buff));
	_sendSSDPMessage(msg_type, stnt_buff, usn_buff);
}

void SSDPClass::_advertiseAll(MessageType msg_type) {
	_advertiseTarget(msg_type, rootdevice);
	_advertiseTarget(msg_type, uuid);
	_advertiseTarget(msg_type, deviceType);

	for (int16_t i = 0; i < _servicesNum; i++)
		_advertiseTarget(msg_type, i);
}

void SSDPClass::_getTargetUsnHeader(int16_t target, const char* st_or_nt_val, char* buffer, int16_t buffer_size) {
	buffer[0] = '\0';
	if (target == uuid)
		strlcpy(buffer, st_or_nt_val, buffer_size);
	else
		snprintf(buffer, buffer_size, "uuid:%s::%s", _uuid, st_or_nt_val);
}

void SSDPClass::_getTargetStOrNtHeader(int16_t target, char buffer[], int16_t buffer_size) {
	buffer[0] = '\0';
	switch (target)
	{
	case uuid:
		snprintf(buffer, buffer_size, "uuid:%s", _uuid);
		break;
	case rootdevice:
		strlcpy(buffer, "upnp:rootdevice", buffer_size);
		break;
	case deviceType:
		snprintf(buffer, buffer_size, "urn:%s", _deviceType);
		break;
	default:
		if (target >= 0 && target < _servicesNum)
			snprintf(buffer, buffer_size, "urn:%s", _serviceTypes[target]);
		break;
	}
}

void SSDPClass::schema(Print& client) const {
	IPAddress ip = WiFi.localIP();
	char buffer[strlen_P(_ssdp_schema_template) + 1];
	strcpy_P(buffer, _ssdp_schema_template);
	client.printf(buffer,
		_configId,
		ip.toString().c_str(), _port,
		_deviceType,
		_friendlyName,
		_presentationURL,
		_serialNumber,
		_modelName,
		_modelNumber,
		_modelURL,
		_manufacturer,
		_manufacturerURL,
		_uuid
	);
}

void SSDPClass::_update() {
	if (_advertisement_target == none && _server->next()) {
		_addrForResponse = _server->getRemoteAddress();
		_portForResponse = _server->getRemotePort();

		typedef enum { METHOD, URI, PROTO, KEY, VALUE, ABORT } states;
		states state = METHOD;

		typedef enum { START, MAN, ST, MX } headers;
		headers header = START;

		uint8_t cursor = 0;
		uint8_t cr = 0;

		char buffer[SSDP_BUFFER_SIZE] = { 0 };

		int len = 0;

		while (_server->getSize() > 0) {
			char c = _server->read();

			(c == '\r' || c == '\n') ? cr++ : cr = 0;

			switch (state) {
			case METHOD:
				if (c == ' ') {
					if (strcmp(buffer, "M-SEARCH") == 0) 
						state = URI;
					else
						state = ABORT;
					cursor = 0;

				} else if (cursor < SSDP_METHOD_SIZE - 1) {
					buffer[cursor++] = c;
					buffer[cursor] = '\0';
				}
				break;
			case URI:
				if (c == ' ') {
					if (strcmp(buffer, "*")) state = ABORT;
					else state = PROTO;
					cursor = 0;
				} else if (cursor < SSDP_URI_SIZE - 1) {
					buffer[cursor++] = c;
					buffer[cursor] = '\0';
				}
				break;
			case PROTO:
				if (cr == 2) {
					state = KEY;
					cursor = 0;
				}
				break;
			case KEY:
				if (cr == 4) {
					_process_time = millis();
				} else if (c == ' ') {
					cursor = 0;
					state = VALUE;
				} else if (c != '\r' && c != '\n' && c != ':' && cursor < SSDP_BUFFER_SIZE - 1) {
					buffer[cursor++] = c;
					buffer[cursor] = '\0';
				}
				break;
			case VALUE:
				if (cr == 2) {
					switch (header) {
					case START:
						break;
					case MAN:
						#ifdef DEBUG_SSDP
							DEBUG_SSDP.printf("MAN: %s\n", (char*)buffer);
						#endif
						break;
					case ST:
						_process_time = millis();
						state = KEY;
						len = strlen(buffer);

						if (!strcasecmp(buffer, "ssdp:all")) {
							_advertisement_target = all;
						} else if (!strcasecmp(buffer, "upnp:rootdevice")) {
							_advertisement_target = rootdevice;
						} else if (len > 4 && !strcasecmp(buffer + 4, _deviceType)) {
							_advertisement_target = deviceType;
						} else if (len > 5 && !strcasecmp(buffer + 5, _uuid)) {
							_advertisement_target = uuid;
						} else if (len > 4) {
							for (int i = 0; i < _servicesNum; i++) {
								if (!strcasecmp(buffer + 4, _serviceTypes[i])) {
									_advertisement_target = i;
									break;
								}
							}
						}

						if (_advertisement_target == none) {
							state = ABORT;
							#ifdef DEBUG_SSDP
								DEBUG_SSDP.printf("REJECT: %s\n", (char*)buffer);
							#endif
						}
						break;
					case MX:
						_delay = random(0, atoi(buffer)) * 1000L;
						break;
					}

					if (state != ABORT) {
						state = KEY;
						header = START;
						cursor = 0;
					}
				} else if (c != '\r' && c != '\n') {
					if (header == START) {
						if (strncmp(buffer, "MA", 2) == 0) header = MAN;
						else if (strcmp(buffer, "ST") == 0) header = ST;
						else if (strcmp(buffer, "MX") == 0) header = MX;
					}

					if (cursor < SSDP_BUFFER_SIZE - 1) {
						buffer[cursor++] = c;
						buffer[cursor] = '\0';
					}
				}
				break;
			case ABORT:
				_delay = 0;
				_advertisement_target = none;
				break;
			}
		}
	}

	if (_advertisement_target != none && (millis() - _process_time) > _delay) {
		if (_advertisement_target == all) {
			_advertiseAll(RESPONSE);
		} else {
			_advertiseTarget(RESPONSE, _advertisement_target);
		}
		_delay = 0;
		_advertisement_target = none;
	} else if (_notify_time == 0 || (millis() - _notify_time) > (_interval * 1000L)) {
		// Send NOTIFY_ALIVE messages about all every <_interval> seconds.
		_notify_time = millis();
		_advertiseAll(NOTIFY_ALIVE);
	}

	// If get new request while previous isn't answered already, ignore it.
	if (_advertisement_target != none) {
		while (_server->next())
			_server->flush();
	}

}

void SSDPClass::setSchemaURL(const char* url) {
	strlcpy(_schemaURL, url, sizeof(_schemaURL));
}

void SSDPClass::setHTTPPort(uint16_t port) {
	_port = port;
}

void SSDPClass::setDeviceType(const char *domain, const char* deviceType, const char* version) {
	snprintf_P(_deviceType, sizeof(_deviceType), "%s:device:%s:%s", 
				domain, deviceType, version);
}

void SSDPClass::setUUID(const char* uuid) {
	strlcpy(_uuid, uuid, sizeof(_uuid));
}

void SSDPClass::setName(const char* name) {
	strlcpy(_friendlyName, name, sizeof(_friendlyName));
}

void SSDPClass::setURL(const char* url) {
	strlcpy(_presentationURL, url, sizeof(_presentationURL));
}

void SSDPClass::setSerialNumber(const char* serialNumber) {
	strlcpy(_serialNumber, serialNumber, sizeof(_serialNumber));
}

void SSDPClass::setSerialNumber(const uint32_t serialNumber) {
	snprintf(_serialNumber, sizeof(uint32_t) * 2 + 1, "%08X", serialNumber);
}

void SSDPClass::setModelName(const char* name) {
	strlcpy(_modelName, name, sizeof(_modelName));
}

void SSDPClass::setModelNumber(const char* num) {
	strlcpy(_modelNumber, num, sizeof(_modelNumber));
}

void SSDPClass::setModelURL(const char* url) {
	strlcpy(_modelURL, url, sizeof(_modelURL));
}

void SSDPClass::setManufacturer(const char* name) {
	strlcpy(_manufacturer, name, sizeof(_manufacturer));
}

void SSDPClass::setManufacturerURL(const char* url) {
	strlcpy(_manufacturerURL, url, sizeof(_manufacturerURL));
}

void SSDPClass::setBootId(int boot_id) {
	if (boot_id >= 0)
		_bootId = boot_id;
}

void SSDPClass::setConfigId(int config_id) {
	if (config_id >= 0)
		_configId = config_id;
}

void SSDPClass::setServiceTypes(SSDPServiceType types[], uint8_t services_num) {
	_deleteServiceTypes();
	_serviceTypes = new char* [services_num];
	for (int i = 0; i < services_num; i++) {
		_serviceTypes[i] = new char[SSDP_SERVICE_TYPE_SIZE];
		snprintf(_serviceTypes[i], SSDP_SERVICE_TYPE_SIZE, "%s:service:%s:%s",
					types[i].domain, types[i].service, types[i].version);
	}
	_servicesNum = services_num;
}

void SSDPClass::setTTL(const uint8_t ttl) {
	_ttl = ttl;
}

void SSDPClass::setInterval(uint32_t interval) {
	_interval = interval;
}

void SSDPClass::setAutorun(bool flag) {
	_auto_mode = flag;
}

void SSDPClass::loop() {
	if (_server)
		_update();
}

void SSDPClass::_onTimerStatic(SSDPClass* self) {
	if (self->_auto_mode)
		self->_update();
	else
		self->_stopTimer();
}

void SSDPClass::_deleteServiceTypes() {
	if (!_serviceTypes) {
		_servicesNum = 0;
		return;
	}

	for (int i = 0; i < _servicesNum; i++)
		if (_serviceTypes[i]) {
			_advertiseTarget(NOTIFY_BB, i);
			delete _serviceTypes[i];
		}
	delete _serviceTypes;
	_servicesNum = 0;
}

void SSDPClass::addHeader(const char* header, const char* value) {
	if (!_sending)
		return;
		
	int len = strlen(header) + strlen(value) + 5;
	char buffer[len];
	snprintf(buffer, sizeof(buffer), "%s: %s\r\n", header, value);
	_server->append(buffer, len);
}

void SSDPClass::_startTimer() {
	_stopTimer();
	_timer = new SSDPTimer();
	ETSTimer* tm = &(_timer->timer);
	const int interval = 1000;
	os_timer_disarm(tm);
	os_timer_setfn(tm, reinterpret_cast<ETSTimerFunc*>(&SSDPClass::_onTimerStatic), reinterpret_cast<void*>(this));
	os_timer_arm(tm, interval, 1 /* repeat */);
}

void SSDPClass::_stopTimer() {
	if (!_timer)
		return;

	ETSTimer* tm = &(_timer->timer);
	os_timer_disarm(tm);
	delete _timer;
	_timer = NULL;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SSDP)
	SSDPClass SSDP;
#endif
