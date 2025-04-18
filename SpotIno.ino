
// ----------------------------
// Standard Libraries
// ----------------------------

#include <TJpg_Decoder.h>
// Include SPIFFS
#define FS_NO_GLOBALS
#include <FS.h>
#include "SPIFFS.h"
#include <HTTPClient.h>
#include <WiFi.h>

// Load tabs attached to this sketch
#include "List_SPIFFS.h"
#include "Web_Fetch.h"

// Include the TFT library https://github.com/Bodmer/TFT_eSPI
#include <TFT_eSPI.h>              // Hardware-specific library
#include <SPI.h>
#include "sf_pro.h"

#include "secrets.h"

TFT_eSPI tft = TFT_eSPI();

#include <WiFiClientSecure.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <SpotifyArduino.h>
// Library for connecting to the Spotify API

// Install from Github
// https://github.com/witnessmenow/spotify-api-arduino

// including a "spotify_server_cert" variable
// header is included as part of the SpotifyArduino libary
#include <SpotifyArduinoCert.h>

#include <ArduinoJson.h>
// Library used for parsing Json from the API responses

// Search for "Arduino Json" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

IPAddress ip(192, 168, 0, 135);

#define LIGHT_THEME
//------- ---------------------- ------

WiFiClientSecure client;
SpotifyArduino spotify(client, CLIENT_ID, CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);

unsigned long delayBetweenRequests = 20000; // Time between requests (1 minute)
unsigned long requestDueTime;               //time when request due
unsigned long updateTime;
float pixelPerSecond;
float pixelPast;
int secondsPast;
int durationSec;
bool isPlaying;
uint8_t menuIndex = 0;
uint8_t offset = 0;
uint8_t maxPlaylists;
uint8_t maxSongs;
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

// buttons
#define PREV 27
#define PLAY 17
#define NEXT 22

//tft.width()/2-83
#define PREV_X 157
//tft.width()/2+35
#define NEXT_X 275
//tft.width()/2-24
#define PLAY_X 216

// encoder
#define CLK_ENC 26
#define DT_ENC 18
#define SW_ENC 19
#define DIRECTION_CW  0   // clockwise direction
#define DIRECTION_CCW 1  // counter-clockwise direction
int direction = DIRECTION_CW;
int CLK_state;
int prev_CLK_state;
byte lastSw = LOW;
byte currSw;

bool isPaused = false;
bool action = false;
int currentPlay;
int prevVolume;
int imageDelay;
UserPlaylistsResult playlists[25] = {NULL};
PlaylistResult songs[50] = {NULL};

#ifdef LIGHT_THEME
uint16_t mainColor = TFT_WHITE;
uint16_t altColor = TFT_BLACK;
#elif defined(DARK_THEME)
uint16_t mainColor = TFT_BLACK;
uint16_t altColor = TFT_WHITE;
#endif

String song;
bool changedSong = false;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);  // Blocking, so only returns when image block is drawn

  // Return 1 to decode next block
  return 1;
}


