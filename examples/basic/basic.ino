/*
*  You can (or will be able soon) find full description and user guide on https://github.com/almiluk/almilukESP8266SSDP/blob/master/readme.md
*/

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <almilukESP8266SSDP.h>

#define NETNAME ""
#define PASSWORD ""

ESP8266WebServer g_webServer(80);

void setup() {
	Serial.begin(115200);

	// Connect to WiFi
	WiFi.mode(WIFI_STA);
	WiFi.begin(NETNAME, PASSWORD);
	Serial.print("Waiting for WiFi connection");
	while (!WiFi.localIP().isSet()) {
		Serial.print('.');
		delay(500);
	}
	Serial.println("\nConnected to WiFi");

	/*	Set info about the device. You don't have to set all attributes, 
	*	if you don't set some of required attributes, it will be generated automatically.
	*/	 
	// Most important attributes:
	SSDP.setDeviceType("almiluk-domain", "esp8266-ssdp-test", "1.0");

	// Secondary:
	SSDP.setManufacturer("almiluk");
	SSDP.setManufacturerURL("https://github.com/almiluk");
	SSDP.setModelName("almilukESP8266SSDP_test");
	SSDP.setName("myESP");
	SSDP.setSerialNumber("123456789");
	SSDP.setModelNumber("1");
	SSDP.setModelURL("https://description-of-device-model");
	String schema_url = "ssdp/schema.xml";
	SSDP.setSchemaURL(schema_url);

	// Additional:
	SSDPClass::SSDPServiceType services[] = {
		{"almiluk-domain", "service1", "v1"},
		{"some-other-domain", "service1", "1.1.0"},
		{"some-other-domain", "service2", "abcd"}
	};
	SSDP.setServiceTypes(services, 3);

	// Start SSDP
	if (SSDP.begin())
		Serial.println("SSDP begun");
	else
		Serial.println("SSDP init failed");

	/*	Route web server to SSDP description of the device.
	*	Actually, SSDP searching will work without it, BUT SSDP standart requires
	*	that description page exists and is available. This page will be generated by
	*	library automatically, but you can set its address with SSDP.setSchemaURL method.
	*/
	g_webServer.on("/" + schema_url, []() {
		SSDP.schema(g_webServer.client());
		});
	g_webServer.begin();

	Serial.println("Web server started");
}

void loop() {
	// Perform SSDP work
	SSDP.loop();
	// Perform web server work
	g_webServer.handleClient();

	delay(16);
}
