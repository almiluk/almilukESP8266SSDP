#ifndef ALMILUK_ESP8266SSDP_H
#define ALMILUK_ESP8266SSDP_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

class UdpContext;

#define SSDP_UUID_SIZE				42
#define SSDP_SCHEMA_URL_SIZE		64
#define SSDP_DEVICE_TYPE_SIZE		64
#define SSDP_FRIENDLY_NAME_SIZE		64
#define SSDP_SERIAL_NUMBER_SIZE		37
#define SSDP_PRESENTATION_URL_SIZE	128
#define SSDP_MODEL_NAME_SIZE		32
#define SSDP_MODEL_URL_SIZE			128
#define SSDP_MODEL_NUMBER_SIZE		32
#define SSDP_MANUFACTURER_SIZE		64
#define SSDP_MANUFACTURER_URL_SIZE  128
#define SSDP_SERVICE_TYPE_SIZE		128
#define SSDP_ST_VAL_SIZE			SSDP_SERVICE_TYPE_SIZE + 4
#define SSDP_USN_SIZE SSDP_UUID_SIZE SSDP_DOMAIN_NAME_SIZE + 12

#define SSDP_INTERVAL_SECONDS		1200
#define SSDP_MULTICAST_TTL			5
#define SSDP_HTTP_PORT				80


struct SSDPTimer;

class SSDPClass {
public:
	struct SSDPServiceType {
		const char* domain = { 0 };
		const char* service = { 0 };
		const char* version = { 0 };
		SSDPServiceType(const char* domain, const char* service, const char* version) :
			domain(domain), service(service), version(version) {}
	};

	SSDPClass();
	~SSDPClass();
	bool begin();
	void end();
	void schema(WiFiClient client) const { schema((Print&)std::ref(client)); }
	void schema(Print& print) const;
	void setDeviceType(const String& domain, const String& deviceType, const String& version) 
		{ setDeviceType(domain.c_str(), deviceType.c_str(), version.c_str()); }
	void setDeviceType(const char* domain, const char* deviceType, const char* version);
	String getDeviceType() { return String(_deviceType); }

	/*To define a custom UUID, you must call the method before begin(). Otherwise an automatic UUID based on CHIPID will be generated.*/
	void setUUID(const String& uuid) { setUUID(uuid.c_str()); }
	void setUUID(const char* uuid);
	String getUUID() { return String(_uuid); }

	void setName(const String& name) { setName(name.c_str()); }
	void setName(const char* name);
	String getName() { return String(_friendlyName); }
	void setURL(const String& url) { setURL(url.c_str()); }
	void setURL(const char* url);
	String getURL() { return String(_presentationURL); }
	void setSchemaURL(const String& url) { setSchemaURL(url.c_str()); }
	void setSchemaURL(const char* url);
	String getSchemaURL() { return String(_schemaURL); }
	void setSerialNumber(const String& serialNumber) { setSerialNumber(serialNumber.c_str()); }
	void setSerialNumber(const char* serialNumber);
	void setSerialNumber(const uint32_t serialNumber);
	String getSerialNumber() { return String(_serialNumber); }
	void setModelName(const String& name) { setModelName(name.c_str()); }
	void setModelName(const char* name);
	String getModelName() { return String(_modelName); }
	void setModelNumber(const String& num) { setModelNumber(num.c_str()); }
	void setModelNumber(const char* num);
	String getModelNumber() { return String(_modelNumber); }
	void setModelURL(const String& url) { setModelURL(url.c_str()); }
	void setModelURL(const char* url);
	String getModelURL() { return String(_modelURL); }
	void setManufacturer(const String& name) { setManufacturer(name.c_str()); }
	void setManufacturer(const char* name);
	String getManufacturer() { return String(_manufacturer); }
	void setManufacturerURL(const String& url) { setManufacturerURL(url.c_str()); }
	void setManufacturerURL(const char* url);
	String getManufacturerURL() { return String(_manufacturerURL); }
	// Some non-negative, 31-bit integer value that is unique for every entering of device to SSDP(UPnP) network.
	// For example, timestamp of entering moment. Is always 0 by default.
	void setBootId(int boot_id);
	int getBootId() { return _bootId; }