void setup() {
	
	//tft.setTextColor(altColor, mainColor);
    Serial.begin(115200);
	Serial.setTimeout(15000);


	if (!SPIFFS.begin(true)) {
		Serial.println("SPIFFS initialisation failed!");
		while (1) yield(); // Stay here twiddling thumbs waiting
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

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
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
    // ... or don't!
    //client.setInsecure();

    // If you want to enable some extra debugging
    // uncomment the "#define SPOTIFY_DEBUG" in SpotifyArduino.h

    Serial.println("Refreshing Access Tokens");
    if (!spotify.refreshAccessToken())
    {
        Serial.println("Failed to get access tokens");
    }

	// buttons setup
	//pinMode(PREV, INPUT_PULLUP);
	//pinMode(PLAY, INPUT_PULLUP);
	//pinMode(NEXT, INPUT_PULLUP);

	// encoder setup
	pinMode(CLK_ENC, INPUT);
	pinMode(DT_ENC, INPUT);
	pinMode(SW_ENC, INPUT_PULLUP);

	// read the initial state of the rotary encoder's CLK pin
	prev_CLK_state = digitalRead(CLK_ENC);

	screen = PLAYING;
	// Initialise the TFT
	tft.begin();
	tft.setRotation(3);
	tft.setFreeFont(&SFPRODISPLAYREGULAR10pt8b);
	//SPIFFS.remove("/circle-pause-regular.bmp");
	//SPIFFS.remove("/circle-play-regular.bmp");
	//SPIFFS.remove("/next-track.bmp");

	// initialise touch
	uint16_t calData[5] = { 279, 3550, 313, 3452, 3 };
	tft.setTouch(calData);

	//led
	pinMode(4, OUTPUT);
	pinMode(17, OUTPUT);
	pinMode(16, OUTPUT);
	digitalWrite(4, 0);
	digitalWrite(16, 0);
	digitalWrite(17, 0);
}

void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying) {
    // Use the details in this method or if you want to store them
    // make sure you copy them (using something like strncpy)
    // const char* artist =

    Serial.println("--------- Currently Playing ---------");

    Serial.print("Is Playing: ");
    if (currentlyPlaying.isPlaying)
    {
        Serial.println("Yes");
    }
    else
    {
        Serial.println("No");
    }

    Serial.print("Track: ");
    Serial.println(currentlyPlaying.trackName);

	int blocksElapsed = 20*currentlyPlaying.progressMs/currentlyPlaying.durationMs;

	String artists;

    Serial.println("Artists: ");
    for (int i = 0; i < currentlyPlaying.numArtists; i++) {
		artists += String(currentlyPlaying.artists[i].artistName) + String(", ");

        Serial.print("Name: ");
        Serial.println(currentlyPlaying.artists[i].artistName);
        Serial.println();
    }
	if (artists.length() > 40) {
		artists = artists.substring(0,37);
		artists += String("...");
	} else {
		artists = artists.substring(0, artists.lastIndexOf(","));
	}

    Serial.print("Album: ");
    Serial.println(currentlyPlaying.albumName);
    Serial.println();

	if (SPIFFS.exists("/M81.jpg") == true) {
		SPIFFS.remove("/M81.jpg");
	}

	if (song != currentlyPlaying.trackName) {
		song = currentlyPlaying.trackName;
		changedSong = true;
	}

	if (secondsPast*1000 > currentlyPlaying.progressMs) {
		tft.fillRect(tft.width()/2-99, BAR_HEIGHT+1, (int)(pixelPast)-1, 11, mainColor);
	}
	pixelPast = (200*((float)currentlyPlaying.progressMs/1000))/((float)currentlyPlaying.durationMs/1000);
	secondsPast = currentlyPlaying.progressMs/1000;
	isPlaying = currentlyPlaying.isPlaying;
	if (!currentlyPlaying.isPlaying) {
		pixelPerSecond = 0.0;
	} else {
		pixelPerSecond = 200/((float)currentlyPlaying.durationMs/1000);
	}

	if (changedSong == true) {
		changedSong = false;
		prevVolume = 101;

		String displaySong = song;
		if (song.indexOf("(") != -1) {
			displaySong = song.substring(0, song.indexOf("(")-1);
		}

		tft.fillScreen(mainColor);
		tft.setTextColor(altColor, mainColor);

		uint32_t t = millis();

		// Fetch the jpg file from the specified URL, examples only, from imgur
		bool loaded_ok = getFile(currentlyPlaying.albumImages[1].url, "/M81.jpg"); // Note name preceded with "/"
		//bool loaded_ok = getFile("https://i.imgur.com/OnW2qOO.jpg", "/F35.jpg");

		t = millis() - t;
		imageDelay = t;
		if (loaded_ok) { Serial.print(t); Serial.println(" ms to download"); }

		// List files stored in SPIFFS, should have the file now
		listSPIFFS();

		t = millis();

		// Now draw the SPIFFS file
		TJpgDec.drawFsJpg(tft.width()/2-80, 20, "/M81.jpg");
		//TJpgDec.drawFsJpg(0, 0, "/F35.jpg");

		t = millis() - t;
		imageDelay+=t;
		Serial.print(t); Serial.println(" ms to draw to TFT");

		Serial.print("image delay: ");
		Serial.println(imageDelay);
		tft.drawCentreString(displaySong, tft.width()/2, 175, FONT_TYPE);
		tft.drawCentreString(artists, tft.width()/2, 200, FONT_TYPE);
		tft.drawRect(tft.width()/2-100, BAR_HEIGHT, 200, 13, altColor);
		String seconds = String((currentlyPlaying.durationMs+imageDelay)/1000%60);
		durationSec = currentlyPlaying.durationMs/1000;
		String totTime = String((currentlyPlaying.durationMs+imageDelay)/1000/60) + ":" + (seconds.toInt()<10 ? "0"+seconds : seconds);
		tft.drawString(totTime, tft.width()/2+103, BAR_HEIGHT-6, FONT_TYPE);
		Serial.println("assigned pixel per second: ");
		Serial.print(pixelPerSecond);
		// Time recorded for test purposes

		
	}
}

