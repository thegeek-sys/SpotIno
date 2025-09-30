// memory and images management
#define FS_NO_GLOBALS
#include <FS.h>
#include "SPIFFS.h"
#include <TJpg_Decoder.h>
//#include "List_SPIFFS.h"

// wireless connection
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "Web_Fetch.h"

// tft library https://github.com/Bodmer/TFT_eSPI
#include <TFT_eSPI.h>              // Hardware-specific library
#include <SPI.h>
#include "sf_pro.h"

// spotify api calls
#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>

// utility
#include <ArduinoJson.h>
#include <ESP32Encoder.h>

#include "secrets.h"

// choose between LIGHT_THEME and DARK_THEME
#define LIGHT_THEME

TFT_eSPI tft = TFT_eSPI();
IPAddress ip(192, 168, 1, 135);
uint16_t calData[5] = { 281, 3526, 359, 3403, 5 };

//------- ---------------------- ------

WiFiClientSecure client;
SpotifyArduino spotify(client, CLIENT_ID, CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);

unsigned long delayBetweenRequests = 20000; // time between requests
unsigned long requestDueTime;               // time when request due
unsigned long updateTime;                   
float pixelPerSecond;                       // how many pixel has to fill for each second of the song
float pixelPast;                            // time in pixel past
int secondsPast;                            // time past
int durationSec;                            // total duration of the song
bool isPlaying;
bool isPaused = false;
bool action = false;
int currentPlay;
int prevVolume;
int imageDelay;

uint8_t menuIndex = 0;                      // current index of playlists/song list
uint8_t offset = 0;                         // offset from menuIndex went offscreen
uint8_t maxPlaylists;                       // total playlists
uint8_t maxSongs;                           // total songs of a playlist
char playlistId[24];

enum Screen {
	PLAYING,
	MENU,
	SONGS
};
Screen screen;

// tft
#define FONT_TYPE 1
#define BAR_HEIGHT 240
#define FONT_SIZE 20

//tft.width()/2-83
#define PREV_X 157
//tft.width()/2+35
#define NEXT_X 275
//tft.width()/2-24
#define PLAY_X 216

// encoder
#define CLK_ENC 22
#define DT_ENC 35
#define SW_ENC 21
#define DIRECTION_CW  0  // clockwise direction
#define DIRECTION_CCW 1  // counter-clockwise direction
int direction;
int CLK_state;
int prev_CLK_state;
unsigned long lastSw = 0;
ESP32Encoder encoder;
int counter = 0;

UserPlaylistsResult playlists[25] = {NULL};
PlaylistResult songs[50] = {NULL};

#ifdef LIGHT_THEME
uint16_t altColor = TFT_WHITE;
uint16_t mainColor = TFT_BLACK;
#elif defined(DARK_THEME)
uint16_t altColor = TFT_BLACK;
uint16_t mainColor = TFT_WHITE;
#endif

String song;
bool changedSong = false;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
	// stop further decoding as image is running off bottom of screen
	if ( y >= tft.height() ) return 0;

	// this function will clip the image block rendering automatically at the TFT boundaries
	tft.pushImage(x, y, w, h, bitmap);  // blocking, so only returns when image block is drawn

	// return 1 to decode next block
	return 1;
}