	/* Some non-negative, 31-bit integer value that is unique for every combination of
	* Device Description Document of the device and Service Control Protocol Description
	* of every services on the device. For example hash of sum or concatenation
	* of device and all services versions. Is always 0 by default.
	*/
	void setConfigId(int config_id);
	int getConfigId() { return _configId; }

	void setServiceTypes(SSDPServiceType types[], uint8_t services_num);
	void setHTTPPort(uint16_t port);
	void setTTL(uint8_t ttl);
	void setInterval(uint32_t interval);

	/* If true, SSDP will work automatically without any calls in loop().
	* Else you must call loop method of this class regularly (in loop function).
	* It is false by default.
	*/
	void setAutorun(bool flag);

	void loop();

protected:
	// if >= 0, it is index of service in _serviceTypes array
	enum advertisement_target_t { none = -5, uuid, all, deviceType, rootdevice };
	int getAdvertisementTarget() { return _advertisement_target; };

	void addHeader(const char* header, const char* value);
	void addHeader(String& header, String& value) { addHeader(header.c_str(), value.c_str()); };
	virtual void on_response() {};
	virtual void on_notify_alive() {};
	virtual void on_notify_bb() {};

private:
	typedef enum {
		RESPONSE,
		SEARCH,
		NOTIFY_ALIVE,
		NOTIFY_BB
	} ssdp_message_type_t;

	void _send(ssdp_message_type_t msg_type, const char* st_on_nt_val, const char* usn);
	void _notify(const char* nt_header, const char* usn_header);
	void _advertise_about_target(ssdp_message_type_t msg_type, int16_t target);
	void _advertise_about_all(ssdp_message_type_t msg_type);
	void _get_target_usn(int16_t target, const char* st_or_nt_val, char* buffer, int16_t buffer_size);
	void _get_target_st_or_nt(int16_t target, char* buffer, int16_t buffer_size);
	void _update();
	void _startTimer();
	void _stopTimer();
	static void _onTimerStatic(SSDPClass* self);
	void _deleteServiceTypes();

	UdpContext* _server = nullptr;
	SSDPTimer* _timer = nullptr;
	uint16_t _port = SSDP_HTTP_PORT;
	uint8_t _ttl = SSDP_MULTICAST_TTL;
	uint32_t _interval = SSDP_INTERVAL_SECONDS;
	bool _auto_mode = false;

	IPAddress _respondToAddr;
	uint16_t  _respondToPort = 0;

	int _advertisement_target = none;
	unsigned short _delay = 0;
	unsigned long _process_time = 0;
	unsigned long _notify_time = 0;
	bool _sending = false;

	char _schemaURL[SSDP_SCHEMA_URL_SIZE];
	char _uuid[SSDP_UUID_SIZE];
	// Device type is stored with domain name and version
	char _deviceType[SSDP_DEVICE_TYPE_SIZE];

	char _friendlyName[SSDP_FRIENDLY_NAME_SIZE];
	char _serialNumber[SSDP_SERIAL_NUMBER_SIZE];
	char _presentationURL[SSDP_PRESENTATION_URL_SIZE];
	char _manufacturer[SSDP_MANUFACTURER_SIZE];
	char _manufacturerURL[SSDP_MANUFACTURER_URL_SIZE];
	char _modelName[SSDP_MODEL_NAME_SIZE];
	char _modelURL[SSDP_MODEL_URL_SIZE];
	char _modelNumber[SSDP_MODEL_NUMBER_SIZE];
	int _bootId = 0;
	int _configId = 0;
	// Service types are stored with domain name and version
	char** _serviceTypes = nullptr;

	uint8_t _servicesNum = 0;
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SSDP)
	extern SSDPClass SSDP;
#endif

#endif