bool getPlaylistsCallback(UserPlaylistsResult result, int index, int numPlaylists) {

	Serial.println("--------- Song Details ---------");

	Serial.print("Playlist Index: ");
	Serial.println(index);

	Serial.print("Playlist ID: ");
	Serial.println(result.playlistId);

	Serial.print("Playlist Name: ");
	Serial.println(result.playlistName);

	if (index == 0) {
		tft.setTextColor(mainColor);
		tft.fillRect(0, 0, 255, 16, altColor);
	} else if (index == 1) tft.setTextColor(altColor);

	maxPlaylists = index;

	tft.drawString(result.playlistName, 3, 16*index, FONT_TYPE);
	//tft.drawString(result.artistName, tft.width()-tft.textWidth(result.artistName, 2)-3, 16*index, FONT_TYPE);

  //Serial.print("Song Album: ");
  //Serial.println(result.albumName);

  //Serial.print("Song Images (url): ");
  //for (int i = 0; i < result.numImages; i++){
  //  Serial.println(result.albumImages[i].url);
  //}

  Serial.println("-------------------------------");

  return true;
}

bool getResultsCallback(PlaylistResult result, int index, int numResults) {

	Serial.println("--------- Song Details ---------");

	Serial.print("Song Index: ");
	Serial.println(index);

	Serial.print("Song ID: ");
	Serial.println(result.trackUri);

	Serial.print("Song Name: ");
	Serial.println(result.trackName);

	Serial.print("Song Artists: ");
	Serial.println(result.artistName);
	//for (int i = 0; i < result.numArtists; i++){
	//	Serial.println(result.artists[i].artistName);
	//}

	//tft.setCursor(3, 16*index);
	//tft.print(result.trackName);
	//tft.setCursor(tft.width()-(strlen(result.artists[0].artistName)*6), 16*index);
	//tft.print(result.artists[0].artistName);

	if (index == 0) {
		tft.setTextColor(mainColor);
		tft.fillRect(0, 0, 320, 16, altColor);
	} else if (index == 1) tft.setTextColor(altColor);



	maxSongs = index;
	tft.drawString(result.trackName, 3, 16*index, FONT_TYPE);
	tft.drawString(result.artistName, tft.width()-tft.textWidth(result.artistName, 2)-3, 16*index, FONT_TYPE);

  //Serial.print("Song Album: ");
  //Serial.println(result.albumName);

  //Serial.print("Song Images (url): ");
  //for (int i = 0; i < result.numImages; i++){
  //  Serial.println(result.albumImages[i].url);
  //}

  Serial.println("-------------------------------");

  return true;
}