void setup() {
	Serial.begin(115200);
	Serial.setTimeout(15000);

	if (!SPIFFS.begin(true)) {
		Serial.println("SPIFFS initialisation failed!");
		while (1) yield(); // stay here twiddling thumbs waiting
	}
	Serial.println("\r\nInitialisation done.");

	TJpgDec.setJpgScale(2);
	TJpgDec.setSwapBytes(true);
	TJpgDec.setCallback(tft_output);

	//WiFi.config(ip);
	client.setInsecure();
	WiFi.mode(WIFI_STA);
	WiFi.begin(SSID, PASSWORD);
	Serial.println("");

	// wait for connection
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(SSID);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	// Handle HTTPS Verification
	client.setCACert(spotify_server_cert);

	Serial.println("Refreshing Access Tokens");
	if (!spotify.refreshAccessToken())
	{
		Serial.println("Failed to get access tokens");
	}

	// encoder setup
	pinMode(SW_ENC, INPUT_PULLUP);
  encoder.attachHalfQuad(DT_ENC, CLK_ENC);
  encoder.setCount(0);

	// read the initial state of the rotary encoder's CLK pin
	prev_CLK_state = digitalRead(CLK_ENC);

	screen = PLAYING;
	// initialise tft
	tft.begin();
	tft.setRotation(3);
	tft.setFreeFont(&SFPRODISPLAYREGULAR10pt8b);
	//SPIFFS.remove("/circle-pause-regular.bmp");
	//SPIFFS.remove("/circle-play-regular.bmp");
	//SPIFFS.remove("/next-track.bmp");

	// initialise touch
	tft.setTouch(calData);

	//led
	pinMode(4, OUTPUT);
	pinMode(17, OUTPUT);
	pinMode(16, OUTPUT);
	digitalWrite(4, 0); // B
	digitalWrite(16, 0); // G
	digitalWrite(17, 0); // R
}

void handleTouch(uint16_t t_x, uint16_t t_y) {
	Serial.println((String)"clicked ("+t_x+", "+t_y+")");
	// clicked next
	if (t_x>=PREV_X && t_x<=PREV_X+48 && t_y>=260 && t_y<=308) {
		drawBmp("/previous-track-reverse.bmp", PREV_X, 260);
		spotify.previousTrack();
		action = true;
		Serial.println("PREV");
	}

	// clicked prev
	if (t_x>=NEXT_X && t_x<=NEXT_X+48 && t_y>=260 && t_y<=308) {
		drawBmp("/next-track-reverse.bmp", NEXT_X, 260);
		spotify.nextTrack();
		action = true;
		Serial.println("NEXT");
	}

	// clicked next
	if (t_x>=PLAY_X && t_x<=PLAY_X+48 && t_y>=260 && t_y<=308) {
		if (isPlaying) {
			drawBmp("/circle-play-reverse.bmp", PLAY_X, 260);
			spotify.pause();
		}
		else {
			drawBmp("/circle-pause-reverse.bmp", PLAY_X, 260);
			spotify.play();	
		}
		isPlaying = !isPlaying;
		action = true;
		Serial.println("PLAY/PAUSE");
	}

	if (t_x>=tft.width()/2+70 && t_x<=tft.width()/2+100 && t_y>=20 && t_y<=170) {
		t_y = max((int)t_y, 30);
		t_y = min((int)t_y, 160);
		prevVolume=(130-(t_y-30))*100/130;
		showVolume(prevVolume);
		spotify.setVolume(prevVolume);
		Serial.print("setting volume: ");
		Serial.println(prevVolume);
	}
}



