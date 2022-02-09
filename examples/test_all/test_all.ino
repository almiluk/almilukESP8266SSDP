#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#define NO_GLOBAL_SSDP
#include <almilukESP8266SSDP.h>

#define NETNAME ""
#define PASSWORD ""

class mySSDPClass : public SSDPClass {
public:
	int response_cnt = 0;

	mySSDPClass() {
		setManufacturer("almiluk");
		setManufacturerURL("https://github.com/almiluk");
		setModelName("almilukESP8266SSDP_test");
		setDeviceType("almiluk-domain", "esp8266-ssdp-test", "1.0");
		SSDPServiceType services[] = {
			{"almiluk-domain", "service1", "v1"},
			{"some-other-domain", "service1", "1.1.0"},
			{"some-other-domain", "service2", "abcd"}
		};
		setServiceTypes(services, 3);
		// You can set all combinations of attributes you want.
	}

protected:

	void on_response() {
		Serial.print("Sending SSDP response for target: ");
		Serial.println(getAdvertisementTarget());
		addHeader("resp_header", "resp_header_val");
		response_cnt++;
	}

	void on_notify_alive() {
		Serial.print("Sending SSDP alive notification for target: ");
		Serial.println(getAdvertisementTarget());
		addHeader("alive_header", "alive_header_val");
	}

	void on_notify_bb() {
		Serial.print("Sending SSDP byebye notification for target: ");
		Serial.println(getAdvertisementTarget());
		addHeader("byebye_header", "byebye_header_val");
	}
};

mySSDPClass g_ssdp;
ESP8266WebServer g_webServer(80);

void setup() {
	Serial.begin(115200);

	WiFi.mode(WIFI_STA);
	WiFi.begin(NETNAME, PASSWORD);
	Serial.print("Waiting for WiFi connection");
	while (!WiFi.localIP().isSet()) {
		Serial.print('.');
		delay(500);
	}
	Serial.println("\nConnected to WiFi");

	if (g_ssdp.begin())
		Serial.println("SSDP begun");
	else
		Serial.println("SSDP init failed");

	g_webServer.on("/ssdp/schema.xml", []() {
		g_ssdp.schema(g_webServer.client());
		});
	g_webServer.begin();

	Serial.println("Web server started");
}

void loop() {
	if (g_ssdp.response_cnt == 9) {
		delay(10000);
		g_ssdp.end();
		g_ssdp.response_cnt++;
	}
	g_webServer.handleClient();
	delay(16);
}