void showVolume(int volume) {
	tft.fillRect(tft.width()/2+77, 30, 10, 130, altColor);
	tft.fillRect(tft.width()/2+89, 10, 20, 170, mainColor);
	tft.drawRect(tft.width()/2+77, 30, 10, 130, altColor);
	tft.fillRect(tft.width()/2+78, 31, 8, (100-volume)*1.3, mainColor);
	//tft.fillRect(245, 20, 10, 130, altColor);
	//tft.fillRect(257, 0, 20, 170, mainColor);
	//tft.drawRect(245, 20, 10, 130, altColor);
	//tft.fillRect(246, 21, 8, (100-volume)*1.3, mainColor);
	tft.drawNumber(volume, tft.width()/2+88, ((100-volume)*1.3)+28);
}

void showVolume(PlayerDetails playerDetails) {
	if (prevVolume != playerDetails.device.volumePercent) {
		prevVolume = playerDetails.device.volumePercent;
		tft.fillRect(tft.width()/2+77, 30, 10, 130, altColor);
		tft.fillRect(tft.width()/2+89, 10, 20, 170, mainColor);
		tft.drawRect(tft.width()/2+77, 30, 10, 130, altColor);
		tft.fillRect(tft.width()/2+78, 31, 8, (100-playerDetails.device.volumePercent)*1.3, mainColor);
		/*tft.fillRect(245, 20, 10, 130, altColor);
		tft.fillRect(257, 0, 20, 170, mainColor);
		tft.drawRect(245, 20, 10, 130, altColor);
		tft.fillRect(246, 21, 8, (100-playerDetails.device.volumePercent)*1.3, mainColor);*/
		tft.drawNumber(playerDetails.device.volumePercent, tft.width()/2+88, ((100-playerDetails.device.volumePercent)*1.3)+28);
	}
}