void loop() {
	uint16_t t_x = 0, t_y = 0;
	bool pressed = tft.getTouch(&t_x, &t_y);

	if (pressed) handleTouch(t_x, t_y);

	int swState = digitalRead(SW_ENC);
	
	if(swState == LOW) {
    Serial.println("SW");
    int status;
    switch (screen) {
      case PLAYING:
        screen = MENU;
        tft.fillScreen(altColor);
        Serial.println("MENU");
        menuIndex = 0;
        offset = 0;
        do {
          status = spotify.getUserPlaylists(25, getPlaylistsCallback, playlists);
        } while (status != 200);
        break;
      case MENU:
        screen = SONGS;
        tft.fillScreen(altColor);
        Serial.println("SONGS");
        strcpy(playlistId, playlists[menuIndex].playlistId);
        Serial.println(playlistId);
        memset(playlists, 0, sizeof(playlists));
        do {
          status = spotify.getPlaylist(playlistId, 51, "IT", getResultsCallback, songs);
        } while (status != 200);
        menuIndex = 0;
        offset = 0;
        break;
      case SONGS:
        screen = PLAYING;
        tft.fillScreen(altColor);
        char playlist[42] = "spotify:playlist:";
        strcat(playlist, playlistId);

        //char playlist[] = "spotify:playlist:"+playlistId;
        char body[200];
        sprintf(body, "{\"context_uri\" : \"%s\", \"offset\": {\"position\": %d}}", playlist, menuIndex);
        if (spotify.playAdvanced(body))
        {
          Serial.println("sent!");
        }
        memset(songs, 0, sizeof(songs));
        memset(playlistId, 0, sizeof(playlistId));
        menuIndex = 0;
        offset = 0;
        action = true;
        break;
    }
    lastSw = millis();

  }

  int newC = encoder.getCount()/2;
  if (counter!=newC) {
    if (counter>newC) {
			direction = DIRECTION_CCW;
			Serial.println("CCW");
    } else {
			direction = DIRECTION_CW;
			Serial.println("CW");
    }
    counter=newC;

		switch (screen) {
			case PLAYING:
        if (direction == DIRECTION_CW) {
          prevVolume = min(prevVolume+7, 100);
          spotify.setVolume(prevVolume);
          showVolume(prevVolume);
        } else {
          prevVolume = max(prevVolume-7, 0);
          spotify.setVolume(prevVolume);
          showVolume(prevVolume);
        }
      break;
			case MENU:
        if (direction == DIRECTION_CW) {
				  if (menuIndex<maxPlaylists && tft.readPixel(479, 315) != TFT_BLACK) {
            // scrolling down without going offscreen (not the end of the screen)
				  	tft.fillRect(0, FONT_SIZE*(menuIndex-offset), 480, FONT_SIZE, altColor);
				  	tft.drawString(playlists[menuIndex].playlistName, 3, FONT_SIZE*(menuIndex++-offset), FONT_TYPE);
				  	tft.fillRect(0, menuIndex*FONT_SIZE-FONT_SIZE*offset, 480, FONT_SIZE, mainColor);
				  	tft.setTextColor(altColor);
				  	tft.drawString(playlists[menuIndex].playlistName, 3, FONT_SIZE*(menuIndex-offset), FONT_TYPE);
				  	tft.setTextColor(mainColor);
				  } else if (menuIndex<maxPlaylists) {
            // scrolling down going offscreen (moves the entire page, end of screen)
				  	Serial.println("scrolling down");
				  	menuIndex++;
				  	offset++;
				  	tft.fillScreen(altColor);
				  	for (uint8_t i=0; i<tft.height()/FONT_SIZE; i++) {
				  		tft.drawString(playlists[menuIndex-(tft.height()/FONT_SIZE-1)+i].playlistName, 3, FONT_SIZE*i, FONT_TYPE);
				  	}
				  	tft.fillRect(0, tft.height()-FONT_SIZE, 480, FONT_SIZE, mainColor);
				  	tft.setTextColor(altColor);
				  	tft.drawString(playlists[menuIndex].playlistName, 3, tft.height()-FONT_SIZE, FONT_TYPE);
				  	tft.setTextColor(mainColor);
				  }
        } else {
				  if (menuIndex > 0 && menuIndex == offset) {
            // scrolling up going offscreen (positive offset at the top of the screen)
				  	menuIndex--;
				  	offset--;
				  	Serial.println("redrawing");
				  	Serial.println(menuIndex);
				  	tft.fillScreen(altColor);
				  	for (uint8_t i=0; i<tft.height()/FONT_SIZE; i++) {
				  		tft.drawString(playlists[menuIndex+i].playlistName, 3, FONT_SIZE*i, FONT_TYPE);
				  	}
				  	tft.fillRect(0, 0, 480, FONT_SIZE, mainColor);
				  	tft.setTextColor(altColor);
				  	tft.drawString(playlists[menuIndex].playlistName, 3, 0, FONT_TYPE);
				  	tft.setTextColor(mainColor);
				  } else if (menuIndex > 0) {
            // scrolling up without goind offscreen (not necessarly positive offset but not at the top of the screen)
				  	tft.fillRect(0, (menuIndex-offset)*FONT_SIZE, 480, FONT_SIZE, altColor);
				  	tft.drawString(playlists[menuIndex].playlistName, 3, FONT_SIZE*(menuIndex---offset), FONT_TYPE);
				  	tft.fillRect(0, (menuIndex-offset)*FONT_SIZE, 480, FONT_SIZE, mainColor);
				  	tft.setTextColor(altColor);
				  	tft.drawString(playlists[menuIndex].playlistName, 3, FONT_SIZE*(menuIndex-offset), FONT_TYPE);
				  	tft.setTextColor(mainColor);
				  }
        }
			break;
			case SONGS:
        if (direction == DIRECTION_CW) {
				  Serial.println(menuIndex);
				  if (menuIndex<maxSongs && tft.readPixel(479, 315) != TFT_BLACK) {
            // scrolling down without going offscreen (not the end of the screen)
				  	tft.fillRect(0, FONT_SIZE*(menuIndex-offset), 480, FONT_SIZE, altColor);
				  	tft.drawString(songs[menuIndex].trackName, 3, FONT_SIZE*(menuIndex-offset), FONT_TYPE);
				  	tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName)-3, FONT_SIZE*(menuIndex++-offset), FONT_TYPE);
				  	tft.fillRect(0, menuIndex*FONT_SIZE-FONT_SIZE*offset, 480, FONT_SIZE, mainColor);
				  	tft.setTextColor(altColor);
				  	tft.drawString(songs[menuIndex].trackName, 3, FONT_SIZE*(menuIndex-offset), FONT_TYPE);
				  	tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName)-3, FONT_SIZE*(menuIndex-offset), FONT_TYPE);
				  	tft.setTextColor(mainColor);
				  } else if (menuIndex<maxSongs) {
            // scrolling down going offscreen (moves the entire page, end of screen)
				    Serial.print("scrolling down ");
				  	menuIndex++;
				  	offset++;
				  	tft.fillScreen(altColor);
				  	for (uint8_t i=0; i<tft.height()/FONT_SIZE; i++) {
				  		tft.drawString(songs[menuIndex-(tft.height()/FONT_SIZE-1)+i].trackName, 3, FONT_SIZE*i, FONT_TYPE);
				  		tft.drawString(songs[menuIndex-(tft.height()/FONT_SIZE-1)+i].artistName, tft.width()-tft.textWidth(songs[menuIndex-(tft.height()/FONT_SIZE-1)+i].artistName)-3, FONT_SIZE*i, FONT_TYPE);
				  	}
				  	tft.fillRect(0, tft.height()-FONT_SIZE, 480, FONT_SIZE, mainColor);
				  	tft.setTextColor(altColor);
				  	tft.drawString(songs[menuIndex].trackName, 3, tft.height()-FONT_SIZE, FONT_TYPE);
				  	tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName)-3, tft.height()-FONT_SIZE, FONT_TYPE);
				  	tft.setTextColor(mainColor);
				  }
        } else {
				  Serial.print("scrolling up");
				  Serial.println(menuIndex);
				  if (menuIndex > 0 && menuIndex == offset) {
            // scrolling up going offscreen (positive offset at the top of the screen)
				  	menuIndex--;
				  	offset--;
				  	tft.fillScreen(altColor);
				  	for (uint8_t i=0; i<tft.height()/FONT_SIZE; i++) {
				  		tft.drawString(songs[menuIndex+i].trackName, 3, FONT_SIZE*i, FONT_TYPE);
				  		tft.drawString(songs[menuIndex+i].artistName, tft.width()-tft.textWidth(songs[menuIndex+i].artistName)-3, FONT_SIZE*i, FONT_TYPE);
				  	}
				  	tft.fillRect(0, 0, 480, FONT_SIZE, mainColor);
				  	tft.setTextColor(altColor);
				  	tft.drawString(songs[menuIndex].trackName, 3, 0, FONT_TYPE);
				  	tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName)-3, 0, FONT_TYPE);
				  	tft.setTextColor(mainColor);
				  } else if (menuIndex > 0) {
            // scrolling up without goind offscreen (not necessarly positive offset but not at the top of the screen)
				  	tft.fillRect(0, (menuIndex-offset)*FONT_SIZE, 480, FONT_SIZE, altColor);
				  	tft.drawString(songs[menuIndex].trackName, 3, FONT_SIZE*(menuIndex-offset), FONT_TYPE);
				  	tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName)-3, FONT_SIZE*(menuIndex---offset), FONT_TYPE);
				  	tft.fillRect(0, (menuIndex-offset)*FONT_SIZE, 480, FONT_SIZE, mainColor);
				  	tft.setTextColor(altColor);
				  	tft.drawString(songs[menuIndex].trackName, 3, FONT_SIZE*(menuIndex-offset), FONT_TYPE);
				  	tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName)-3, FONT_SIZE*(menuIndex-offset), FONT_TYPE);
				  	tft.setTextColor(mainColor);
				  }
        }
			break;
		}
	}

	if ((millis() > requestDueTime || action) && screen == PLAYING) {
		Serial.print("Free Heap: ");
		Serial.println(ESP.getFreeHeap());
		action = false;

		Serial.println("getting currently playing song:");
		int status = spotify.getCurrentlyPlaying(printCurrentlyPlayingToSerial, SPOTIFY_MARKET);

		if (status == 200) {
			Serial.println("Successfully got currently playing");
			spotify.getPlayerDetails(showVolume, SPOTIFY_MARKET);
			//tft.fillCircle(315, 5, 3, TFT_GREEN);
			digitalWrite(16, 1);
			digitalWrite(17, 0);
		} else if (status == 204) {
			Serial.println("Doesn't seem to be anything playing");
			//tft.fillCircle(315, 5, 3, TFT_GREEN);
			digitalWrite(16, 1);
			digitalWrite(17, 0);
		} else {
			//tft.fillCircle(315, 5, 3, TFT_RED);
			digitalWrite(17, 1);
			digitalWrite(16, 0);
			Serial.print("Error: ");
			Serial.println(status);
		}
		drawBmp("/previous-track.bmp", PREV_X, 260);
		drawBmp("/next-track.bmp", NEXT_X, 260);
		if (isPlaying)
			drawBmp("/circle-pause-regular.bmp", PLAY_X, 260);
		else
			drawBmp("/circle-play-regular.bmp", PLAY_X, 260);
		requestDueTime = millis() + delayBetweenRequests;
	}

	if (millis() > updateTime && screen == PLAYING) {
		Serial.println();
		Serial.print("pixel per second: ");
		Serial.println(pixelPerSecond);
		Serial.print("pixel past: ");
		Serial.print(pixelPast);
		if (pixelPast >= 200) {
			pixelPerSecond = 0;
		}
		pixelPast += pixelPerSecond;
		secondsPast += isPlaying;

		if (secondsPast >= durationSec) {
			action = true;
			secondsPast = 0;
		}

		String secondsElapsed = String((int)secondsPast%60);
		Serial.println();
		String totTime = String((int)secondsPast/60) + ":" + (secondsElapsed.toInt()<10 ? "0"+secondsElapsed : secondsElapsed);

		tft.fillRect(tft.width()/2-142, BAR_HEIGHT-6, 41, 18, altColor);
		tft.drawString(totTime, tft.width()/2-142, BAR_HEIGHT-6, FONT_TYPE);
		tft.fillRect(tft.width()/2-100, BAR_HEIGHT, (int)(pixelPast), 13, mainColor);

		updateTime = millis() + 1000;

	}	
}
