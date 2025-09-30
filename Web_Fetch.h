void listFiles(const char *dir) {
  Serial.print("Listing files in directory: ");
  Serial.println(String(dir));
  File root = SPIFFS.open(dir);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    Serial.println("File: " + String(file.name()) + ", Size: " + file.size());
    file = root.openNextFile();
  }
}
// Fetch a file from the URL given and save it in SPIFFS
// Return 1 if a web fetch was needed or 0 if file already exists
bool getFile(String url, String filename) {
	// If it exists then no need to fetch it
	if (SPIFFS.exists(filename) == true) {
		Serial.println("Found " + filename);
		return 0;
	}

	Serial.println("Downloading "  + filename + " from " + url);

	// Check WiFi connection
	if ((WiFi.status() == WL_CONNECTED)) {

		Serial.print("[HTTP] begin...\n");

#ifdef ARDUINO_ARCH_ESP8266
		std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
		client -> setInsecure();
		HTTPClient http;
		http.begin(*client, url);
#else
		HTTPClient http;
		// Configure server and url
		http.begin(url);
#endif

		Serial.print("[HTTP] GET...\n");
		// Start connection and send HTTP header
		int httpCode = http.GET();
		if (httpCode > 0) {
			fs::File f = SPIFFS.open(filename, "w+");
			if (!f) {
				Serial.println("file open failed");
				return 0;
			}
			// HTTP header has been send and Server response header has been handled
			Serial.printf("[HTTP] GET... code: %d\n", httpCode);

			// File found at server
			if (httpCode == HTTP_CODE_OK) {

				// Get length of document (is -1 when Server sends no Content-Length header)
				int total = http.getSize();
				int len = total;

				// Create buffer for read
				uint8_t buff[128] = { 0 };

				// Get tcp stream
				WiFiClient * stream = http.getStreamPtr();

				// Read all data from server
				listFiles("/");
				while (http.connected() && (len > 0 || len == -1)) {
					// Get available data size
					size_t size = stream->available();

					if (size && SPIFFS.totalBytes()-SPIFFS.usedBytes()>size) {
						// Read up to 128 bytes
						int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

						// Write it to file
						f.write(buff, c);

						// Calculate remaining bytes
						if (len > 0) {
							len -= c;
						}
					} else if (size) {
						Serial.println("Not enough space, deleting file");
						File root = SPIFFS.open("/");
						if (!root) {
							Serial.println("Failed to open directory");
							return 0;
						}
						File file = root.openNextFile();
						while (file) {
							String rm = String(file.name());
							file = root.openNextFile();
							if (rm!="previous-track.bmp" && rm!="next-track.bmp" && rm!="circle-pause-regular.bmp" && rm!="circle-play-regular.bmp" && rm!="previous-track-reverse.bmp" && rm!="circle-pause-reverse.bmp" && rm!="circle-play-reverse.bmp" && rm!="next-track-reverse.bmp") {
								SPIFFS.remove("/"+rm);
							}

						}
						// Read up to 128 bytes
						int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

						// Write it to file
						f.write(buff, c);

						// Calculate remaining bytes
						if (len > 0) {
							len -= c;
						}
					}
					yield();
				}
				Serial.println();
				Serial.print("[HTTP] connection closed or file end.\n");
			}
			f.close();
		}
		else {
			Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
		}
		http.end();
	}
	return 1; // File was fetched from web
}