void loop() {
	// current state of encoder
	CLK_state = digitalRead(CLK_ENC);
	currSw = digitalRead(SW_ENC);
	// save the last state
	//byte currPlay = digitalRead(PLAY);
	//byte currPrev = digitalRead(PREV);
	//byte currNext = digitalRead(NEXT);
	//byte currNext = HIGH;
	/*if(currPlay == LOW) {
		if (isPlaying) spotify.pause();
		else spotify.play();
		isPlaying = !isPlaying;
		action = true;
		Serial.println("PLAY/PAUSE");
	}*/

	//if(currPrev == LOW) {
	//	spotify.previousTrack();
	//	action = true;
	//	Serial.println("PREV");
	//}

	/*if(currNext == LOW) {
		spotify.nextTrack();
		action = true;
		Serial.println("NEXT");
	}*/

	uint16_t t_x = 0, t_y = 0;
	bool pressed = tft.getTouch(&t_x, &t_y);
	
	if (pressed) {
		// clicked next
		if (t_x>=PREV_X && t_x<=PREV_X+48 && 260<=t_y<=308) {
			drawBmp("/next-track-reverse.bmp", NEXT_X, 260);
			spotify.nextTrack();
			action = true;
			Serial.println("NEXT");
		}

		// clicked prev
		if (t_x>=NEXT_X && t_x<=NEXT_X+48 && 260<=t_y<=308) {
			drawBmp("/previous-track-reverse.bmp", PREV_X, 260);
			spotify.previousTrack();
			action = true;
			Serial.println("PREV");
		}

		// clicked next
		if (t_x>=PLAY_X && t_x<=PLAY_X+48 && 260<=t_y<=308) {
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
	}

	if(lastSw == HIGH && currSw == LOW) {
		Serial.println("SW");
		int status;
		switch (screen) {
			case PLAYING:
				screen = MENU;
				tft.fillScreen(mainColor);
				Serial.println("MENU");
				menuIndex = 0;
				offset = 0;
				do {
					status = spotify.getUserPlaylists(25, getPlaylistsCallback, playlists);
				} while (status != 200);
				break;
			case MENU:
				screen = SONGS;
				tft.fillScreen(mainColor);
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
				tft.fillScreen(mainColor);
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
		
	}
	lastSw = currSw;

	if (CLK_state != prev_CLK_state && CLK_state == HIGH) {
		if (digitalRead(DT_ENC) == HIGH) {
			direction = DIRECTION_CW;
		}
		else direction = DIRECTION_CCW;
		
		if (direction == DIRECTION_CW) {
			switch (screen) {
				case PLAYING:
					prevVolume = min(prevVolume+7, 100);
					spotify.setVolume(prevVolume);
					showVolume(prevVolume);
					break;
				case MENU:
					if (menuIndex<maxPlaylists && tft.readPixel(0, 225) != TFT_BLACK) {
						tft.fillRect(0, 16*(menuIndex-offset), 320, 16, mainColor);
						tft.drawString(playlists[menuIndex].playlistName, 3, 16*(menuIndex++-offset), FONT_TYPE);
						tft.fillRect(0, menuIndex*16-16*offset, 320, 16, altColor);
						tft.setTextColor(mainColor);
						tft.drawString(playlists[menuIndex].playlistName, 3, 16*(menuIndex-offset), FONT_TYPE);
						tft.setTextColor(altColor);
					} else if (menuIndex<maxPlaylists) {
						Serial.println("scrolling down");
						menuIndex++;
						offset++;
						tft.fillScreen(mainColor);
						for (uint8_t i=0; i<15; i++) {
							tft.drawString(playlists[menuIndex-14+i].playlistName, 3, 16*i, FONT_TYPE);
						}
						tft.fillRect(0, 224, 320, 16, altColor);
						tft.setTextColor(mainColor);
						tft.drawString(playlists[menuIndex].playlistName, 3, 224, FONT_TYPE);
						tft.setTextColor(altColor);
					}
					break;
				case SONGS:
					Serial.print("scrolling down ");
					Serial.println(menuIndex);
					//for (int i=0; i<50; i++) {
					//	Serial.println(songs[i].artistName);
					//}
					if (menuIndex<maxSongs && tft.readPixel(0, 225) != TFT_BLACK) {
						tft.fillRect(0, 16*(menuIndex-offset), 320, 16, mainColor);
						tft.drawString(songs[menuIndex].trackName, 3, 16*(menuIndex-offset), FONT_TYPE);
						tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName, 2)-3, 16*(menuIndex++-offset), FONT_TYPE);
						tft.fillRect(0, menuIndex*16-16*offset, 320, 16, altColor);
						tft.setTextColor(mainColor);
						tft.drawString(songs[menuIndex].trackName, 3, 16*(menuIndex-offset), FONT_TYPE);
						tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName, 2)-3, 16*(menuIndex-offset), FONT_TYPE);
						tft.setTextColor(altColor);
					} else if (menuIndex<maxSongs) {
						menuIndex++;
						offset++;
						tft.fillScreen(mainColor);
						for (uint8_t i=0; i<15; i++) {
							tft.drawString(songs[menuIndex-14+i].trackName, 3, 16*i, FONT_TYPE);
							tft.drawString(songs[menuIndex-14+i].artistName, tft.width()-tft.textWidth(songs[menuIndex-14+i].artistName, 2)-3, 16*i, FONT_TYPE);
						}
						tft.fillRect(0, 224, 320, 16, altColor);
						tft.setTextColor(mainColor);
						tft.drawString(songs[menuIndex].trackName, 3, 224, FONT_TYPE);
						tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName, 2)-3, 224, FONT_TYPE);
						tft.setTextColor(altColor);
					}
					break;
			}
		}
		else {
			switch (screen) {
				case PLAYING:
					prevVolume = max(prevVolume-7, 0);
					spotify.setVolume(prevVolume);
					showVolume(prevVolume);
					break;
				case MENU:
					if (menuIndex > 0 && menuIndex == offset) {
						menuIndex--;
						offset--;
						Serial.println("redrawing");
						Serial.println(menuIndex);
						tft.fillScreen(mainColor);
						for (uint8_t i=0; i<15; i++) {
							tft.drawString(playlists[menuIndex+i].playlistName, 3, 16*i, FONT_TYPE);
						}
						tft.fillRect(0, 0, 320, 16, altColor);
						tft.setTextColor(mainColor);
						tft.drawString(playlists[menuIndex].playlistName, 3, 0, FONT_TYPE);
						tft.setTextColor(altColor);
					} else if (menuIndex > 0) {
						tft.fillRect(0, (menuIndex-offset)*16, 320, 16, mainColor);
						tft.drawString(playlists[menuIndex].playlistName, 3, 16*(menuIndex---offset), FONT_TYPE);
						tft.fillRect(0, (menuIndex-offset)*16, 320, 16, altColor);
						tft.setTextColor(mainColor);
						tft.drawString(playlists[menuIndex].playlistName, 3, 16*(menuIndex-offset), FONT_TYPE);
						tft.setTextColor(altColor);
					}
					break;
				case SONGS:
					Serial.print("scrolling up");
					Serial.println(menuIndex);
					//for (int i=0; i<50; i++) {
					//	Serial.println(songs[i].artistName);
					//}
					if (menuIndex > 0 && menuIndex == offset) {
						menuIndex--;
						offset--;
						tft.fillScreen(mainColor);
						for (uint8_t i=0; i<15; i++) {
							tft.drawString(songs[menuIndex+i].trackName, 3, 16*i, FONT_TYPE);
							tft.drawString(songs[menuIndex+i].artistName, tft.width()-tft.textWidth(songs[menuIndex+i].artistName, 2)-3, 16*i, FONT_TYPE);
						}
						tft.fillRect(0, 0, 320, 16, altColor);
						tft.setTextColor(mainColor);
						tft.drawString(songs[menuIndex].trackName, 3, 0, FONT_TYPE);
						tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName, 2)-3, 0, FONT_TYPE);
						tft.setTextColor(altColor);
					} else if (menuIndex > 0) {
						tft.fillRect(0, (menuIndex-offset)*16, 320, 16, mainColor);
						tft.drawString(songs[menuIndex].trackName, 3, 16*(menuIndex-offset), FONT_TYPE);
						tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName, 2)-3, 16*(menuIndex---offset), FONT_TYPE);
						tft.fillRect(0, (menuIndex-offset)*16, 320, 16, altColor);
						tft.setTextColor(mainColor);
						tft.drawString(songs[menuIndex].trackName, 3, 16*(menuIndex-offset), FONT_TYPE);
						tft.drawString(songs[menuIndex].artistName, tft.width()-tft.textWidth(songs[menuIndex].artistName, 2)-3, 16*(menuIndex-offset), FONT_TYPE);
						tft.setTextColor(altColor);
					}
					break;

			}
		}

	}

	prev_CLK_state = CLK_state;

	if ((millis() > requestDueTime || action) && screen == PLAYING) {
        Serial.print("Free Heap: ");
        Serial.println(ESP.getFreeHeap());
		action = false;

        Serial.println("getting currently playing song:");
		// Market can be excluded if you want e.g. spotify.getCurrentlyPlaying()
		int status = spotify.getCurrentlyPlaying(printCurrentlyPlayingToSerial, SPOTIFY_MARKET);
		spotify.getPlayerDetails(showVolume, SPOTIFY_MARKET);

        if (status == 200)
        {
			Serial.println("Successfully got currently playing");
			//tft.fillCircle(315, 5, 3, TFT_GREEN);
			digitalWrite(16, 1);
			digitalWrite(4, 0);
        }
        else if (status == 204)
        {
            Serial.println("Doesn't seem to be anything playing");
			//tft.fillCircle(315, 5, 3, TFT_GREEN);
			digitalWrite(16, 1);
			digitalWrite(4, 0);
        }
        else
        {
			//tft.fillCircle(315, 5, 3, TFT_RED);
			digitalWrite(4, 1);
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

		tft.fillRect(tft.width()/2-142, BAR_HEIGHT-6, 41, 18, mainColor);
		tft.drawString(totTime, tft.width()/2-142, BAR_HEIGHT-6, FONT_TYPE);
		tft.fillRect(tft.width()/2-100, BAR_HEIGHT, (int)(pixelPast), 13, altColor);

		updateTime = millis() + 1000;

	}	
}
